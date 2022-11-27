#ifdef USE_ESP32

#include "ptm215b.h"
#include "esphome/core/log.h"

#include <sstream>
#include "mbedtls/ccm.h"

namespace esphome {
namespace ptm215b {

struct switch_status {
  bool press : 1;
  bool A0 : 1;
  bool A1 : 1;
  bool B0 : 1;
  bool B1 : 1;

  std::string to_string() const {
    std::stringstream ss;
    ss << (press ? "Press" : "Release");
    ss << (A0 ? " A0" : "");
    ss << (A1 ? " A1" : "");
    ss << (B0 ? " B0" : "");
    ss << (B1 ? " B1" : "");
    return ss.str();
  }
};

union {
  struct __packed {
    uint32_t sequence_counter;
    struct switch_status switch_status;
    uint32_t security_signature;

    std::string to_string() const {
      std::stringstream ss;
      ss << "Data Telegram: ";
      ss << "sequence_counter: " << sequence_counter << " ";
      ss << "switch_status: " << switch_status.to_string() << "";
      return ss.str();
    }
  } f;
  std::array<uint8_t, 9> b;
} data_telegram;

static const char *const TAG = "ptm215b";
static const esp32_ble_tracker::ESPBTUUID manufacturer_id = esp32_ble_tracker::ESPBTUUID::from_uint16(0x03DA);

bool PTM215B::parse_device(const esp32_ble_tracker::ESPBTDevice &device) {
  if (device.address_uint64() != this->address_) {
    ESP_LOGVV(TAG, "parse_device(): unknown MAC address.");
    return false;
  }
  ESP_LOGVV(TAG, "parse_device(): MAC address %s found.", device.address_str().c_str());

  for (auto &manufacturer_data : device.get_manufacturer_datas()) {
    if (manufacturer_data.uuid != manufacturer_id) {
      ESP_LOGE(TAG, "%s: Unknow Manufacturer ID %s, %s required.", device.address_str().c_str(),
               manufacturer_data.uuid.to_string().c_str(), manufacturer_id.to_string().c_str());
      continue;
    }

    if (manufacturer_data.data.size() == 26) {
      ESP_LOGE(TAG, "%s: Radio-based commissioning in progress. Make `Any Other Button Action` to exit.",
               device.address_str().c_str());
      continue;
    }

    if (manufacturer_data.data.size() != 9) {
      ESP_LOGE(TAG, "%s: Invalid length. Optional data fileds are not supported.", device.address_str().c_str());
      continue;
    }

    ESP_LOGD(TAG, "%s: Manufacturer data: %s %s", device.address_str().c_str(),
             manufacturer_data.uuid.to_string().c_str(), format_hex_pretty(manufacturer_data.data).c_str());

    std::copy_n(manufacturer_data.data.begin(), 9, data_telegram.b.begin());

    ESP_LOGD(TAG, "%s: %s", device.address_str().c_str(), data_telegram.f.to_string().c_str());

    // validate https://siliconlabs.github.io/Gecko_SDK_Doc/mbedtls/html/ccm_8h.html#a464d8e724738b4bbd5b415ca0580f1b1
    {
      {
        address_ = 0xE215000019B8;
        key_ = {0x6c, 0x48, 0x55, 0x07, 0x1a, 0xcd, 0xee, 0x44, 0x86, 0xf3, 0x0a, 0x41, 0xca, 0x20, 0x89, 0xa1};
        data_telegram.b = {0x5D, 0x04, 0x00, 0x00, 0x11, 0xB2, 0xFA, 0x88, 0xFF};
      }
      int ret;
      mbedtls_ccm_context ctx;
      mbedtls_ccm_init(&ctx);

      ret = mbedtls_ccm_setkey(&ctx, MBEDTLS_CIPHER_ID_AES, key_.data(), key_.size() * 8);

      union {
        struct __packed {
          std::array<uint8_t, 6> source_address;
          uint32_t sequence_counter;
          std::array<uint8_t, 3> _padding;
        } fields;
        std::array<uint8_t, 13> buff;
      } nonce{{{static_cast<uint8_t>(address_ >> 0 * 8), static_cast<uint8_t>(address_ >> 1 * 8),
                static_cast<uint8_t>(address_ >> 2 * 8), static_cast<uint8_t>(address_ >> 3 * 8),
                static_cast<uint8_t>(address_ >> 4 * 8), static_cast<uint8_t>(address_ >> 5 * 8)},
               data_telegram.f.sequence_counter,
               {0, 0, 0}}};

      union {
        struct __packed {
          uint8_t len;
          uint8_t type;
          uint16_t manufacturer;
          uint32_t sequence_counter;
          struct switch_status state;
        } fields;
        std::array<uint8_t, 9> buff;
      } payload{{0x0C, 0xFF, 0x03DA, data_telegram.f.sequence_counter, data_telegram.f.switch_status}};

      union {
        struct {
          uint32_t security_signature;
          uint32_t security_signature1;
          uint32_t security_signature2;
          uint32_t security_signature3;
        } fields;
        std::array<uint8_t, 16> buff;
      } tag{{data_telegram.f.security_signature}};

      std::array<uint8_t, 9> buff;

      ESP_LOGD(TAG, "%s: NONCE: %s", device.address_str().c_str(),
               format_hex_pretty(nonce.buff.data(), nonce.buff.size()).c_str());
      ESP_LOGD(TAG, "%s: PAYLOAD: %s", device.address_str().c_str(),
               format_hex_pretty(payload.buff.data(), payload.buff.size()).c_str());

      ret = mbedtls_ccm_encrypt_and_tag(&ctx, buff.size(), nonce.buff.data(), nonce.buff.size(), nullptr, 0,
                                        payload.buff.data(), buff.data(), tag.buff.data(), tag.buff.size());

      ESP_LOGI(TAG, "%s: ENCRYPT: %d TAG: %x ? %x / %x / %x / %x ", device.address_str().c_str(), ret,
               data_telegram.f.security_signature, tag.fields.security_signature, tag.fields.security_signature1,
               tag.fields.security_signature2, tag.fields.security_signature3);

      mbedtls_ccm_free(&ctx);
    }

    if (last_sequence_ == data_telegram.f.sequence_counter) {
      ESP_LOGD(TAG, "%s: Debouncing sequence %d", device.address_str().c_str(), last_sequence_);
      continue;
    } else {
      last_sequence_ = data_telegram.f.sequence_counter;
      ESP_LOGD(TAG, "%s: New sequence %d", device.address_str().c_str(), last_sequence_);
    }

    ESP_LOGI(TAG, "%s: %s", device.address_str().c_str(), data_telegram.f.switch_status.to_string().c_str());

    if (bar_sensor_) {
      bar_sensor_->publish_state(data_telegram.f.switch_status.press);
    }
    if (a0_sensor_) {
      a0_sensor_->publish_state(data_telegram.f.switch_status.A0);
    }
    if (a1_sensor_) {
      a1_sensor_->publish_state(data_telegram.f.switch_status.A1);
    }
    if (b0_sensor_) {
      b0_sensor_->publish_state(data_telegram.f.switch_status.B0);
    }
    if (b1_sensor_) {
      b1_sensor_->publish_state(data_telegram.f.switch_status.B1);
    }

    break;
  }

  return true;
}
}  // namespace ptm215b
}  // namespace esphome

#endif