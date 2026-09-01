#ifndef OPENTEC_BASE_BOARD_LED_PATTERN_H
#define OPENTEC_BASE_BOARD_LED_PATTERN_H

#include <stdbool.h>
#include <stdint.h>

/**
 * @brief Constants used by the board LED pattern controller.
 *
 * LED_PATTERN_NO_UPDATE distinguishes an unchanged output from an eight-bit LED pattern, while
 * LED_PATTERN_STARTUP_STEP_COUNT bounds the startup brightness sweep.
 */
enum {
    LED_PATTERN_NO_UPDATE =
        0x100, /**< Return value meaning that the current output is unchanged. */
    LED_PATTERN_STARTUP_STEP_COUNT = 63, /**< Number of steps in the startup brightness sweep. */
};

/**
 * @brief Phases of an LED breathing transition.
 *
 * The controller alternates one-step darkening and brightening phases with timed wait phases.
 */
typedef enum {
    LED_PATTERN_BREATH_IDLE,          /**< No breathing transition is active. */
    LED_PATTERN_BREATH_DARKEN,        /**< Move the current pattern toward the minimum. */
    LED_PATTERN_BREATH_BRIGHTEN,      /**< Move the current pattern toward the maximum. */
    LED_PATTERN_BREATH_WAIT_DARKEN,   /**< Wait before the next darkening step. */
    LED_PATTERN_BREATH_WAIT_BRIGHTEN, /**< Wait before the next brightening step. */
} LedPatternBreathPhase;

/**
 * @brief Inputs that select autonomous LED behavior.
 *
 * The controller gives inhibited output a heartbeat priority, turns the LED off after shutdown,
 * and uses the remaining fields to request or sustain a breathing transition.
 */
typedef struct {
    bool output_inhibited;         /**< True to select the inhibited-output heartbeat. */
    bool shutdown_complete;        /**< True to force the LED output off. */
    bool pedal_handshake_active;   /**< True while the pedal handshake requests breathing. */
    bool alternate_runtime_active; /**< True while alternate runtime requests breathing. */
    bool force_override_requested; /**< True to start breathing for a force override request. */
    bool profile_save_complete;    /**< True after the current profile save completes. */
} LedPatternControllerInput;

/**
 * @brief Stateful autonomous LED pattern controller.
 *
 * Stores breathing and heartbeat timing, the current pattern, and transition flags across periodic
 * update calls.
 */
typedef struct {
    uint32_t breath_deadline_ms;    /**< Deadline for the next breathing step in milliseconds. */
    uint32_t heartbeat_deadline_ms; /**< Deadline for the next heartbeat change in milliseconds. */
    LedPatternBreathPhase breath_phase; /**< Current breathing phase. */
    uint8_t current_pattern;            /**< Current eight-bit LED pattern. */
    uint8_t heartbeat_pattern;          /**< Next heartbeat pattern to return. */
    bool normal_started;                /**< True after the first normal full-brightness output. */
    bool transition_active;             /**< True while a breathing cycle is in progress. */
    bool heartbeat_waiting; /**< True while waiting for the heartbeat interval to expire. */
} LedPatternController;

/**
 * @brief Converts an eight-bit LED pattern to its board PWM duty.
 *
 * Quantizes the pattern into 64 brightness buckets and applies the board's nonlinear duty curve.
 *
 * @param[in] pattern Eight-bit host-selected LED brightness pattern.
 * @return Ten-bit PWM duty corresponding to pattern.
 */
uint16_t led_pattern_pwm_duty(uint8_t pattern);

/**
 * @brief Builds one pattern in the startup brightness sweep.
 *
 * Converts a zero-based step below LED_PATTERN_STARTUP_STEP_COUNT into the first pattern of its
 * corresponding four-pattern brightness bucket.
 *
 * @param[in] step Zero-based startup step below LED_PATTERN_STARTUP_STEP_COUNT.
 * @return Eight-bit LED pattern for the startup step.
 */
uint8_t led_pattern_startup_pattern(uint8_t step);

/**
 * @brief Initializes autonomous board LED state.
 *
 * Clears timing and transition state so the first normal update emits full brightness and the
 * first inhibited update emits the initial heartbeat pattern.
 *
 * @param[out] controller LED pattern state to initialize.
 */
void led_pattern_controller_init(LedPatternController *controller);

/**
 * @brief Advances autonomous board LED behavior.
 *
 * Gives inhibited output priority over normal output, forces the LED off after shutdown, and
 * otherwise starts or advances the requested breathing transition.
 *
 * @param[in,out] controller Persistent autonomous LED state.
 * @param[in] input Current output gate, shutdown, and transition requests.
 * @param[in] now_ms Current monotonic time in milliseconds.
 * @return A new eight-bit pattern or LED_PATTERN_NO_UPDATE when the output is unchanged.
 */
uint16_t led_pattern_controller_update(LedPatternController *controller,
                                       LedPatternControllerInput input, uint32_t now_ms);

#endif
