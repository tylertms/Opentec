#ifndef OPENTEC_BASE_BOARD_LED_PATTERN_H
#define OPENTEC_BASE_BOARD_LED_PATTERN_H

#include <stdbool.h>
#include <stdint.h>

enum {
    LED_PATTERN_NO_UPDATE = 0x100,
    LED_PATTERN_STARTUP_STEP_COUNT = 63,
};

typedef enum {
    LED_PATTERN_BREATH_IDLE,
    LED_PATTERN_BREATH_DARKEN,
    LED_PATTERN_BREATH_BRIGHTEN,
    LED_PATTERN_BREATH_WAIT_DARKEN,
    LED_PATTERN_BREATH_WAIT_BRIGHTEN,
} LedPatternBreathPhase;

typedef struct {
    bool output_inhibited;
    bool shutdown_complete;
    bool pedal_handshake_active;
    bool alternate_runtime_active;
    bool force_override_requested;
} LedPatternControllerInput;

typedef struct {
    uint32_t breath_deadline_ms;
    uint32_t heartbeat_deadline_ms;
    LedPatternBreathPhase breath_phase;
    uint8_t current_pattern;
    uint8_t heartbeat_pattern;
    bool normal_started;
    bool transition_active;
    bool heartbeat_waiting;
} LedPatternController;

uint16_t led_pattern_pwm_duty(uint8_t pattern);
uint8_t led_pattern_startup_pattern(uint8_t step);
void led_pattern_controller_init(LedPatternController *controller);
uint16_t led_pattern_controller_update(LedPatternController *controller,
                                       LedPatternControllerInput input, uint32_t now_ms);

#endif
