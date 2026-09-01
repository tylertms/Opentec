#ifndef OPENTEC_BASE_COOLING_TACHOMETER_H
#define OPENTEC_BASE_COOLING_TACHOMETER_H

#include <stdint.h>

/**
 * @brief Converts a fan tachometer capture interval to revolutions per minute.
 *
 * Subtracts consecutive 60 MHz timer captures with unsigned wraparound, converts the interval from
 * a two-pulse-per-revolution tachometer to RPM, and preserves the implementation's 16-bit
 * arithmetic truncation. The capture interval must be nonzero.
 *
 * @param[in] previous_capture Older 32-bit capture timestamp.
 * @param[in] current_capture Newer 32-bit capture timestamp.
 * @return Fan speed in RPM.
 */
uint16_t fan_tachometer_rpm(uint32_t previous_capture, uint32_t current_capture);

#endif
