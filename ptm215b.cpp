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

std::string to_string(const PTM215B::switch_status_t &switch_status) {
  std::stringstream ss;
  ss << (switch_status.press ? "Press" : "Release");
  ss << (switch_status.A0 ? " A0" : "");
  ss << (switch_status.A1 ? " A1" : "");
  ss << (switch_status.B0 ? " B0" : "");
  ss << (switch_status.B1 ? " B1" : "");
  return ss.str();
}

}  // namespace

static const char *const TAG = "ptm215b";

void PTM215B::dump_config() {
  ESP_LOGCONFIG(TAG, "PTM 215B:");
  ESP_LOGCONFIG(TAG, " Address: %s", to_string(this->address_).c_str());
  ESP_LOGCONFIG(TAG, " Security Key: %s", to_string(this->security_key_).c_str());
  LOG_BINARY_SENSOR(" ", "Any Button", this->bar_sensor_);
  LOG_BINARY_SENSOR(" ", "A0 Button", this->a0_sensor_);
  LOG_BINARY_SENSOR(" ", "A1 Button", this->a1_sensor_);
  LOG_BINARY_SENSOR(" ", "B0 Button", this->b0_sensor_);
  LOG_BINARY_SENSOR(" ", "B1 Button", this->b1_sensor_);
}

bool PTM215B::parse_device(const esp32_ble_tracker::ESPBTDevice &device) {
  if (!this->check_address(*reinterpret_cast<const address_t *>(device.address()))) {
    return false;
  }

  for (auto &manufacturer_data : device.get_manufacturer_datas()) {
    if (!this->check_manufacturer(manufacturer_data.uuid)) {
      continue;
    }

    return this->handle_data(manufacturer_data.data);
  }

  return false;
}

