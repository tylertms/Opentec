#include <assert.h>
#include <stdint.h>

#include "motor/tuning_parameter.h"

static TuningProfile default_profile(void) {
    TuningProfile profile;
    tuning_profile_defaults(&profile);
    profile.force_feedback_strength = 75;
    profile.force_scale = TUNING_FORCE_SCALE_LINEAR;
    profile.natural_friction = 40;
    profile.natural_inertia = 25;
    profile.force_effect_intensity = 80;
    profile.spring_effect_strength = 11;
    profile.damper_effect_strength = 12;
    return profile;
}

static MotorTuningContext default_context(void) {
    MotorTuningContext context = {
        .automatic_rotation_degrees = 1080,
        .ramp_percent = 100,
        .strength_percent = 100,
        .xbox_mode = 0,
        .calibration_active = 0,
        .extended_parameters = 1,
    };
    return context;
}

static MotorParameterWrite encode(MotorTuningParameter parameter, TuningProfile *profile,
                                  MotorTuningContext *context) {
    MotorParameterWrite write;
    assert(motor_tuning_parameter_encode(parameter, profile, context, &write) == 1);
    return write;
}

static uint8_t encode_value(MotorTuningParameter parameter, TuningProfile *profile,
                            MotorTuningContext *context) {
    MotorParameterWrite write = encode(parameter, profile, context);
    return write.data[0];
}

static void test_parameter_addresses(void) {
    TuningProfile profile = default_profile();
    MotorTuningContext context = default_context();
    static const struct {
        MotorTuningParameter parameter;
        uint8_t address;
        uint8_t value;
    } cases[] = {
        {MOTOR_TUNING_SENSITIVITY, 0x20, 0xed},
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
        MotorParameterWrite write = encode(cases[index].parameter, &profile, &context);
        assert(write.address == cases[index].address);
        assert(write.length == 1);
        assert(write.data[0] == cases[index].value);
        assert(write.data[1] == 0);
    }
}

static void test_friction_scaling(void) {
    TuningProfile profile = default_profile();
    MotorTuningContext context = default_context();

    MotorParameterWrite write = encode(MOTOR_TUNING_NATURAL_FRICTION, &profile, &context);
    assert(write.address == 0x24);
    assert(write.length == 2);
    assert(write.data[0] == 0x66);
    assert(write.data[1] == 0x66);

    context.ramp_percent = 50;
    context.strength_percent = 50;
    write = encode(MOTOR_TUNING_NATURAL_FRICTION, &profile, &context);
    assert(write.data[0] == 0x99);
    assert(write.data[1] == 0x19);
}

static void test_mode_dependent_parameters(void) {
    TuningProfile profile = default_profile();
    MotorTuningContext context = default_context();

    profile.force_scale = TUNING_FORCE_SCALE_PEAK;
    assert(encode_value(MOTOR_TUNING_FORCE_FEEDBACK_SCALE, &profile, &context) == 0);
    context.calibration_active = 1;
    assert(encode_value(MOTOR_TUNING_FORCE_FEEDBACK_SCALE, &profile, &context) == 0xaa);

    context.calibration_active = 0;
    context.xbox_mode = 1;
    assert(encode_value(MOTOR_TUNING_INTERPOLATION_FILTER, &profile, &context) == 4);
    profile.interpolation_filter = 10;
    assert(encode_value(MOTOR_TUNING_INTERPOLATION_FILTER, &profile, &context) == 10);
}

static void test_extended_parameters_require_controller_support(void) {
    TuningProfile profile = default_profile();
    MotorTuningContext context = default_context();
    MotorParameterWrite write;

    context.extended_parameters = 0;
    assert(!motor_tuning_parameter_encode(MOTOR_TUNING_FORCE_FEEDBACK_SCALE, &profile, &context,
                                          &write));
    assert(
        !motor_tuning_parameter_encode(MOTOR_TUNING_NATURAL_INERTIA, &profile, &context, &write));
    assert(!motor_tuning_parameter_encode(MOTOR_TUNING_INTERPOLATION_FILTER, &profile, &context,
                                          &write));
    assert(motor_tuning_parameter_encode(MOTOR_TUNING_NATURAL_DAMPER, &profile, &context, &write));
}

static void test_rotation_encoding(void) {
    TuningProfile profile = default_profile();
    MotorTuningContext context = default_context();

    profile.automatic_rotation = 0;
    profile.rotation_degrees = 90;
    assert(encode_value(MOTOR_TUNING_SENSITIVITY, &profile, &context) == (uint8_t)-118);
    profile.rotation_degrees = 990;
    assert(encode_value(MOTOR_TUNING_SENSITIVITY, &profile, &context) == (uint8_t)-28);
    profile.rotation_degrees = 1000;
    assert(encode_value(MOTOR_TUNING_SENSITIVITY, &profile, &context) == (uint8_t)-27);
    profile.rotation_degrees = 2520;
    assert(encode_value(MOTOR_TUNING_SENSITIVITY, &profile, &context) == 125);
}

static void test_limits(void) {
    TuningProfile profile = default_profile();
    MotorTuningContext context = default_context();

    profile.force_feedback_strength = UINT8_MAX;
    profile.force_effect_intensity = UINT8_MAX;
    profile.force_effect_strength = UINT8_MAX;
    profile.spring_effect_strength = UINT8_MAX;
    profile.damper_effect_strength = UINT8_MAX;
    assert(encode_value(MOTOR_TUNING_FORCE_FEEDBACK_STRENGTH, &profile, &context) == UINT8_MAX);
    assert(encode_value(MOTOR_TUNING_FORCE_EFFECT_INTENSITY, &profile, &context) == 100);
    assert(encode_value(MOTOR_TUNING_FORCE_EFFECT_STRENGTH, &profile, &context) == 12);
    assert(encode_value(MOTOR_TUNING_SPRING_EFFECT_STRENGTH, &profile, &context) == 12);
    assert(encode_value(MOTOR_TUNING_DAMPER_EFFECT_STRENGTH, &profile, &context) == 12);
    assert(motor_tuning_parameter_encode((MotorTuningParameter)UINT8_MAX, &profile, &context,
                                         &(MotorParameterWrite){0}) == 0);
}

int main(void) {
    test_parameter_addresses();
    test_friction_scaling();
    test_mode_dependent_parameters();
    test_extended_parameters_require_controller_support();
    test_rotation_encoding();
    test_limits();
    return 0;
}
