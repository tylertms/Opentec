#include "board/led_pattern.h"

#include <stdbool.h>
#include <stdint.h>

/**
 * @brief LED pattern quantization and transition timing constants.
 *
 * These values define the brightness bucket width, breathing endpoints and step interval, and
 * inhibited-output heartbeat period.
 */
enum {
    LED_PATTERN_BUCKET_SHIFT = 2,      /**< Number of low pattern bits discarded for bucketing. */
    LED_PATTERN_BREATH_MINIMUM = 0x80, /**< Lowest pattern used by a breathing cycle. */
    LED_PATTERN_BREATH_MAXIMUM = 0xff, /**< Highest pattern used by a breathing cycle. */
    LED_PATTERN_BREATH_STEP_INTERVAL_MS = 4, /**< Delay between breathing steps in milliseconds. */
    LED_PATTERN_HEARTBEAT_INTERVAL_MS =
        250, /**< Delay between heartbeat changes in milliseconds. */
};

/**
 * @brief PWM duties for the 64 host-selectable brightness buckets.
 *
 * Each entry is selected by the six most-significant bits of an eight-bit LED pattern and provides
 * the corresponding ten-bit nonlinear PWM duty.
 */
static const uint16_t pwm_duties[] = {
    0,   1,   1,   2,   2,   2,   2,   2,   3,   3,   3,   4,   4,   5,   5,   6,
    6,   7,   8,   9,   10,  11,  12,  13,  15,  17,  19,  21,  23,  26,  29,  32,
    36,  40,  44,  49,  55,  61,  68,  76,  85,  94,  105, 117, 131, 146, 162, 181,
    202, 225, 250, 279, 311, 346, 386, 430, 479, 534, 595, 663, 739, 824, 918, 1023,
};

/**
 * @brief Converts a host LED pattern to its PWM duty.
 *
 * Quantizes the eight-bit pattern into 64 brightness buckets and applies the board's nonlinear
 * duty curve. The resulting duty spans zero through the 10-bit PWM period.
 *
 * @param[in] pattern Host-selected LED brightness pattern.
 * @return Ten-bit PWM duty for the pattern.
 */
uint16_t led_pattern_pwm_duty(uint8_t pattern) {
    return pwm_duties[pattern >> LED_PATTERN_BUCKET_SHIFT];
}

/**
 * @brief Builds one pattern in the startup brightness sweep.
 *
 * Converts a zero-based startup step into the first pattern of its corresponding PWM bucket. The
 * 63-step sequence covers buckets zero through 62 and leaves the final full-scale bucket for the
 * first normal service pass.
 *
 * @param[in] step Zero-based startup step below LED_PATTERN_STARTUP_STEP_COUNT.
 * @return Eight-bit pattern for the startup step.
 */
uint8_t led_pattern_startup_pattern(uint8_t step) {
    return (uint8_t)(step << LED_PATTERN_BUCKET_SHIFT);
}

/**
 * @brief Tests whether a strict millisecond deadline has elapsed.
 *
 * Uses signed modular subtraction so LED transitions remain correct when the monotonic counter
 * wraps.
 *
 * @param[in] now_ms Current monotonic time in milliseconds.
 * @param[in] deadline_ms Deadline to compare against.
 * @return True only after the deadline, not at the deadline itself.
 */
static bool deadline_passed(uint32_t now_ms, uint32_t deadline_ms) {
    return (int32_t)(now_ms - deadline_ms) > 0;
}

/**
 * @brief Moves an LED pattern one unit toward a target.
 *
 * Increments or decrements the supplied pattern by one when it differs from the target and leaves
 * it unchanged when the target was already reached.
 *
 * @param[in,out] pattern Pattern to adjust.
 * @param[in] target Desired pattern.
 * @return True when the pattern was already equal to the target; otherwise false.
 */
static bool step_toward(uint8_t *pattern, uint8_t target) {
    if (*pattern > target) {
        --*pattern;
        return false;
    }
    if (*pattern < target) {
        ++*pattern;
        return false;
    }
    return true;
}

/**
 * @brief Initializes autonomous board LED state.
 *
 * Starts the normal output, breathing transition, and inhibited-output heartbeat from their first
 * phases with a zero-valued heartbeat pattern.
 *
 * @param[out] controller LED state to initialize.
 */
void led_pattern_controller_init(LedPatternController *controller) {
    *controller = (LedPatternController){0};
}

/**
 * @brief Services the inhibited-output heartbeat.
 *
 * Alternates between zero and full-scale patterns on successive 250 ms intervals. Expiring an
 * interval arms the next write, so the new pattern is returned on the following service pass.
 *
 * @param[in,out] controller Persistent heartbeat pattern, phase, and deadline.
 * @param[in] now_ms Current monotonic time in milliseconds.
 * @return A new pattern or LED_PATTERN_NO_UPDATE when the retained output must remain unchanged.
 */
