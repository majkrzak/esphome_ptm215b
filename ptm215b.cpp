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
  ss << (switch_status.a0 ? " A0" : "");
  ss << (switch_status.a1 ? " A1" : "");
  ss << (switch_status.b0 ? " B0" : "");
  ss << (switch_status.b1 ? " B1" : "");
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
  auto &address = *reinterpret_cast<const address_t *>(device.address());
  if (!this->check_address(address)) {
    ESP_LOGV(TAG, "%s: Address %s do not match. Skipping.", to_string(this->address_).c_str(),
             to_string(address).c_str());
    return false;
  }

  for (auto &manufacturer_data : device.get_manufacturer_datas()) {
    if (!this->check_manufacturer(manufacturer_data.uuid)) {
      ESP_LOGV(TAG, "%s: Manufacturer %s do not match EnOcean id. Skipping.", to_string(this->address_).c_str(),
               to_string(manufacturer_data.uuid).c_str());
      continue;
    }

    ESP_LOGV(TAG, "%s: Received EnOcean Manufacturer Data.", to_string(this->address_).c_str());
    return this->handle_data(manufacturer_data.data);
  }

  ESP_LOGE(TAG, "%s: No EnOcean Manufacturer Data received.", to_string(this->address_).c_str());
  return false;
}

bool PTM215B::check_address(const address_t &address) { return address == this->address_; }

bool PTM215B::check_manufacturer(const manufacturer_t &manufacturer) {
  return manufacturer == esp32_ble_tracker::ESPBTUUID::from_uint16(0x03DA);
}

bool PTM215B::handle_data(const data_t &data) {
  auto data_telegram_o = this->parse_data_telegram(data);
  if (data_telegram_o.has_value()) {
    ESP_LOGV(TAG, "%s: Received Data Telegram.", to_string(this->address_).c_str());
    return this->handle_data_telegram(data_telegram_o.value());
  }

  auto commissioning_telegram_o = this->parse_commissioning_telegram(data);
  if (commissioning_telegram_o.has_value()) {
    ESP_LOGV(TAG, "%s: Received Commissionning Telegram.", to_string(this->address_).c_str());
    return this->handle_commissioning_telegram(commissioning_telegram_o.value());
  }

  ESP_LOGE(TAG, "%s: Unrecognized payload.", to_string(this->address_).c_str());
  return false;
}

optional<PTM215B::data_telegram_t> PTM215B::parse_data_telegram(const data_t &data) {
  if (data.size() != sizeof(data_telegram_t)) {
    return nullopt;
  }

  union {
    data_telegram_t data_telegram;
    array_t<sizeof(data_telegram_t)> buff;
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
    array_t<sizeof(commissioning_telegram_t)> buff;
  };

  std::copy_n(data.begin(), buff.size(), buff.begin());

  return make_optional(commissioning_telegram);
}

bool PTM215B::handle_data_telegram(const data_telegram_t &data_telegram) {
  if (!this->check_debounce(data_telegram.sequence_counter)) {
    ESP_LOGV(TAG, "%s: Debouncing packet.", to_string(this->address_).c_str());
    return false;
  }

  if (!this->check_replay(data_telegram.sequence_counter)) {
    ESP_LOGE(TAG, "%s: Replayed packet.", to_string(this->address_).c_str());
    return false;
  }

  if (!this->check_signature(data_telegram)) {
    ESP_LOGE(TAG, "%s: Invalid signature.", to_string(this->address_).c_str());
    return false;
  }

  this->update_sequence_counter(data_telegram.sequence_counter);

  this->update_switch_status(data_telegram.switch_status);

  this->notify();

  ESP_LOGI(TAG, "%s: %s", to_string(this->address_).c_str(), to_string(this->switch_status_).c_str());

  return true;
}

bool PTM215B::handle_commissioning_telegram(const commissioning_telegram_t &commissioning_telegram) {
  if (!this->check_debounce(commissioning_telegram.sequence_counter)) {
    ESP_LOGV(TAG, "%s: Debouncing packet.", to_string(this->address_).c_str());
    return false;
  }

  if (!this->check_replay(commissioning_telegram.sequence_counter)) {
    ESP_LOGE(TAG, "%s: Replayed packet.", to_string(this->address_).c_str());
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
  return sequence_counter != this->sequence_counter_;
}

bool PTM215B::check_replay(const sequence_counter_t &sequence_counter) {
  return sequence_counter > this->sequence_counter_;
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
    ESP_LOGE(TAG, "%s: mbedtls_ccm_setkey failed", to_string(this->address_).c_str());
    return false;
  }

  union {
    struct __packed {
      address_t source_address;
      sequence_counter_t sequence_counter;
      array_t<3> _padding;
    } fields;
    array_t<13> buff;
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
    array_t<9> buff;
  } payload{{0x0C, 0xFF, 0x03DA, data_telegram.sequence_counter, data_telegram.switch_status}};

  ESP_LOGV(TAG, "%s: NONCE: %s", to_string(this->address_).c_str(), to_string(nonce.buff).c_str());
  ESP_LOGV(TAG, "%s: PAYLOAD: %s", to_string(this->address_).c_str(), to_string(payload.buff).c_str());

  if (mbedtls_ccm_auth_decrypt(ctx.get(), 0, nonce.buff.data(), nonce.buff.size(), payload.buff.data(),
                               payload.buff.size(), nullptr, nullptr, data_telegram.security_signature.data(),
                               data_telegram.security_signature.size())) {
    ESP_LOGE(TAG, "%s: mbedtls_ccm_auth_decrypt failed", to_string(this->address_).c_str());
    return false;
  }

  ESP_LOGD(TAG, "%s: Signature validated sucessfuly.", to_string(this->address_).c_str());
  return true;
}

void PTM215B::update_sequence_counter(const sequence_counter_t &sequence_counter) {
  this->sequence_counter_ = sequence_counter;
}

void PTM215B::update_switch_status(const switch_status_t &switch_status) { this->switch_status_ = switch_status; }

void PTM215B::notify() {
  if (this->bar_sensor_) {
    this->bar_sensor_->publish_state(this->switch_status_.press);
  }
  if (this->a0_sensor_) {
    this->a0_sensor_->publish_state(this->switch_status_.a0);
  }
  if (this->a1_sensor_) {
    this->a1_sensor_->publish_state(this->switch_status_.a1);
  }
  if (this->b0_sensor_) {
    this->b0_sensor_->publish_state(this->switch_status_.b0);
  }
  if (this->b1_sensor_) {
    this->b1_sensor_->publish_state(this->switch_status_.b1);
  }
}

}  // namespace ptm215b
}  // namespace esphome

#endif
