from esphome import automation
import esphome.codegen as cg
from esphome.components import cover
import esphome.config_validation as cv
from esphome.const import (
    CONF_ACTION,
    CONF_ASSUMED_STATE,
    CONF_CALIBRATION,
    CONF_CLOSE_ACTION,
    CONF_CLOSE_DURATION,
    CONF_ID,
    CONF_OPEN_ACTION,
    CONF_OPEN_DURATION,
    CONF_STOP_ACTION,
    DEVICE_CLASS_AWNING,
)

CONF_CALIBRATE = "calibrate"
CONF_CALIBRATION_ENABLED = "enabled"
CONF_RESET_SEQUENCE = "reset_sequence"
CONF_UP_SEQUENCE = "up_sequence"
CONF_DOWN_SEQUENCE = "down_sequence"
CONF_PULSE_PAUSE = "pulse_pause"
CONF_FINAL_PAUSE = "final_pause"

CALIBRATION_RESET = "reset"
CALIBRATION_UP = "up"
CALIBRATION_DOWN = "down"

CALIBRATION_ACTIONS = {
    CALIBRATION_RESET: 0,
    CALIBRATION_UP: 1,
    CALIBRATION_DOWN: 2,
}

CALIBRATION_NAMES = {
    0: CALIBRATION_RESET,
    1: CALIBRATION_UP,
    2: CALIBRATION_DOWN,
}


def validate_calibration_action(value):
    if isinstance(value, str):
        try:
            return CALIBRATION_ACTIONS[value]
        except KeyError as exc:
            raise cv.Invalid(
                f"Invalid calibration action '{value}'. Valid values are: reset, up, down"
            ) from exc
    return cv.int_range(min=0, max=2)(value)


DEPENDENCIES = ["output"]
AUTO_LOAD = ["cover"]

awning_ns = cg.esphome_ns.namespace("awning")
AwningCover = awning_ns.class_("AwningCover", cover.Cover, cg.Component)
CalibrateAction = awning_ns.class_("CalibrateAction", automation.Action)

CONF_HAS_BUILT_IN_ENDSTOP = "has_built_in_endstop"

CONFIG_SCHEMA = cover.cover_schema(
    AwningCover,
    device_class=DEVICE_CLASS_AWNING,
    icon="mdi:window-shutter",
).extend(
    {
        cv.Required(CONF_OPEN_DURATION): cv.positive_time_period_milliseconds,
        cv.Required(CONF_CLOSE_DURATION): cv.positive_time_period_milliseconds,
        cv.Required(CONF_OPEN_ACTION): automation.validate_automation(single=True),
        cv.Required(CONF_CLOSE_ACTION): automation.validate_automation(single=True),
        cv.Required(CONF_STOP_ACTION): automation.validate_automation(single=True),
        cv.Optional(CONF_HAS_BUILT_IN_ENDSTOP, default=False): cv.boolean,
        cv.Optional(CONF_ASSUMED_STATE, default=True): cv.boolean,
        cv.Optional(CONF_CALIBRATION): cv.Schema(
            {
                cv.Optional(CONF_CALIBRATION_ENABLED, default=True): cv.boolean,
                cv.Optional(CONF_RESET_SEQUENCE, default=[]): cv.ensure_list(
                    cv.one_of("up", "down", "stop", lower=True)
                ),
                cv.Optional(CONF_UP_SEQUENCE, default=[]): cv.ensure_list(
                    cv.one_of("up", "down", "stop", lower=True)
                ),
                cv.Optional(CONF_DOWN_SEQUENCE, default=[]): cv.ensure_list(
                    cv.one_of("up", "down", "stop", lower=True)
                ),
                cv.Optional(
                    CONF_PULSE_PAUSE, default="250ms"
                ): cv.positive_time_period_milliseconds,
                cv.Optional(
                    CONF_FINAL_PAUSE, default="2500ms"
                ): cv.positive_time_period_milliseconds,
            }
        ),
    }
).extend(cv.COMPONENT_SCHEMA)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await cover.register_cover(var, config)

    cg.add(var.set_open_duration(config[CONF_OPEN_DURATION]))
    await automation.build_automation(
        var.get_open_trigger(), [], config[CONF_OPEN_ACTION]
    )

    cg.add(var.set_close_duration(config[CONF_CLOSE_DURATION]))
    await automation.build_automation(
        var.get_close_trigger(), [], config[CONF_CLOSE_ACTION]
    )

    await automation.build_automation(
        var.get_stop_trigger(), [], config[CONF_STOP_ACTION]
    )

    cg.add(var.set_has_built_in_endstop(config[CONF_HAS_BUILT_IN_ENDSTOP]))
    cg.add(var.set_assumed_state(config[CONF_ASSUMED_STATE]))

    if CONF_CALIBRATION in config:
        cal_config = config[CONF_CALIBRATION]
        cg.add(var.set_calibration_enabled(cal_config[CONF_CALIBRATION_ENABLED]))
        cg.add(var.set_reset_sequence(cal_config[CONF_RESET_SEQUENCE]))
        cg.add(var.set_up_sequence(cal_config[CONF_UP_SEQUENCE]))
        cg.add(var.set_down_sequence(cal_config[CONF_DOWN_SEQUENCE]))
        cg.add(var.set_pulse_pause(cal_config[CONF_PULSE_PAUSE]))
        cg.add(var.set_final_pause(cal_config[CONF_FINAL_PAUSE]))

    # Add the calibration names mapping
    for value, name in CALIBRATION_NAMES.items():
        cg.add(var.register_calibration_name(value, name))


@automation.register_action(
    "awning.calibrate",
    CalibrateAction,
    cv.Schema(
        {
            cv.Required(CONF_ID): cv.use_id(AwningCover),
            cv.Required(CONF_ACTION): cv.templatable(validate_calibration_action),
        }
    ),
)
def awning_calibrate_to_code(config, action_id, template_arg, args):
    var = cg.new_Pvariable(action_id, template_arg)
    yield cg.register_parented(var, config[CONF_ID])
    paren = yield cg.get_variable(config[CONF_ID])

    # Solo registrar la acción si la calibración está habilitada
    if paren.calibration_is_enabled():
        template_ = yield cg.templatable(config[CONF_ACTION], args, cg.uint8)
        cg.add(var.set_action(template_))
        yield var
