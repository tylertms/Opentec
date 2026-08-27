#include "cooling/fan.h"

#include <stdbool.h>
#include <stdint.h>

static uint8_t level_duty(FanLevel level) {
    static const uint8_t duties[] = {0, 20, 40, 50, 70, 100};
    return duties[level];
}

void fan_controller_init(FanController *controller) {
    controller->level = FAN_LEVEL_MAXIMUM;
    controller->duty_percent = level_duty(controller->level);
}

static void raise_level(FanController *controller, int16_t temperature_c) {
    static const int16_t thresholds[] = {35, 45, 60, 75, 95};
    if (controller->level < FAN_LEVEL_MAXIMUM && temperature_c > thresholds[controller->level]) {
        controller->level++;
    }
}

static void lower_level(FanController *controller, int16_t temperature_c) {
    static const int16_t thresholds[] = {30, 40, 55, 70, 90};
    if (controller->level > FAN_LEVEL_OFF && temperature_c < thresholds[controller->level - 1]) {
        controller->level--;
    }
}

uint8_t fan_controller_update(FanController *controller, int16_t temperature_c,
                              bool temperature_valid, bool output_permitted) {
    if (!temperature_valid) {
        controller->level = FAN_LEVEL_MAXIMUM;
    } else {
        raise_level(controller, temperature_c);
        lower_level(controller, temperature_c);
    }

    controller->duty_percent = output_permitted ? level_duty(controller->level) : 0;
    return controller->duty_percent;
}

uint16_t fan_tachometer_rpm(uint32_t previous_capture, uint32_t current_capture,
                            uint32_t timer_frequency_hz, uint8_t pulses_per_revolution) {
    uint32_t elapsed_ticks = current_capture - previous_capture;
    if (elapsed_ticks == 0 || timer_frequency_hz == 0 || pulses_per_revolution == 0) {
        return 0;
    }

    uint64_t revolutions_per_minute =
        (uint64_t)timer_frequency_hz * 60 / elapsed_ticks / pulses_per_revolution;
    return revolutions_per_minute > UINT16_MAX ? UINT16_MAX : (uint16_t)revolutions_per_minute;
}
