#include "motor/tuning_parameter.h"

#include <stdint.h>

static uint8_t clamp(uint8_t value, uint8_t maximum) { return value > maximum ? maximum : value; }

static void encode_u8(MotorParameterWrite *write, uint8_t address, uint8_t value) {
    write->address = address;
    write->length = 1;
    write->data[0] = value;
    write->data[1] = 0;
}

static void encode_u16(MotorParameterWrite *write, uint8_t address, uint16_t value) {
    write->address = address;
    write->length = 2;
    write->data[0] = (uint8_t)value;
    write->data[1] = (uint8_t)(value >> 8);
}

static uint8_t scale_percent_to_u8(uint8_t value) {
    return (uint8_t)((uint16_t)clamp(value, 100) * UINT8_MAX / 100);
}

static uint16_t scale_friction(const MotorTuningValues *values, const MotorTuningContext *context) {
    uint32_t friction = (uint32_t)clamp(values->natural_friction, 100) * UINT16_MAX / 100;
    friction = friction * clamp(context->ramp_percent, 100) / 100;
    friction = friction * clamp(context->strength_percent, 100) / 100;
    return (uint16_t)friction;
}

static uint8_t encode_interpolation_filter(const MotorTuningValues *values,
                                           const MotorTuningContext *context) {
    uint8_t filter = clamp(values->interpolation_filter, 20);
    if (context->xbox_mode != 0 && filter <= 9) {
        return 10 - filter;
    }
    return 20 - filter;
}

uint8_t motor_tuning_parameter_encode(MotorTuningParameter parameter,
                                      const MotorTuningValues *values,
                                      const MotorTuningContext *context,
                                      MotorParameterWrite *write) {
    switch (parameter) {
    case MOTOR_TUNING_SENSITIVITY:
        encode_u8(write, 0x20, values->sensitivity);
        return 1;
    case MOTOR_TUNING_FORCE_FEEDBACK_STRENGTH:
        encode_u8(write, 0x21, clamp(values->force_feedback_strength, 100));
        return 1;
    case MOTOR_TUNING_FORCE_FEEDBACK_SCALE:
        encode_u8(write, 0x22,
                  values->linear_force_mode != 0 || context->calibration_active != 0 ? 0xaa : 0);
        return 1;
    case MOTOR_TUNING_NATURAL_DAMPER:
        encode_u8(write, 0x23, scale_percent_to_u8(values->natural_damper));
        return 1;
    case MOTOR_TUNING_NATURAL_FRICTION:
        encode_u16(write, 0x24, scale_friction(values, context));
        return 1;
    case MOTOR_TUNING_NATURAL_INERTIA:
        encode_u8(write, 0x25, scale_percent_to_u8(values->natural_inertia));
        return 1;
    case MOTOR_TUNING_INTERPOLATION_FILTER:
        encode_u8(write, 0x26, encode_interpolation_filter(values, context));
        return 1;
    case MOTOR_TUNING_FORCE_EFFECT_INTENSITY:
        encode_u8(write, 0x27, clamp(values->force_effect_intensity, 100));
        return 1;
    case MOTOR_TUNING_FORCE_EFFECT_STRENGTH:
        encode_u8(write, 0x28, clamp(values->force_effect_strength, 12));
        return 1;
    case MOTOR_TUNING_SPRING_EFFECT_STRENGTH:
        encode_u8(write, 0x29, clamp(values->spring_effect_strength, 12));
        return 1;
    case MOTOR_TUNING_DAMPER_EFFECT_STRENGTH:
        encode_u8(write, 0x2a, clamp(values->damper_effect_strength, 12));
        return 1;
    }
    return 0;
}
