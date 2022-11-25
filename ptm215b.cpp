#include "ptm215b.h"
#include "esphome/core/log.h"

#include <sstream>

#ifdef USE_ESP32

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

struct data_telegram {
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
};

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

    auto data_telegram = *reinterpret_cast<const struct data_telegram *const>(manufacturer_data.data.data());

    ESP_LOGD(TAG, "%s: %s", device.address_str().c_str(), data_telegram.to_string().c_str());

    if (last_sequence_ == data_telegram.sequence_counter) {
      ESP_LOGD(TAG, "%s: Debouncing sequence %d", device.address_str().c_str(), last_sequence_);
      continue;
    } else {
      last_sequence_ = data_telegram.sequence_counter;
      ESP_LOGD(TAG, "%s: New sequence %d", device.address_str().c_str(), last_sequence_);
    }

    ESP_LOGI(TAG, "%s: %s", device.address_str().c_str(), data_telegram.switch_status.to_string().c_str());

    if (bar_sensor_) {
      bar_sensor_->publish_state(data_telegram.switch_status.press);
    }
    if (a0_sensor_) {
      a0_sensor_->publish_state(data_telegram.switch_status.A0);
    }
    if (a1_sensor_) {
      a1_sensor_->publish_state(data_telegram.switch_status.A1);
    }
    if (b0_sensor_) {
      b0_sensor_->publish_state(data_telegram.switch_status.B0);
    }
    if (b1_sensor_) {
      b1_sensor_->publish_state(data_telegram.switch_status.B1);
    }

    break;
  }

  return true;
}
}  // namespace ptm215b
}  // namespace esphome

#endif