static uint16_t update_heartbeat(LedPatternController *controller, uint32_t now_ms) {
    if (!controller->heartbeat_waiting) {
        uint8_t pattern = controller->heartbeat_pattern;
        controller->heartbeat_pattern = (uint8_t)~pattern;
        controller->heartbeat_deadline_ms = now_ms + LED_PATTERN_HEARTBEAT_INTERVAL_MS;
        controller->heartbeat_waiting = true;
        return pattern;
    }
    if (deadline_passed(now_ms, controller->heartbeat_deadline_ms)) {
        controller->heartbeat_waiting = false;
    }
    return LED_PATTERN_NO_UPDATE;
}

/**
 * @brief Services a requested breathing transition.
 *
 * Moves one pattern unit at a time between full brightness and pattern 0x80. Each successful step
 * is followed by a strict four-millisecond wait. A completed upward transition stops when neither
 * the pedal handshake nor alternate runtime remains active; a force override can start a new
 * transition but does not keep the completed cycle active.
 *
 * @param[in,out] controller Persistent breathing phase, pattern, and deadline.
 * @param[in] input Current sources that request or sustain the transition.
 * @param[in] now_ms Current monotonic time in milliseconds.
 * @return A new pattern or LED_PATTERN_NO_UPDATE when the retained output must remain unchanged.
 */
static uint16_t update_breathing(LedPatternController *controller, LedPatternControllerInput input,
                                 uint32_t now_ms) {
    if (!input.pedal_handshake_active && !input.alternate_runtime_active &&
        !input.force_override_requested && !controller->transition_active) {
        return LED_PATTERN_NO_UPDATE;
    }
    controller->transition_active = true;

    if (controller->breath_phase == LED_PATTERN_BREATH_IDLE) {
        controller->current_pattern = LED_PATTERN_BREATH_MAXIMUM;
        controller->breath_phase = LED_PATTERN_BREATH_DARKEN;
    }
    if (controller->breath_phase == LED_PATTERN_BREATH_DARKEN) {
        if (step_toward(&controller->current_pattern, LED_PATTERN_BREATH_MINIMUM)) {
            controller->breath_phase = LED_PATTERN_BREATH_BRIGHTEN;
        } else {
            controller->breath_deadline_ms = now_ms + LED_PATTERN_BREATH_STEP_INTERVAL_MS;
            controller->breath_phase = LED_PATTERN_BREATH_WAIT_DARKEN;
        }
        return controller->current_pattern;
    }
    if (controller->breath_phase == LED_PATTERN_BREATH_BRIGHTEN) {
        if (step_toward(&controller->current_pattern, LED_PATTERN_BREATH_MAXIMUM)) {
            if (!input.pedal_handshake_active && !input.alternate_runtime_active) {
                controller->transition_active = false;
                controller->breath_phase = LED_PATTERN_BREATH_IDLE;
            } else {
                controller->breath_phase = LED_PATTERN_BREATH_DARKEN;
            }
        } else {
            controller->breath_deadline_ms = now_ms + LED_PATTERN_BREATH_STEP_INTERVAL_MS;
            controller->breath_phase = LED_PATTERN_BREATH_WAIT_BRIGHTEN;
        }
        return controller->current_pattern;
    }
    if (controller->breath_phase == LED_PATTERN_BREATH_WAIT_DARKEN &&
        deadline_passed(now_ms, controller->breath_deadline_ms)) {
        controller->breath_phase = LED_PATTERN_BREATH_DARKEN;
    } else if (controller->breath_phase == LED_PATTERN_BREATH_WAIT_BRIGHTEN &&
               deadline_passed(now_ms, controller->breath_deadline_ms)) {
        controller->breath_phase = LED_PATTERN_BREATH_BRIGHTEN;
    }
    return LED_PATTERN_NO_UPDATE;
}

/**
 * @brief Advances autonomous board LED behavior.
 *
 * Gives the inhibited-output heartbeat priority over normal output. Completed shutdown forces the
 * output off. The first normal pass selects full brightness, and later passes retain the current
 * output unless a breathing transition produces a new pattern.
 *
 * @param[in,out] controller Persistent autonomous LED state.
 * @param[in] input Current output gate, shutdown, and transition requests.
 * @param[in] now_ms Current monotonic time in milliseconds.
 * @return A new pattern or LED_PATTERN_NO_UPDATE when the retained output must remain unchanged.
 */
uint16_t led_pattern_controller_update(LedPatternController *controller,
                                       LedPatternControllerInput input, uint32_t now_ms) {
    if (input.profile_save_complete) {
        return 0;
    }
    if (input.output_inhibited) {
        return update_heartbeat(controller, now_ms);
    }
    if (input.shutdown_complete) {
        return 0;
    }
    if (!controller->normal_started) {
        controller->normal_started = true;
        return LED_PATTERN_BREATH_MAXIMUM;
    }
    return update_breathing(controller, input, now_ms);
}
