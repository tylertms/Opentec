#ifndef OPENTEC_BASE_COOLING_FAN_H
#define OPENTEC_BASE_COOLING_FAN_H

#include <stdbool.h>
#include <stdint.h>

typedef enum {
    FAN_LEVEL_OFF,
    FAN_LEVEL_LOW,
    FAN_LEVEL_MEDIUM,
    FAN_LEVEL_HIGH,
    FAN_LEVEL_NEAR_MAXIMUM,
    FAN_LEVEL_MAXIMUM,
} FanLevel;

typedef struct {
    FanLevel level;
    uint8_t duty_percent;
} FanController;

void fan_controller_init(FanController *controller);
uint8_t fan_controller_update(FanController *controller, int16_t temperature_c,
                              bool temperature_valid, bool output_permitted);
uint16_t fan_tachometer_rpm(uint32_t previous_capture, uint32_t current_capture,
                            uint32_t timer_frequency_hz, uint8_t pulses_per_revolution);

#endif
