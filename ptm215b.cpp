#ifdef USE_ESP32

#include "ptm215b.h"
#include "esphome/core/log.h"

#include <sstream>
#include <iomanip>
#include "mbedtls/ccm.h"

namespace esphome {
namespace ptm215b {

namespace {

template<class T> std::string to_string(const T &bytes) {
  std::stringstream ss;
  for (auto &byte : bytes) {
    ss << std::hex << std::uppercase << std::setfill('0') << std::setw(2);
    ss << static_cast<int>(byte);
    if (&byte != &bytes.back()) {
      ss << ":";
    }
  }
  return ss.str();
}

union {
  struct __packed {
    uint32_t sequence_counter;
    PTM215B::state switch_status;
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
}  // namespace

bool PTM215B::parse_device(const esp32_ble_tracker::ESPBTDevice &device) {
  if (!check_address(*reinterpret_cast<const address_t *>(device.address()))) {
    return false;
  }

  for (auto &manufacturer_data : device.get_manufacturer_datas()) {
    if (!check_manufacturer(manufacturer_data.uuid)) {
      continue;
    }

    return handle_data(manufacturer_data.data);
  }

  return false;
}

bool PTM215B::check_address(const address_t &address) {
  if (address == address_) {
    return true;
  } else {
    return false;
  }
}

bool PTM215B::check_manufacturer(const manufacturer_t &manufacturer) {
  if (manufacturer == esp32_ble_tracker::ESPBTUUID::from_uint16(0x03DA)) {
    return true;
  } else {
    return false;
  }
}

bool PTM215B::handle_data(const data_t &data) {
  if (data.size() == 26) {
    return false;
  }

  if (data.size() != 9) {
    return false;
  }

  std::copy_n(data.begin(), 9, data_telegram.b.begin());

  {
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
    } nonce{{{address_[5], address_[4], address_[3], address_[2], address_[1], address_[0]},
             data_telegram.f.sequence_counter,
             {0, 0, 0}}};

    union {
      struct __packed {
        uint8_t len;
        uint8_t type;
        uint16_t manufacturer;
        uint32_t sequence_counter;
        PTM215B::state state;
      } fields;
      std::array<uint8_t, 9> buff;
    } payload{{0x0C, 0xFF, 0x03DA, data_telegram.f.sequence_counter, data_telegram.f.switch_status}};

    union {
      struct {
        uint32_t security_signature;
      } fields;
      std::array<uint8_t, 4> buff;
    } tag{{data_telegram.f.security_signature}};

    ret = mbedtls_ccm_auth_decrypt(&ctx, 0, nonce.buff.data(), nonce.buff.size(), payload.buff.data(),
                                   payload.buff.size(), nullptr, nullptr, tag.buff.data(), tag.buff.size());

    std::array<uint8_t, 4> buff;
    ret = mbedtls_ccm_encrypt_and_tag(&ctx, 0, nonce.buff.data(), nonce.buff.size(), payload.buff.data(),
                                      payload.buff.size(), nullptr, nullptr, buff.data(), buff.size());

    mbedtls_ccm_free(&ctx);
  }

  if (last_sequence_ == data_telegram.f.sequence_counter) {
    return false;
  } else {
    last_sequence_ = data_telegram.f.sequence_counter;
  }

  update_state(data_telegram.f.switch_status);

  return false;
}

void PTM215B::update_state(state new_state) {
  state_ = new_state;

  ESP_LOGI(TAG, "%s: %s", to_string(address_).c_str(), state_.to_string().c_str());

  if (bar_sensor_) {
    bar_sensor_->publish_state(state_.press);
  }
  if (a0_sensor_) {
    a0_sensor_->publish_state(state_.A0);
  }
  if (a1_sensor_) {
    a1_sensor_->publish_state(state_.A1);
  }
  if (b0_sensor_) {
    b0_sensor_->publish_state(state_.B0);
  }
  if (b1_sensor_) {
    b1_sensor_->publish_state(state_.B1);
  }
}

std::string PTM215B::state::to_string() const {
  std::stringstream ss;
  ss << (press ? "Press" : "Release");
  ss << (A0 ? " A0" : "");
  ss << (A1 ? " A1" : "");
  ss << (B0 ? " B0" : "");
  ss << (B1 ? " B1" : "");
  return ss.str();
}
}  // namespace ptm215b
}  // namespace esphome

#endif
