#pragma once

#ifdef USE_ESP32

#include "esphome/core/component.h"
#include "esphome/components/esp32_ble_tracker/esp32_ble_tracker.h"
#include "esphome/components/binary_sensor/binary_sensor.h"

namespace esphome {
namespace ptm215b {

class PTM215B : public Component, public esp32_ble_tracker::ESPBTDeviceListener {
 public:
  void set_address(uint64_t address) { address_ = address; }
  void set_key(const std::array<uint8_t, 16> &&key) { key_ = key; }
  void set_bar_sensor(binary_sensor::BinarySensor *sensor) { this->bar_sensor_ = sensor; }
  void set_a0_sensor(binary_sensor::BinarySensor *sensor) { this->a0_sensor_ = sensor; }
  void set_a1_sensor(binary_sensor::BinarySensor *sensor) { this->a1_sensor_ = sensor; }
  void set_b0_sensor(binary_sensor::BinarySensor *sensor) { this->b0_sensor_ = sensor; }
  void set_b1_sensor(binary_sensor::BinarySensor *sensor) { this->b1_sensor_ = sensor; }

  bool parse_device(const esp32_ble_tracker::ESPBTDevice &device) override;

 protected:
  uint64_t address_{0};
  std::array<uint8_t, 16> key_{};

  binary_sensor::BinarySensor *bar_sensor_{nullptr};
  binary_sensor::BinarySensor *a0_sensor_{nullptr};
  binary_sensor::BinarySensor *a1_sensor_{nullptr};
  binary_sensor::BinarySensor *b0_sensor_{nullptr};
  binary_sensor::BinarySensor *b1_sensor_{nullptr};

 private:
  uint32_t last_sequence_{0};
};

}  // namespace ptm215b
}  // namespace esphome

#endif