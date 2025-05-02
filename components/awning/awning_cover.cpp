#include "awning_cover.h"
#include "esphome/core/log.h"
#include "esphome/core/hal.h"
#include "esphome/components/cover/cover.h"

namespace esphome {
namespace awning {

using namespace cover;
static const char *const TAG = "awning.cover";
static const uint32_t POSITION_REPORT_INTERVAL = 1000;  // 1 second in milliseconds
static const uint32_t IDLE_REPORT_INTERVAL = 30000;     // 30 seconds in milliseconds

// Tabla de transformación de posición-ángulo (0-1)
static const uint8_t POS_LEN_LUT[] = {
    0,  0,  0,  0,  0,  0,  0,  1,  1,  1,   // 0  -  9
    1,  2,  2,  2,  2,  3,  3,  4,  4,  4,   // 10 - 19
    5,  5,  6,  6,  7,  8,  8,  9,  9,  10,  // 20 - 29
    11, 12, 13, 13, 14, 15, 16, 17, 17, 18,  // 30 - 39
    19, 20, 21, 22, 23, 24, 25, 26, 27, 28,  // 40 - 49
    29, 30, 32, 33, 34, 35, 36, 38, 39, 40,  // 50 - 59
    41, 43, 44, 45, 46, 48, 49, 51, 52, 53,  // 60 - 69
    55, 56, 58, 59, 60, 62, 63, 65, 66, 67,  // 70 - 79
    69, 71, 72, 73, 75, 77, 78, 80, 81, 83,  // 80 - 89
    84, 86, 87, 89, 90, 92, 94, 95, 97, 98,  // 90 - 99
    100                                      // 100
};

void AwningCover::setup() {
  ESP_LOGCONFIG(TAG, "Setting up Awning Cover...");
  this->current_position_ = this->position;
  ESP_LOGCONFIG(TAG, "Initial position: %.2f", this->position);
}

void AwningCover::loop() {
  const uint32_t now = millis();

  if (this->moving_up_ || this->moving_down_) {
    const uint32_t elapsed = now - this->movement_start_time_;
    const uint32_t travel_time = this->calculate_travel_time_(this->original_position_, this->target_position_);

    // ESP_LOGD (TAG, "Movement in progress - Elapsed: %ums/%ums", elapsed, travel_time);
    // ESP_LOGD (TAG, "Current position: %.3f, Target: %.3f", this->current_position_, this->target_position_);

    // Calcular posición actual basada en el tiempo transcurrido
    if (this->moving_up_) {
      if (elapsed >= travel_time) {
        this->current_position_ = this->target_position_;
        if (this->target_position_ >= 1.0f) {
          // For max position, keep motor active until open_duration_ is completed
          if (elapsed >= this->open_duration_) {
            ESP_LOGW(TAG, "Full opening duration reached - Motor stopped at max position");
            this->stop_();
          }
        } else {
          ESP_LOGD(TAG, "Movement time reached - Setting to target position %.3f", this->target_position_);
          this->stop_();
        }
      } else {
        float progress = elapsed / (float) travel_time;
        this->current_position_ =
            this->original_position_ + (this->target_position_ - this->original_position_) * progress;
      }
    } else if (this->moving_down_) {
      if (elapsed >= travel_time) {
        this->current_position_ = this->target_position_;
        if (this->target_position_ <= 0.0f) {
          // For min position, keep motor active until close_duration_ is completed
          if (elapsed >= this->close_duration_) {
            ESP_LOGW(TAG, "Full closing duration reached - Motor stopped at min position");
            this->stop_();
          }
        } else {
          ESP_LOGD(TAG, "Movement time reached - Setting to target position %.3f", this->target_position_);
          this->stop_();
        }
      } else {
        float progress = elapsed / (float) travel_time;
        this->current_position_ =
            this->original_position_ + (this->target_position_ - this->original_position_) * progress;
      }
    }

    // Publicar estado cada segundo durante el movimiento
    if ((now - this->last_report_time_) >= POSITION_REPORT_INTERVAL) {
      this->position = this->position_to_angle_(this->current_position_);
      ESP_LOGD(TAG, "Publishing state - Linear position: %.3f, Angle position: %.3f", this->current_position_,
               this->position);
      this->publish_state();
      this->last_report_time_ = now;
    }
  } else {
    // Publish state every 5 minutes when idle
    if ((now - this->last_report_time_) >= IDLE_REPORT_INTERVAL) {
      this->position = this->position_to_angle_(this->current_position_);
      ESP_LOGD(TAG, "Publishing state - Linear position: %.3f, Angle position: %.3f", this->current_position_,
               this->position);
      this->publish_state();
      this->last_report_time_ = now;
    }
  }
}

void AwningCover::dump_config() {
  ESP_LOGCONFIG(TAG, "Awning Cover:");
  ESP_LOGCONFIG(TAG, "  Open Duration: %ums", this->open_duration_);
  ESP_LOGCONFIG(TAG, "  Close Duration: %ums", this->close_duration_);
  ESP_LOGCONFIG(TAG, "  Current Position: %.2f", this->current_position_);
  ESP_LOGCONFIG(TAG, "  Has Built-in Endstop: %s", YESNO(this->has_built_in_endstop_));
}

float AwningCover::angle_to_position_(float angle) {
  if (angle < 0.0f) {
    ESP_LOGW(TAG, "Angle %.3f out of bounds (< 0.0) - Clamping to 0.0", angle);
    return 0.0f;
  }
  if (angle > 1.0f) {
    ESP_LOGW(TAG, "Angle %.3f out of bounds (> 1.0) - Clamping to 1.0", angle);
    return 1.0f;
  }

  int idx = (int) (angle * 100);
  return POS_LEN_LUT[idx] / 100.0f;
}

float AwningCover::position_to_angle_(float position) {
  if (position <= 0.0f) {
    if (this->current_operation != COVER_OPERATION_IDLE)
      ESP_LOGW(TAG, "Position %.3f out of bounds (<= 0.0) - Clamping to 0.0", position);
    return 0.0f;
  }
  if (position >= 1.0f) {
    if (this->current_operation != COVER_OPERATION_IDLE)
      ESP_LOGW(TAG, "Position %.3f out of bounds (>= 1.0) - Clamping to 1.0", position);
    return 1.0f;
  }

  int pos_int = (int) (position * 100);
  for (int i = 0; i < sizeof(POS_LEN_LUT); i++) {
    if (POS_LEN_LUT[i] >= pos_int) {
      return i / 100.0f;
    }
  }
  return 1.0f;
}

void AwningCover::stop_() {
  ESP_LOGI(TAG, "Stopping movement at position %.3f", this->current_position_);
  this->moving_up_ = false;
  this->moving_down_ = false;
  this->stop_trigger_->trigger();
  this->position = this->position_to_angle_(this->current_position_);
  this->current_operation = COVER_OPERATION_IDLE;
  ESP_LOGD(TAG, "Publishing state - Linear position: %.3f, Angle position: %.3f", this->current_position_,
           this->position);
  this->publish_state();
}

void AwningCover::start_up_() {
  ESP_LOGI(TAG, "Starting upward movement from %.3f to %.3f", this->current_position_, this->target_position_);
  this->moving_down_ = false;
  this->moving_up_ = true;
  this->movement_start_time_ = millis();
  this->original_position_ = this->current_position_;
  this->open_trigger_->trigger();
  this->current_operation = COVER_OPERATION_OPENING;
}

void AwningCover::start_down_() {
  ESP_LOGI(TAG, "Starting downward movement from %.3f to %.3f", this->current_position_, this->target_position_);
  this->moving_up_ = false;
  this->moving_down_ = true;
  this->movement_start_time_ = millis();
  this->original_position_ = this->current_position_;
  this->close_trigger_->trigger();
  this->current_operation = COVER_OPERATION_CLOSING;
}

void AwningCover::control(const cover::CoverCall &call) {
  if (call.get_position().has_value()) {
    float pos = *call.get_position();
    ESP_LOGD(TAG, "Control - Requested position: %.2f", pos);

    // Convertir posición a ángulo
    this->target_position_ = this->angle_to_position_(pos);

    // Iniciar movimiento incluso si ya estamos en la posición
    if (pos >= 1.0f || (pos > 0.0f && this->target_position_ > this->current_position_)) {
      this->start_up_();
    } else if (pos <= 0.0f || this->target_position_ < this->current_position_) {
      this->start_down_();
    }
  }

  if (call.get_stop()) {
    ESP_LOGD(TAG, "Control - Stop requested");
    this->stop_();
  }
}

uint32_t AwningCover::calculate_travel_time_(float start_pos, float target_pos) {
  // Calculate proportional time based on actual distance
  float travel = abs(target_pos - start_pos);
  uint32_t base_time;

  if (target_pos > start_pos) {
    base_time = (uint32_t) (travel * this->open_duration_);
  } else {
    base_time = (uint32_t) (travel * this->close_duration_);
  }

  // Ensure minimum movement time for small movements
  const uint32_t final_time = base_time > 100 ? base_time : 100;
  // if (final_time == 100) {
  //   ESP_LOGD (TAG, "Applying minimum movement time of 100ms");
  // }
  return final_time;
}

void AwningCover::calibrate(uint8_t action) {
  if (!this->calibration_enabled_) {
    ESP_LOGW(TAG, "Calibration is disabled");
    return;
  }

  if (!this->is_stopped()) {
    ESP_LOGW(TAG, "Cannot calibrate while awning is moving");
    return;
  }

  ESP_LOGI(TAG, "Starting calibration sequence '%s'", this->get_calibration_name(action).c_str());

  switch (action) {
    case 0:  // Reset calibration
      if (reset_sequence_.empty()) {
        ESP_LOGW(TAG, "Reset sequence not configured");
        return;
      }
      current_sequence_ = &reset_sequence_;
      break;
    case 1:  // Upper limit calibration
      if (up_sequence_.empty()) {
        ESP_LOGW(TAG, "Up sequence not configured");
        return;
      }
      current_sequence_ = &up_sequence_;
      break;
    case 2:  // Lower limit calibration
      if (down_sequence_.empty()) {
        ESP_LOGW(TAG, "Down sequence not configured");
        return;
      }
      current_sequence_ = &down_sequence_;
      break;
    default:
      ESP_LOGW(TAG, "Invalid calibration action: %u", action);
      return;
  }

  current_step_ = 0;
  this->calibrate_trigger_->trigger();
  execute_next_calibration_step_();
}

void AwningCover::execute_next_calibration_step_() {
  if (!current_sequence_ || current_step_ >= current_sequence_->size() * 2) {
    ESP_LOGI(TAG, "Calibration sequence completed");
    current_sequence_ = nullptr;
    current_step_ = 0;
    return;
  }

  bool is_pulse = (current_step_ % 2) == 0;
  uint8_t sequence_index = current_step_ / 2;

  if (is_pulse) {
    // Ejecutar la acción
    const auto &action = (*current_sequence_)[sequence_index];
    ESP_LOGD(TAG, "Executing calibration step %u: %s", sequence_index, action.c_str());

    if (action == "up") {
      this->open_trigger_->trigger();
    } else if (action == "down") {
      this->close_trigger_->trigger();
    } else if (action == "stop") {
      this->stop_trigger_->trigger();
    } else {
      ESP_LOGW(TAG, "Invalid calibration action: %s", action.c_str());
      current_sequence_ = nullptr;
      current_step_ = 0;
      return;
    }

    // Programar el final del pulso
    current_step_++;
    schedule_next_step_(pulse_pause_);
  } else {
    // Final del pulso
    this->stop_trigger_->trigger();

    // Programar siguiente acción o pausa final
    current_step_++;
    if (current_step_ >= current_sequence_->size() * 2) {
      schedule_next_step_(final_pause_);
    } else {
      schedule_next_step_(pulse_pause_);
    }
  }
}

void AwningCover::schedule_next_step_(uint32_t delay) {
  this->set_timeout(delay, [this]() { this->execute_next_calibration_step_(); });
}

}  // namespace awning
}  // namespace esphome
