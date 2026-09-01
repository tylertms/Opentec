#include <assert.h>
#include <stdint.h>

#include "force_feedback/command.h"
#include "force_feedback/effect.h"
#include "force_feedback/engine.h"
#include "force_feedback/soft_stop.h"

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

static void test_parameter_settings(void) {
    MotorForceFeedbackSettings settings = motor_force_feedback_settings_default();
    motor_force_feedback_settings_apply(&settings, -19, 80U, 90U, 9U, 8U, 7U);
    assert(settings.position_half_range == 0x8ac0);
    assert(settings.overall_gain_percent == 80U);
    assert(settings.filter_setting == 90U);
    assert(settings.constant_gain_tenths == 9U);
    assert(settings.window_gain_tenths == 8U);
    assert(settings.directional_gain_tenths == 7U);

    motor_force_feedback_settings_apply(&settings, 126, 101U, 101U, 13U, 13U, 13U);
    assert(settings.position_half_range == 82880);
    assert(settings.overall_gain_percent == 80U);
    assert(settings.filter_setting == 90U);
    assert(settings.constant_gain_tenths == 9U);
    assert(settings.window_gain_tenths == 8U);
    assert(settings.directional_gain_tenths == 7U);

    motor_force_feedback_settings_apply(&settings, 127, 100U, 100U, 12U, 12U, 12U);
    assert(settings.position_half_range == 82880);
    assert(settings.overall_gain_percent == 100U);
    assert(settings.filter_setting == 100U);
    assert(settings.constant_gain_tenths == 12U);
    assert(settings.window_gain_tenths == 12U);
    assert(settings.directional_gain_tenths == 12U);
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
    assert(motor_force_feedback_filter_length(20U) == 30U);
    assert(motor_force_feedback_filter_length(30U) == 25U);
    assert(motor_force_feedback_filter_length(40U) == 20U);
    assert(motor_force_feedback_filter_length(50U) == 15U);
    assert(motor_force_feedback_filter_length(60U) == 10U);
    assert(motor_force_feedback_filter_length(70U) == 7U);
    assert(motor_force_feedback_filter_length(80U) == 4U);
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

static void test_soft_stop(void) {
    MotorForceFeedbackSoftStop soft_stop = {0};
    int32_t force = 100;
    assert(
        !motor_force_feedback_soft_stop_apply(&soft_stop, 1U, 35520, 0, 0, 6577U, false, &force));
    assert(force == 100);
    assert(soft_stop.ramp_percent == 1U);
    assert(soft_stop.next_ramp_tick == 51U);

    force = 0;
    assert(motor_force_feedback_soft_stop_apply(&soft_stop, 52U, 35520, 0, 42097, 6577U, false,
                                                &force));
    assert(force == -1310);
    assert(soft_stop.ramp_percent == 2U);

    force = 0;
    assert(motor_force_feedback_soft_stop_apply(&soft_stop, 103U, 35520, 0, -42097, 6577U, false,
                                                &force));
    assert(force == 1966);

    force = 4321;
    assert(!motor_force_feedback_soft_stop_apply(&soft_stop, 104U, 35520, 0, 50000, 6577U, true,
                                                 &force));
    assert(force == 4321);

    soft_stop.previous_half_range = 36001;
    soft_stop.ramp_percent = 100U;
    soft_stop.next_ramp_tick = 100U;
    force = 0;
    assert(motor_force_feedback_soft_stop_apply(&soft_stop, 200U, 35520, 0, 42097, 6577U, false,
                                                &force));
    assert(soft_stop.ramp_percent == 1U);
    assert(force == -655);

    soft_stop.ramp_percent = 0U;
    soft_stop.next_ramp_tick = UINT32_MAX - 10U;
    force = 0;
    assert(motor_force_feedback_soft_stop_apply(&soft_stop, 40U, 35520, 0, 42097, 6577U, false,
                                                &force));
    assert(soft_stop.ramp_percent == 1U);
    assert(soft_stop.next_ramp_tick == 90U);
}

static void test_engine(void) {
    MotorForceFeedbackEngine engine;
    motor_force_feedback_engine_initialize(&engine);
    assert(engine.settings.position_half_range == 0x8ac0);
    assert(engine.effects[MOTOR_FORCE_FEEDBACK_POSITION_SLOT].active);
    assert(engine.effects[MOTOR_FORCE_FEEDBACK_POSITION_SLOT].type ==
           MOTOR_FORCE_FEEDBACK_EFFECT_WINDOW);
    assert(engine.effects[MOTOR_FORCE_FEEDBACK_DAMPER_SLOT].active);
    assert(engine.effects[MOTOR_FORCE_FEEDBACK_DAMPER_SLOT].type ==
           MOTOR_FORCE_FEEDBACK_EFFECT_DIRECTIONAL);
    assert(engine.soft_stop_transition_range == 0x19b1U);
    assert(engine.ramp_percent == 100U);

    MotorForceFeedbackMix mix = motor_force_feedback_mix(&engine, 1U, 0, 0, 0, 0, false);
    assert(mix.primary.positive);
    assert(mix.primary.magnitude == 0U);
    assert(mix.secondary == 0);
    assert(!engine.effects[MOTOR_FORCE_FEEDBACK_DAMPER_SLOT].active);

    const uint8_t constant_payload[5] = {0U, 0U, 0U, 0U, 0U};
    assert(motor_force_feedback_constant_configure(&engine, 0U, constant_payload));
    assert(motor_force_feedback_effect_enable(&engine, 0U));
    mix = motor_force_feedback_mix(&engine, 2U, 0, 0, 0, 0, false);
    assert(mix.primary.positive);
    assert(mix.primary.magnitude == 22937U);
    assert(mix.secondary == 0);

    engine.ramp_percent = 50U;
    mix = motor_force_feedback_mix(&engine, 3U, 0, 0, 0, 0, false);
    assert(mix.primary.magnitude == 11468U);
    assert(motor_force_feedback_effect_disable(&engine, 0U));
    assert(!motor_force_feedback_effect_disable(&engine, MOTOR_FORCE_FEEDBACK_EFFECT_COUNT));
    assert(!motor_force_feedback_constant_configure(&engine, MOTOR_FORCE_FEEDBACK_EFFECT_COUNT,
                                                    constant_payload));
    assert(!motor_force_feedback_window_configure(&engine, MOTOR_FORCE_FEEDBACK_EFFECT_COUNT,
                                                  constant_payload));
    assert(!motor_force_feedback_directional_configure(&engine, MOTOR_FORCE_FEEDBACK_EFFECT_COUNT,
                                                       constant_payload));
    assert(!motor_force_feedback_effect_enable(&engine, MOTOR_FORCE_FEEDBACK_EFFECT_COUNT));

    engine.ramp_percent = 100U;
    mix = motor_force_feedback_mix(&engine, 4U, 0, 0, 0, 320, false);
    assert(mix.primary.magnitude == 3U);
    assert(mix.secondary == 0);

    motor_force_feedback_engine_initialize(&engine);
    engine.settings.overall_gain_percent = 100U;
    engine.settings.filter_setting = 100U;
    for (uint8_t slot = 0U; slot < MOTOR_FORCE_FEEDBACK_EFFECT_COUNT; ++slot) {
        assert(motor_force_feedback_constant_configure(&engine, slot, constant_payload));
        assert(motor_force_feedback_effect_enable(&engine, slot));
    }
    mix = motor_force_feedback_mix(&engine, 5U, 0, 0, 0, 0, true);
    assert(mix.primary.positive);
    assert(mix.primary.magnitude == UINT16_MAX);

    const uint8_t negative_payload[5] = {0xffU, 0U, 0U, 0U, 0U};
    for (uint8_t slot = 0U; slot < MOTOR_FORCE_FEEDBACK_EFFECT_COUNT; ++slot)
        assert(motor_force_feedback_constant_configure(&engine, slot, negative_payload));
    mix = motor_force_feedback_mix(&engine, 6U, 0, 0, 0, 0, true);
    assert(!mix.primary.positive);
    assert(mix.primary.magnitude == UINT16_MAX);

    engine.settings.overall_gain_percent = 0U;
    mix = motor_force_feedback_mix(&engine, 7U, 0, 0, 0, 0, true);
    assert(mix.primary.magnitude == 0U);

    motor_force_feedback_engine_initialize(&engine);
    const uint8_t directional_payload[5] = {1U, 0U, 1U, 0U, 0xa0U};
    for (uint8_t slot = 0U; slot < MOTOR_FORCE_FEEDBACK_EFFECT_COUNT; ++slot) {
        assert(motor_force_feedback_directional_configure(&engine, slot, directional_payload));
        assert(motor_force_feedback_effect_enable(&engine, slot));
    }
    mix = motor_force_feedback_mix(&engine, 8U, 0, 0, 0, 1000000, true);
    assert(mix.secondary == -32767);
    mix = motor_force_feedback_mix(&engine, 9U, 0, 0, 0, -1000000, true);
    assert(mix.secondary == 32767);

    motor_force_feedback_engine_initialize(&engine);
    for (uint8_t slot = 0U; slot < MOTOR_FORCE_FEEDBACK_EFFECT_COUNT; ++slot) {
        engine.effects[slot].active = false;
    }
    engine.settings.position_half_range = 82880;
    engine.settings.filter_setting = 100U;
    engine.soft_stop.ramp_percent = 100U;
    mix = motor_force_feedback_mix(&engine, 10U, 82880, 1000, 90457, 0, false);
    assert(!mix.primary.positive);
    assert(mix.primary.magnitude == UINT16_MAX);
}

static void test_commands(void) {
    MotorForceFeedbackEngine engine;
    motor_force_feedback_engine_initialize(&engine);

    uint8_t command[7] = {0x21U, 8U, 0U, 0U, 0U, 0U, 0U};
    assert(motor_force_feedback_command_apply(&engine, command));
    assert(engine.effects[2].active);
    assert(engine.effects[2].type == MOTOR_FORCE_FEEDBACK_EFFECT_CONSTANT);
    assert(engine.effects[2].data.constant.magnitude == 65535);

    command[0] = 0x23U;
    assert(motor_force_feedback_command_apply(&engine, command));
    assert(!engine.effects[2].active);
    command[0] = 0x21U;
    command[1] = MOTOR_FORCE_FEEDBACK_EFFECT_NONE;
    assert(motor_force_feedback_command_apply(&engine, command));
    assert(engine.effects[2].active);
    assert(engine.effects[2].type == MOTOR_FORCE_FEEDBACK_EFFECT_CONSTANT);

    command[0] = 0x05U;
    assert(motor_force_feedback_command_apply(&engine, command));
    assert(!engine.effects[MOTOR_FORCE_FEEDBACK_POSITION_SLOT].active);
    command[0] = 0x04U;
    assert(motor_force_feedback_command_apply(&engine, command));
    assert(engine.effects[MOTOR_FORCE_FEEDBACK_POSITION_SLOT].active);

    command[0] = 0x11U;
    command[1] = 7U;
    assert(!motor_force_feedback_command_apply(&engine, command));

    command[0] = 0x11U;
    command[1] = MOTOR_FORCE_FEEDBACK_EFFECT_WINDOW;
    assert(motor_force_feedback_command_apply(&engine, command));
    command[1] = MOTOR_FORCE_FEEDBACK_EFFECT_DIRECTIONAL;
    assert(motor_force_feedback_command_apply(&engine, command));

    command[0] = 0x12U;
    assert(motor_force_feedback_command_apply(&engine, command));
}

static void test_window_range_change(void) {
    MotorForceFeedbackEngine engine;
    motor_force_feedback_engine_initialize(&engine);
    const uint8_t payload[5] = {0x40U, 0xc0U, 0x44U, 0U, 0xffU};
    assert(motor_force_feedback_window_configure(&engine, 2U, payload));
    assert(motor_force_feedback_effect_enable(&engine, 2U));
    int32_t lower = engine.effects[2].data.window.lower_position;
    int32_t upper = engine.effects[2].data.window.upper_position;

    motor_force_feedback_settings_apply(&engine.settings, 126, 35U, 100U, 10U, 10U, 10U);
    assert(engine.settings.position_half_range == 82880);
    assert(engine.effects[2].data.window.lower_position == lower);
    assert(engine.effects[2].data.window.upper_position == upper);
    assert(engine.effects[2].active);
    assert(engine.effects[MOTOR_FORCE_FEEDBACK_POSITION_SLOT].data.window.lower_position == 0);
    assert(engine.effects[MOTOR_FORCE_FEEDBACK_POSITION_SLOT].data.window.upper_position == 0);
}

static void test_directional_reconfiguration(void) {
    const uint8_t directional_payload[5] = {1U, 0U, 1U, 0U, 0xa0U};
    const uint8_t window_payloads[2][5] = {
        {0x40U, 0xc0U, 0x44U, 0U, 0xffU},
        {0x40U, 0xc0U, 0x44U, 0x11U, 0xffU},
    };
    const int8_t markers[2] = {-1, 1};

    for (uint8_t slot = 0U; slot < 16U; ++slot) {
        for (uint8_t marker = 0U; marker < 2U; ++marker) {
            MotorForceFeedbackEngine engine;
            motor_force_feedback_engine_initialize(&engine);
            assert(motor_force_feedback_window_configure(&engine, slot, window_payloads[marker]));
            assert(motor_force_feedback_directional_configure(&engine, slot, directional_payload));
            assert(engine.effects[slot].data.window.lower_direction == markers[marker]);
            assert(engine.effects[slot].data.window.upper_direction == markers[marker]);
            assert(engine.effects[slot].data.directional.steering_scaled);
            assert(motor_force_feedback_directional_evaluate(&engine.effects[slot].data.directional,
                                                             320, 10U, 35U, slot) == -20);
        }
    }

    MotorForceFeedbackEngine engine;
    motor_force_feedback_engine_initialize(&engine);
    assert(motor_force_feedback_directional_configure(&engine, 0U, directional_payload));
    assert(!engine.effects[0].data.directional.steering_scaled);
    assert(motor_force_feedback_directional_evaluate(&engine.effects[0].data.directional, 320, 10U,
                                                     35U, 0U) == -10);
}

int motor_test_force_feedback(void) {
    test_defaults();
    test_parameter_settings();
    test_constant_effects();
    test_window_effects();
    test_directional_effects();
    test_filter();
    test_output();
    test_soft_stop();
    test_engine();
    test_commands();
    test_window_range_change();
    test_directional_reconfiguration();
    return 0;
}
