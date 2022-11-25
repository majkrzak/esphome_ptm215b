#pragma once

#include "esphome/core/component.h"
#include "esphome/components/esp32_ble_tracker/esp32_ble_tracker.h"

#ifdef USE_ESP32

namespace esphome {
namespace ptm215b {

class PTM215B : public Component, public esp32_ble_tracker::ESPBTDeviceListener {
 public:
  void set_address(uint64_t address) { address_ = address; }

  bool parse_device(const esp32_ble_tracker::ESPBTDevice &device) override;

 protected:
  uint64_t address_;

 private:
  uint32_t last_sequence_{0};
};

}  // namespace ptm215b
}  // namespace esphome

#endif