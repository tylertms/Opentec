#include "common/motor/force_feedback.h"

#include <assert.h>
#include <stdint.h>

static void test_defaults(void) {
    MotorForceFeedbackSettings settings = motor_force_feedback_settings_default();
    assert(settings.position_half_range == 0x8ac0);
    assert(settings.overall_gain_percent == 35U);
    assert(settings.filter_setting == 100U);
    assert(settings.constant_gain_tenths == 10U);
    assert(settings.window_gain_tenths == 10U);
    assert(settings.directional_gain_tenths == 10U);
    assert(settings.window_multiplier == 1U);
}

static void test_constant_effects(void) {
    uint8_t payload[5] = {0x80U, 0U, 0U, 0U, 0U};
    MotorConstantEffect effect = motor_force_feedback_constant_decode(payload);
    assert(effect.magnitude == 0);

    payload[0] = 0U;
    effect = motor_force_feedback_constant_decode(payload);
    assert(effect.magnitude == 65535);

    payload[0] = 0x7fU;
    effect = motor_force_feedback_constant_decode(payload);
    assert(effect.magnitude == 257);

    payload[0] = 0xffU;
    effect = motor_force_feedback_constant_decode(payload);
    assert(effect.magnitude == -65535);
    assert(motor_force_feedback_constant_evaluate(&effect, 10U) == -65535);
    assert(motor_force_feedback_constant_evaluate(&effect, 5U) == -32767);
    assert(motor_force_feedback_constant_evaluate(&effect, 0U) == 0);

    payload[0] = 0U;
    payload[1] = 0x80U;
    payload[4] = 1U;
    effect = motor_force_feedback_constant_decode(payload);
    assert(effect.magnitude == -1);

    payload[4] = 2U;
    effect = motor_force_feedback_constant_decode(payload);
    assert(effect.magnitude == 0);
}

static void test_window_effects(void) {
    const uint8_t centered_payload[5] = {0x80U, 0x80U, 0x44U, 0U, 0xffU};
    MotorWindowEffect effect = motor_force_feedback_window_decode(centered_payload, 0x8ac0);
    assert(effect.lower_position == 0);
    assert(effect.upper_position == 0);
    assert(effect.lower_coefficient == 4U);
    assert(effect.upper_coefficient == 4U);
    assert(effect.lower_direction == -1);
    assert(effect.upper_direction == -1);
    assert(effect.saturation == 65535U);

    const MotorDirectionalEffect compensation = {
        .positive_coefficient = 1U,
        .negative_coefficient = 1U,
        .positive_direction = 1,
        .negative_direction = 1,
        .saturation = 65535U,
    };
    assert(motor_force_feedback_window_evaluate(&effect, 100, 0, 1U, 10U, 16U, &compensation, 10U,
                                                35U) == -80);
    assert(motor_force_feedback_window_evaluate(&effect, -100, 0, 1U, 10U, 0U, &compensation, 10U,
                                                35U) == 80);

    const uint8_t asymmetric_payload[5] = {0U, 0xffU, 0xa3U, 0x11U, 0x80U};
    effect = motor_force_feedback_window_decode(asymmetric_payload, 0x8ac0);
    assert(effect.lower_position == -35520);
    assert(effect.upper_position == 35242);
    assert(effect.lower_coefficient == 10U);
    assert(effect.upper_coefficient == 3U);
    assert(effect.lower_direction == 1);
    assert(effect.upper_direction == 1);
    assert(effect.saturation == 0x8080U);
}

static void test_directional_effects(void) {
    const uint8_t payload[5] = {1U, 0U, 1U, 0U, 0xa0U};
    MotorDirectionalEffect effect = motor_force_feedback_directional_decode(payload);
    assert(effect.positive_coefficient == 1U);
    assert(effect.negative_coefficient == 1U);
    assert(effect.positive_direction == -1);
    assert(effect.negative_direction == -1);
    assert(effect.saturation == 0xa0a0U);
    assert(!effect.steering_scaled);

    assert(motor_force_feedback_directional_evaluate(&effect, 320, 10U, 35U, 0U) == -10);
    assert(motor_force_feedback_directional_evaluate(&effect, -320, 10U, 35U, 0U) == 10);
    assert(motor_force_feedback_directional_evaluate(&effect, 320, 0U, 35U, 0U) == 0);
    assert(motor_force_feedback_directional_evaluate(&effect, 320, 10U, 35U, 18U) == -20);

    effect.steering_scaled = true;
    assert(motor_force_feedback_directional_evaluate(&effect, 320, 10U, 35U, 0U) == -20);
}

static void test_filter(void) {
    assert(motor_force_feedback_filter_length(0U) == 40U);
    assert(motor_force_feedback_filter_length(10U) == 35U);
    assert(motor_force_feedback_filter_length(50U) == 15U);
    assert(motor_force_feedback_filter_length(70U) == 7U);
    assert(motor_force_feedback_filter_length(90U) == 2U);
    assert(motor_force_feedback_filter_length(100U) == 1U);
    assert(motor_force_feedback_filter_length(99U) == 1U);

    MotorForceFeedbackFilter filter = {0};
    motor_force_feedback_filter_configure(&filter, 90U);
    assert(motor_force_feedback_filter_apply(&filter, 100) == 50);
    assert(motor_force_feedback_filter_apply(&filter, 200) == 150);
    assert(motor_force_feedback_filter_apply(&filter, -100) == 50);
    motor_force_feedback_filter_configure(&filter, 0U);
    assert(motor_force_feedback_filter_apply(&filter, 400) == 10);
}

static void test_output(void) {
    MotorForceFeedbackOutput output = motor_force_feedback_output_resolve(-70000);
    assert(!output.positive);
    assert(output.magnitude == 65535U);
    output = motor_force_feedback_output_resolve(1234);
    assert(output.positive);
    assert(output.magnitude == 1234U);
}

int main(void) {
    test_defaults();
    test_constant_effects();
    test_window_effects();
    test_directional_effects();
    test_filter();
    test_output();
    return 0;
}
