#include "board/led_pattern.h"

#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

static void test_maps_every_pattern_bucket(void) {
    static const uint16_t expected[] = {
        0,   1,   1,   2,   2,   2,   2,   2,   3,   3,   3,   4,   4,   5,   5,   6,
        6,   7,   8,   9,   10,  11,  12,  13,  15,  17,  19,  21,  23,  26,  29,  32,
        36,  40,  44,  49,  55,  61,  68,  76,  85,  94,  105, 117, 131, 146, 162, 181,
        202, 225, 250, 279, 311, 346, 386, 430, 479, 534, 595, 663, 739, 824, 918, 1023,
    };

    for (size_t bucket = 0; bucket < sizeof(expected) / sizeof(expected[0]); bucket++) {
        for (uint8_t offset = 0; offset < 4; offset++) {
            assert(led_pattern_pwm_duty((uint8_t)(bucket * 4 + offset)) == expected[bucket]);
        }
    }
}

static LedPatternControllerInput controller_input(void) { return (LedPatternControllerInput){0}; }

static void test_builds_startup_sweep(void) {
    for (uint8_t step = 0; step < LED_PATTERN_STARTUP_STEP_COUNT; ++step) {
        assert(led_pattern_startup_pattern(step) == (uint8_t)(step * 4));
    }
    assert(led_pattern_startup_pattern(LED_PATTERN_STARTUP_STEP_COUNT - 1) == 0xf8);
}

static void test_starts_normal_output_at_full_brightness(void) {
    LedPatternController controller;
    led_pattern_controller_init(&controller);

    assert(led_pattern_controller_update(&controller, controller_input(), 100) == 0xff);
    assert(led_pattern_controller_update(&controller, controller_input(), 101) ==
           LED_PATTERN_NO_UPDATE);
}

static void test_runs_inhibited_heartbeat(void) {
    LedPatternController controller;
    led_pattern_controller_init(&controller);
    LedPatternControllerInput input = controller_input();
    input.output_inhibited = true;

    assert(led_pattern_controller_update(&controller, input, 100) == 0x00);
    assert(led_pattern_controller_update(&controller, input, 350) == LED_PATTERN_NO_UPDATE);
    assert(led_pattern_controller_update(&controller, input, 351) == LED_PATTERN_NO_UPDATE);
    assert(led_pattern_controller_update(&controller, input, 352) == 0xff);
    assert(led_pattern_controller_update(&controller, input, 602) == LED_PATTERN_NO_UPDATE);
    assert(led_pattern_controller_update(&controller, input, 603) == LED_PATTERN_NO_UPDATE);
    assert(led_pattern_controller_update(&controller, input, 604) == 0x00);
}

static void test_heartbeat_resumes_after_normal_output(void) {
    LedPatternController controller;
    led_pattern_controller_init(&controller);
    LedPatternControllerInput input = controller_input();
    input.output_inhibited = true;

    assert(led_pattern_controller_update(&controller, input, 10) == 0x00);
    input.output_inhibited = false;
    assert(led_pattern_controller_update(&controller, input, 20) == 0xff);
    input.output_inhibited = true;
    assert(led_pattern_controller_update(&controller, input, 261) == LED_PATTERN_NO_UPDATE);
    assert(led_pattern_controller_update(&controller, input, 262) == 0xff);
}

static void test_shutdown_forces_output_off(void) {
    LedPatternController controller;
    led_pattern_controller_init(&controller);
    LedPatternControllerInput input = controller_input();
    input.shutdown_complete = true;

    assert(led_pattern_controller_update(&controller, input, 0) == 0x00);
    assert(led_pattern_controller_update(&controller, input, 1) == 0x00);
}

static void wait_for_next_breath_step(LedPatternController *controller,
                                      LedPatternControllerInput input, uint32_t *now_ms) {
    *now_ms += 4;
    assert(led_pattern_controller_update(controller, input, *now_ms) == LED_PATTERN_NO_UPDATE);
    ++*now_ms;
    assert(led_pattern_controller_update(controller, input, *now_ms) == LED_PATTERN_NO_UPDATE);
    ++*now_ms;
}

static void test_breathes_while_pedal_handshake_is_active(void) {
    LedPatternController controller;
    led_pattern_controller_init(&controller);
    LedPatternControllerInput input = controller_input();
    input.pedal_handshake_active = true;
    uint32_t now_ms = 0;

    assert(led_pattern_controller_update(&controller, controller_input(), now_ms++) == 0xff);
    assert(led_pattern_controller_update(&controller, input, now_ms) == 0xfe);
    for (uint8_t pattern = 0xfd; pattern >= 0x80; --pattern) {
        wait_for_next_breath_step(&controller, input, &now_ms);
        assert(led_pattern_controller_update(&controller, input, now_ms) == pattern);
    }

    wait_for_next_breath_step(&controller, input, &now_ms);
    assert(led_pattern_controller_update(&controller, input, now_ms) == 0x80);
    assert(controller.breath_phase == LED_PATTERN_BREATH_BRIGHTEN);

    for (uint16_t pattern = 0x81; pattern <= 0xff; ++pattern) {
        assert(led_pattern_controller_update(&controller, input, now_ms) == pattern);
        if (pattern != 0xff) {
            wait_for_next_breath_step(&controller, input, &now_ms);
        }
    }
    wait_for_next_breath_step(&controller, input, &now_ms);
    assert(led_pattern_controller_update(&controller, input, now_ms) == 0xff);
    assert(controller.breath_phase == LED_PATTERN_BREATH_DARKEN);
}

static void test_completed_breath_stops_after_request_clears(void) {
    LedPatternController controller;
    led_pattern_controller_init(&controller);
    LedPatternControllerInput input = controller_input();
    input.force_override_requested = true;
    uint32_t now_ms = 0;

    assert(led_pattern_controller_update(&controller, controller_input(), now_ms++) == 0xff);
    assert(led_pattern_controller_update(&controller, input, now_ms) == 0xfe);
    input.force_override_requested = false;

    while (controller.transition_active) {
        uint16_t pattern = led_pattern_controller_update(&controller, input, ++now_ms);
        assert(pattern == LED_PATTERN_NO_UPDATE || pattern <= 0xff);
    }
    assert(controller.current_pattern == 0xff);
    assert(controller.breath_phase == LED_PATTERN_BREATH_IDLE);
    assert(led_pattern_controller_update(&controller, input, ++now_ms) == LED_PATTERN_NO_UPDATE);
}

static void test_deadlines_survive_counter_wraparound(void) {
    LedPatternController controller;
    led_pattern_controller_init(&controller);
    LedPatternControllerInput input = controller_input();
    input.output_inhibited = true;

    assert(led_pattern_controller_update(&controller, input, UINT32_MAX - 100) == 0x00);
    assert(led_pattern_controller_update(&controller, input, 149) == LED_PATTERN_NO_UPDATE);
    assert(led_pattern_controller_update(&controller, input, 150) == LED_PATTERN_NO_UPDATE);
    assert(led_pattern_controller_update(&controller, input, 151) == 0xff);
}

int main(void) {
    test_maps_every_pattern_bucket();
    test_builds_startup_sweep();
    test_starts_normal_output_at_full_brightness();
    test_runs_inhibited_heartbeat();
    test_heartbeat_resumes_after_normal_output();
    test_shutdown_forces_output_off();
    test_breathes_while_pedal_handshake_is_active();
    test_completed_breath_stops_after_request_clears();
    test_deadlines_survive_counter_wraparound();
    return 0;
}
