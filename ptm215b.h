#pragma once

#ifdef USE_ESP32

#include "esphome/core/datatypes.h"
#include "esphome/core/component.h"
#include "esphome/components/esp32_ble_tracker/esp32_ble_tracker.h"
#include "esphome/components/binary_sensor/binary_sensor.h"

namespace esphome {
namespace ptm215b {

class PTM215B : public Component, public esp32_ble_tracker::ESPBTDeviceListener {
 public:
  typedef std::array<uint8_t, 6> address_t;
  typedef std::array<uint8_t, 16> key_t;
  typedef esp32_ble_tracker::ESPBTUUID manufacturer_t;
  typedef std::vector<uint8_t> data_t;
  typedef uint32_le_t sequence_counter_t;
  typedef std::array<uint8_t, 4> security_signature_t;

  typedef struct __packed {
    bool press : 1;
    bool A0 : 1;
    bool A1 : 1;
    bool B0 : 1;
    bool B1 : 1;
  } switch_status_t;

  struct __packed {
    sequence_counter_t sequence_counter;
    switch_status_t switch_status;
    security_signature_t security_signature;
  } data_telegram_t;

 public:
  void set_address(const address_t &&address) { address_ = address; }
  void set_key(const key_t &&key) { key_ = key; }
  void set_bar_sensor(binary_sensor::BinarySensor *sensor) { this->bar_sensor_ = sensor; }
  void set_a0_sensor(binary_sensor::BinarySensor *sensor) { this->a0_sensor_ = sensor; }
  void set_a1_sensor(binary_sensor::BinarySensor *sensor) { this->a1_sensor_ = sensor; }
  void set_b0_sensor(binary_sensor::BinarySensor *sensor) { this->b0_sensor_ = sensor; }
  void set_b1_sensor(binary_sensor::BinarySensor *sensor) { this->b1_sensor_ = sensor; }

 public:
  bool parse_device(const esp32_ble_tracker::ESPBTDevice &device) override;

 public:
  const switch_status_t get_state() const { return state_; };

 protected:
  address_t address_{};
  key_t key_{};

  binary_sensor::BinarySensor *bar_sensor_{nullptr};
  binary_sensor::BinarySensor *a0_sensor_{nullptr};
  binary_sensor::BinarySensor *a1_sensor_{nullptr};
  binary_sensor::BinarySensor *b0_sensor_{nullptr};
  binary_sensor::BinarySensor *b1_sensor_{nullptr};

 private:
  switch_status_t state_{};
  uint32_t last_sequence_{0};

 private:
  bool check_address(const address_t &address);
  bool check_manufacturer(const manufacturer_t &manufacturer);
  bool handle_data(const data_t &data);
  // bool handle_data_telegram();
  // bool handle_commissioning_telegram();
  bool check_signature();
  void update_state(switch_status_t new_state);
};

}  // namespace ptm215b
}  // namespace esphome

#endif
