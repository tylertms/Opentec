#include "cooling/tachometer.h"

#include <stdint.h>

enum {
    FAN_TACHOMETER_TIMER_HZ = 60000000,
    FAN_TACHOMETER_REVOLUTIONS_PER_CAPTURE = 30,
};

/**
 * @brief Converts two consecutive two-pulse-per-revolution fan captures to RPM.
 * @param[in] previous_capture Earlier 32-bit capture timestamp.
 * @param[in] current_capture Later 32-bit capture timestamp.
 * @return Fan speed in RPM, including timer wraparound and 16-bit arithmetic truncation.
 */
uint16_t fan_tachometer_rpm(uint32_t previous_capture, uint32_t current_capture) {
    uint32_t elapsed_ticks = current_capture - previous_capture;
    uint16_t capture_rate = (uint16_t)(FAN_TACHOMETER_TIMER_HZ / elapsed_ticks);
    return (uint16_t)(capture_rate * FAN_TACHOMETER_REVOLUTIONS_PER_CAPTURE);
}
