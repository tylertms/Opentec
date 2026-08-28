#include "analog/pulse_measurement.h"

#include <stdint.h>

enum {
    PULSE_CAPTURE_TIMER_HZ = 60000000,
    PULSE_MEASUREMENT_SCALE = 30,
};

/**
 * @brief Converts two consecutive capture timestamps to the scaled pulse measurement.
 * @param[in] previous_capture Earlier 32-bit capture timestamp.
 * @param[in] current_capture Later 32-bit capture timestamp.
 * @return Scaled 16-bit measurement, including timer wraparound and arithmetic truncation.
 */
uint16_t pulse_measurement_units(uint32_t previous_capture, uint32_t current_capture) {
    uint32_t elapsed_ticks = current_capture - previous_capture;
    uint16_t timer_rate = (uint16_t)(PULSE_CAPTURE_TIMER_HZ / elapsed_ticks);
    return (uint16_t)(timer_rate * PULSE_MEASUREMENT_SCALE);
}
