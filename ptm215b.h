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
  template<int N> using array_t = std::array<uint8_t, N>;
  using address_t = array_t<6>;
  using security_key_t = array_t<16>;
  using button_predicate_t = struct {
    optional<bool> a0;
    optional<bool> a1;
    optional<bool> b0;
    optional<bool> b1;
  };
  using button_sensor_t = binary_sensor::BinarySensor *;
  using button_t = struct {
    button_predicate_t predicate;
    button_sensor_t sensor;
  };
  using buttons_t = std::vector<button_t>;
  using manufacturer_t = esp32_ble_tracker::ESPBTUUID;
  using data_t = std::vector<uint8_t>;
  using sequence_counter_t = uint32_t;
  using security_signature_t = array_t<4>;
  using switch_status_t = struct __packed {
    bool press : 1;
    bool a0 : 1;
    bool a1 : 1;
    bool b0 : 1;
    bool b1 : 1;
  };
  using data_telegram_t = struct __packed {
    sequence_counter_t sequence_counter;
    switch_status_t switch_status;
    security_signature_t security_signature;
  };
  using commissioning_telegram_t = struct __packed {
    sequence_counter_t sequence_counter;
    security_key_t security_key;
    address_t static_source_address;
  };

  virtual ~PTM215B(){};

  void set_address(const address_t &&address) { this->address_ = address; }
  void set_key(const security_key_t &&key) { this->security_key_ = key; }
  void set_button(const button_predicate_t &&predicate, button_sensor_t sensor) {
    this->buttons_.push_back({predicate, sensor});
  }

  void dump_config();

  bool parse_device(const esp32_ble_tracker::ESPBTDevice &device) override;

  switch_status_t get_state() const { return this->switch_status_; };

 protected:
  address_t address_{};
  security_key_t security_key_{};
  buttons_t buttons_{};

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