bool PTM215B::check_address(const address_t &address) {
  if (address == this->address_) {
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
  auto data_telegram_o = this->parse_data_telegram(data);
  if (data_telegram_o.has_value()) {
    return this->handle_data_telegram(data_telegram_o.value());
  }

  auto commissioning_telegram_o = this->parse_commissioning_telegram(data);
  if (commissioning_telegram_o.has_value()) {
    return this->handle_commissioning_telegram(commissioning_telegram_o.value());
  }

  return false;
}

optional<PTM215B::data_telegram_t> PTM215B::parse_data_telegram(const data_t &data) {
  if (data.size() != sizeof(data_telegram_t)) {
    return nullopt;
  }

  union {
    data_telegram_t data_telegram;
    std::array<uint8_t, sizeof(data_telegram_t)> buff;
  };

  std::copy_n(data.begin(), buff.size(), buff.begin());

  return make_optional(data_telegram);
}

optional<PTM215B::commissioning_telegram_t> PTM215B::parse_commissioning_telegram(const data_t &data) {
  if (data.size() != sizeof(commissioning_telegram_t)) {
    return nullopt;
  }

  union {
    commissioning_telegram_t commissioning_telegram;
    std::array<uint8_t, sizeof(commissioning_telegram_t)> buff;
  };

  std::copy_n(data.begin(), buff.size(), buff.begin());

  return make_optional(commissioning_telegram);
}

bool PTM215B::handle_data_telegram(const data_telegram_t &data_telegram) {
  if (!this->check_debounce(data_telegram.sequence_counter)) {
    return false;
  }

  if (!this->check_replay(data_telegram.sequence_counter)) {
    return false;
  }

  if (!this->check_signature(data_telegram)) {
    return false;
  }

  this->update_sequence_counter(data_telegram.sequence_counter);

  this->update_switch_status(data_telegram.switch_status);

  this->notify();

  return true;
}

bool PTM215B::handle_commissioning_telegram(const commissioning_telegram_t &commissioning_telegram) {
  if (!this->check_debounce(commissioning_telegram.sequence_counter)) {
    return false;
  }

  if (!this->check_replay(commissioning_telegram.sequence_counter)) {
    return false;
  }

  this->update_sequence_counter(commissioning_telegram.sequence_counter);

  ESP_LOGI(TAG,
           "%s: Device is in commisioning mode! Security key is %s. (To exit commisioning mode invoke `Any Other "
           "Button Action`)",
           to_string(this->address_).c_str(), to_string(commissioning_telegram.security_key).c_str());

  return true;
}

bool PTM215B::check_debounce(const sequence_counter_t &sequence_counter) {
  if (sequence_counter != this->sequence_counter_) {
    return true;
  } else {
    return false;
  }
}

bool PTM215B::check_replay(const sequence_counter_t &sequence_counter) {
  if (sequence_counter > this->sequence_counter_) {
    return true;
  } else {
    return false;
  }
}

bool PTM215B::check_signature(const data_telegram_t &data_telegram) {
  if (this->security_key_ == security_key_t{}) {
    ESP_LOGD(TAG, "%s: Skipping signature check, key is empty.", to_string(this->address_).c_str());
    return true;
  }

  std::unique_ptr<mbedtls_ccm_context, std::function<void(mbedtls_ccm_context *)>> ctx(([](mbedtls_ccm_context *ctx) {
                                                                                         mbedtls_ccm_init(ctx);
                                                                                         return ctx;
                                                                                       })(new mbedtls_ccm_context),
                                                                                       [](mbedtls_ccm_context *ctx) {
                                                                                         mbedtls_ccm_free(ctx);
                                                                                         delete ctx;
                                                                                       });

  if (mbedtls_ccm_setkey(ctx.get(), MBEDTLS_CIPHER_ID_AES, this->security_key_.data(),
                         this->security_key_.size() * 8)) {
    return false;
  }

  union {
    struct __packed {
      std::array<uint8_t, 6> source_address;
      sequence_counter_t sequence_counter;
      std::array<uint8_t, 3> _padding;
    } fields;
    std::array<uint8_t, 13> buff;
  } nonce{{{this->address_[5], this->address_[4], this->address_[3], this->address_[2], this->address_[1],
            this->address_[0]},
           data_telegram.sequence_counter,
           {0, 0, 0}}};

  union {
    struct __packed {
      uint8_t len;
      uint8_t type;
      uint16_t manufacturer;
      sequence_counter_t sequence_counter;
      switch_status_t state;
    } fields;
    std::array<uint8_t, 9> buff;
  } payload{{0x0C, 0xFF, 0x03DA, data_telegram.sequence_counter, data_telegram.switch_status}};

  if (mbedtls_ccm_auth_decrypt(ctx.get(), 0, nonce.buff.data(), nonce.buff.size(), payload.buff.data(),
                               payload.buff.size(), nullptr, nullptr, data_telegram.security_signature.data(),
                               data_telegram.security_signature.size())) {
    return false;
  }

  return true;
}

void PTM215B::update_sequence_counter(const sequence_counter_t &sequence_counter) {
  this->sequence_counter_ = sequence_counter;
}

void PTM215B::update_switch_status(const switch_status_t &switch_status) {
  this->switch_status_ = switch_status;
  ESP_LOGI(TAG, "%s: %s", to_string(this->address_).c_str(), to_string(this->switch_status_).c_str());
}

void PTM215B::notify() {
  if (this->bar_sensor_) {
    this->bar_sensor_->publish_state(this->switch_status_.press);
  }
  if (this->a0_sensor_) {
    this->a0_sensor_->publish_state(this->switch_status_.A0);
  }
  if (this->a1_sensor_) {
    this->a1_sensor_->publish_state(this->switch_status_.A1);
  }
  if (this->b0_sensor_) {
    this->b0_sensor_->publish_state(this->switch_status_.B0);
  }
  if (this->b1_sensor_) {
    this->b1_sensor_->publish_state(this->switch_status_.B1);
  }
}

}  // namespace ptm215b
}  // namespace esphome

#endif
