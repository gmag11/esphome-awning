#pragma once

#include "esphome/core/component.h"
#include "esphome/core/automation.h"
#include "awning_cover.h"

namespace esphome {
namespace awning {

enum CalibrationType { CALIBRATION_RESET = 0, CALIBRATION_UP = 1, CALIBRATION_DOWN = 2 };

template<typename... Ts> class CalibrateAction : public Action<Ts...>, public Parented<AwningCover> {
 public:
  void play(Ts... x) override {
    if (!this->parent_->calibration_is_enabled())
      return;
    if (this->action_.has_value()) {
      this->parent_->calibrate(this->action_.value(x...));
    }
  }

  void set_action(uint8_t action) { this->action_ = action; }

 protected:
  TEMPLATABLE_VALUE(uint8_t, action)
};

}  // namespace awning
}  // namespace esphome
