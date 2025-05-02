#pragma once

#include "esphome/core/component.h"
#include "esphome/core/automation.h"
#include "esphome/components/cover/cover.h"
#include <map>
#include <string>

namespace esphome {
namespace awning {

class AwningCover : public cover::Cover, public Component {
 public:
  void setup() override;
  void loop() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::HARDWARE; }

  cover::CoverTraits get_traits() override {
    auto traits = cover::CoverTraits();
    traits.set_is_assumed_state(this->assumed_state_);
    traits.set_supports_position(true);
    traits.set_supports_tilt(false);
    return traits;
  }

  void control(const cover::CoverCall &call) override;

  // Configuración
  void set_calibration_enabled(bool enabled) { calibration_enabled_ = enabled; }
  bool calibration_is_enabled() const { return calibration_enabled_; }
  void set_pulse_pause(uint32_t pause) { pulse_pause_ = pause; }
  void set_final_pause(uint32_t pause) { final_pause_ = pause; }
  void set_reset_sequence(const std::vector<std::string> &seq) { reset_sequence_ = seq; }
  void set_up_sequence(const std::vector<std::string> &seq) { up_sequence_ = seq; }
  void set_down_sequence(const std::vector<std::string> &seq) { down_sequence_ = seq; }
  void set_open_duration(uint32_t duration) { this->open_duration_ = duration; }
  void set_close_duration(uint32_t duration) { this->close_duration_ = duration; }
  void set_has_built_in_endstop(bool has_endstop) { this->has_built_in_endstop_ = has_endstop; }
  void set_assumed_state(bool assumed_state) { this->assumed_state_ = assumed_state; }

  Trigger<> *get_open_trigger() const { return this->open_trigger_; }
  Trigger<> *get_close_trigger() const { return this->close_trigger_; }
  Trigger<> *get_stop_trigger() const { return this->stop_trigger_; }
  Trigger<> *get_calibrate_trigger() const { return this->calibrate_trigger_; }

  // Métodos de calibración
  void calibrate(uint8_t action);
  bool is_stopped() const { return !moving_up_ && !moving_down_; }

  // Registro de nombres de calibración
  void register_calibration_name(uint8_t action, const std::string &name) { this->calibration_names_[action] = name; }
  const std::string &get_calibration_name(uint8_t action) const {
    auto it = this->calibration_names_.find(action);
    return it != this->calibration_names_.end() ? it->second : unknown_action_;
  }

 protected:
  // Calibración
  bool calibration_enabled_{true};
  std::vector<std::string> reset_sequence_;
  std::vector<std::string> up_sequence_;
  std::vector<std::string> down_sequence_;
  uint32_t pulse_pause_{250};
  uint32_t final_pause_{2500};

  uint8_t current_step_{0};
  std::vector<std::string> *current_sequence_{nullptr};

  void execute_next_calibration_step_();
  void schedule_next_step_(uint32_t delay);

  // Estado interno
  uint32_t open_duration_{0};   // Duración apertura en ms
  uint32_t close_duration_{0};  // Duración cierre en ms
  float current_position_{0.0f};
  float target_position_{0.0f};
  float original_position_{0.0f};
  bool moving_up_{false};
  bool moving_down_{false};
  uint32_t movement_start_time_{0};
  uint32_t last_report_time_{0};  // Tiempo del último reporte de posición
  bool has_built_in_endstop_{false};
  bool assumed_state_{true};

  // Triggers para acciones
  Trigger<> *open_trigger_{new Trigger<>()};
  Trigger<> *close_trigger_{new Trigger<>()};
  Trigger<> *stop_trigger_{new Trigger<>()};
  Trigger<> *calibrate_trigger_{new Trigger<>()};

  // Mapa de nombres de calibración
  std::map<uint8_t, std::string> calibration_names_;
  const std::string unknown_action_{"unknown"};

  // Métodos auxiliares
  float angle_to_position_(float angle);
  float position_to_angle_(float position);
  void stop_();
  void start_up_();
  void start_down_();
  uint32_t calculate_travel_time_(float start_pos, float target_pos);
};

}  // namespace awning
}  // namespace esphome
