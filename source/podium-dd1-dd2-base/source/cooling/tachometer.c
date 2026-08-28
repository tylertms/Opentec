#include "cooling/tachometer.h"

#include <stdint.h>

enum {
    FAN_TACHOMETER_TIMER_HZ = 60000000,
    FAN_TACHOMETER_REVOLUTIONS_PER_CAPTURE = 30,
};

/**
 * @brief Converts two consecutive two-pulse-per-revolution fan captures to RPM.
 *
 * Divides the 60 MHz timer rate by the wrapping capture interval and applies the capture-to-RPM
 * factor with the device's 16-bit truncation.
 *
 * @param[in] previous_capture Earlier 32-bit capture timestamp.
 * @param[in] current_capture Later 32-bit capture timestamp.
 * @return Fan speed in RPM, including timer wraparound and 16-bit arithmetic truncation.
 */
uint16_t fan_tachometer_rpm(uint32_t previous_capture, uint32_t current_capture) {
    uint32_t elapsed_ticks = current_capture - previous_capture;
    uint16_t capture_rate = (uint16_t)(FAN_TACHOMETER_TIMER_HZ / elapsed_ticks);
    return (uint16_t)(capture_rate * FAN_TACHOMETER_REVOLUTIONS_PER_CAPTURE);
}
