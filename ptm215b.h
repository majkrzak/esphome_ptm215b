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
  using address_t = std::array<uint8_t, 6>;
  using key_t = std::array<uint8_t, 16>;
  using manufacturer_t = esp32_ble_tracker::ESPBTUUID;
  using data_t = std::vector<uint8_t>;
  using sequence_counter_t = uint32_t;
  using security_signature_t = std::array<uint8_t, 4>;
  using switch_status_t = struct __packed {
    bool press : 1;
    bool A0 : 1;
    bool A1 : 1;
    bool B0 : 1;
    bool B1 : 1;
  };
  using data_telegram_t = struct __packed {
    sequence_counter_t sequence_counter;
    switch_status_t switch_status;
    security_signature_t security_signature;
  };
  using commissioning_telegram_t = struct __packed {
    sequence_counter_t sequence_counter;
    key_t security_key;
    address_t static_source_address;
  };

  virtual ~PTM215B(){};

  void set_address(const address_t &&address) { this->address_ = address; }
  void set_key(const key_t &&key) { this->key_ = key; }
  void set_bar_sensor(binary_sensor::BinarySensor *sensor) { this->bar_sensor_ = sensor; }
  void set_a0_sensor(binary_sensor::BinarySensor *sensor) { this->a0_sensor_ = sensor; }
  void set_a1_sensor(binary_sensor::BinarySensor *sensor) { this->a1_sensor_ = sensor; }
  void set_b0_sensor(binary_sensor::BinarySensor *sensor) { this->b0_sensor_ = sensor; }
  void set_b1_sensor(binary_sensor::BinarySensor *sensor) { this->b1_sensor_ = sensor; }

  bool parse_device(const esp32_ble_tracker::ESPBTDevice &device) override;

  switch_status_t get_state() const { return this->switch_status_; };

 protected:
  address_t address_{};
  key_t key_{};

  binary_sensor::BinarySensor *bar_sensor_{nullptr};
  binary_sensor::BinarySensor *a0_sensor_{nullptr};
  binary_sensor::BinarySensor *a1_sensor_{nullptr};
  binary_sensor::BinarySensor *b0_sensor_{nullptr};
  binary_sensor::BinarySensor *b1_sensor_{nullptr};

 private:
  switch_status_t switch_status_{};
  sequence_counter_t sequence_counter_{};

  bool check_address(const address_t &address);
  bool check_manufacturer(const manufacturer_t &manufacturer);
  bool handle_data(const data_t &data);
  optional<data_telegram_t> parse_data_telegram(const data_t &data);
  optional<commissioning_telegram_t> parse_commissioning_telegram(const data_t &data);
  bool handle_data_telegram(const data_telegram_t &data_telegram);
  bool handle_commissioning_telegram(const commissioning_telegram_t &commissioning_telegram);
  bool check_debounce(const sequence_counter_t &sequence_counter);
  bool check_replay(const sequence_counter_t &sequence_counter);
  bool check_signature(const data_telegram_t &data_telegram);
  void update_sequence_counter(const sequence_counter_t &sequence_counter);
  void update_switch_status(const switch_status_t &switch_status);
  void notify();
};

}  // namespace ptm215b
}  // namespace esphome

#endif
