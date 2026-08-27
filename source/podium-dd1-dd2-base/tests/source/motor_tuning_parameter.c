#include <assert.h>
#include <stdint.h>

#include "motor/tuning_parameter.h"

static MotorTuningValues default_values(void) {
    MotorTuningValues values = {
        .sensitivity = 0x83,
        .force_feedback_strength = 75,
        .linear_force_mode = 1,
        .natural_damper = 50,
        .natural_friction = 40,
        .natural_inertia = 25,
        .interpolation_filter = 6,
        .force_effect_intensity = 80,
        .force_effect_strength = 10,
        .spring_effect_strength = 11,
        .damper_effect_strength = 12,
    };
    return values;
}

static MotorTuningContext default_context(void) {
    MotorTuningContext context = {
        .ramp_percent = 100,
        .strength_percent = 100,
        .xbox_mode = 0,
        .calibration_active = 0,
    };
    return context;
}

static MotorParameterWrite encode(MotorTuningParameter parameter, MotorTuningValues *values,
                                  MotorTuningContext *context) {
    MotorParameterWrite write;
    assert(motor_tuning_parameter_encode(parameter, values, context, &write) == 1);
    return write;
}

static uint8_t encode_value(MotorTuningParameter parameter, MotorTuningValues *values,
                            MotorTuningContext *context) {
    MotorParameterWrite write = encode(parameter, values, context);
    return write.data[0];
}

static void test_parameter_addresses(void) {
    MotorTuningValues values = default_values();
    MotorTuningContext context = default_context();
    static const struct {
        MotorTuningParameter parameter;
        uint8_t address;
        uint8_t value;
    } cases[] = {
        {MOTOR_TUNING_SENSITIVITY, 0x20, 0x83},
        {MOTOR_TUNING_FORCE_FEEDBACK_STRENGTH, 0x21, 75},
        {MOTOR_TUNING_FORCE_FEEDBACK_SCALE, 0x22, 0xaa},
        {MOTOR_TUNING_NATURAL_DAMPER, 0x23, 127},
        {MOTOR_TUNING_NATURAL_INERTIA, 0x25, 63},
        {MOTOR_TUNING_INTERPOLATION_FILTER, 0x26, 14},
        {MOTOR_TUNING_FORCE_EFFECT_INTENSITY, 0x27, 80},
        {MOTOR_TUNING_FORCE_EFFECT_STRENGTH, 0x28, 10},
        {MOTOR_TUNING_SPRING_EFFECT_STRENGTH, 0x29, 11},
        {MOTOR_TUNING_DAMPER_EFFECT_STRENGTH, 0x2a, 12},
    };

    for (uint8_t index = 0; index < sizeof(cases) / sizeof(cases[0]); ++index) {
        MotorParameterWrite write = encode(cases[index].parameter, &values, &context);
        assert(write.address == cases[index].address);
        assert(write.length == 1);
        assert(write.data[0] == cases[index].value);
        assert(write.data[1] == 0);
    }
}

static void test_friction_scaling(void) {
    MotorTuningValues values = default_values();
    MotorTuningContext context = default_context();

    MotorParameterWrite write = encode(MOTOR_TUNING_NATURAL_FRICTION, &values, &context);
    assert(write.address == 0x24);
    assert(write.length == 2);
    assert(write.data[0] == 0x66);
    assert(write.data[1] == 0x66);

    context.ramp_percent = 50;
    context.strength_percent = 50;
    write = encode(MOTOR_TUNING_NATURAL_FRICTION, &values, &context);
    assert(write.data[0] == 0x99);
    assert(write.data[1] == 0x19);
}

static void test_mode_dependent_parameters(void) {
    MotorTuningValues values = default_values();
    MotorTuningContext context = default_context();

    values.linear_force_mode = 0;
    assert(encode_value(MOTOR_TUNING_FORCE_FEEDBACK_SCALE, &values, &context) == 0);
    context.calibration_active = 1;
    assert(encode_value(MOTOR_TUNING_FORCE_FEEDBACK_SCALE, &values, &context) == 0xaa);

    context.calibration_active = 0;
    context.xbox_mode = 1;
    assert(encode_value(MOTOR_TUNING_INTERPOLATION_FILTER, &values, &context) == 4);
    values.interpolation_filter = 10;
    assert(encode_value(MOTOR_TUNING_INTERPOLATION_FILTER, &values, &context) == 10);
}

static void test_limits(void) {
    MotorTuningValues values = default_values();
    MotorTuningContext context = default_context();

    values.force_feedback_strength = UINT8_MAX;
    values.force_effect_intensity = UINT8_MAX;
    values.force_effect_strength = UINT8_MAX;
    values.spring_effect_strength = UINT8_MAX;
    values.damper_effect_strength = UINT8_MAX;
    assert(encode_value(MOTOR_TUNING_FORCE_FEEDBACK_STRENGTH, &values, &context) == 100);
    assert(encode_value(MOTOR_TUNING_FORCE_EFFECT_INTENSITY, &values, &context) == 100);
    assert(encode_value(MOTOR_TUNING_FORCE_EFFECT_STRENGTH, &values, &context) == 12);
    assert(encode_value(MOTOR_TUNING_SPRING_EFFECT_STRENGTH, &values, &context) == 12);
    assert(encode_value(MOTOR_TUNING_DAMPER_EFFECT_STRENGTH, &values, &context) == 12);
    assert(motor_tuning_parameter_encode((MotorTuningParameter)UINT8_MAX, &values, &context,
                                         &(MotorParameterWrite){0}) == 0);
}

int main(void) {
    test_parameter_addresses();
    test_friction_scaling();
    test_mode_dependent_parameters();
    test_limits();
    return 0;
}
