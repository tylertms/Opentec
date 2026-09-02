#ifndef OPENTEC_BASE_PLATFORM_COOLING_H
#define OPENTEC_BASE_PLATFORM_COOLING_H

#include <stdbool.h>
#include <stdint.h>

/**
 * @brief Identifies one platform fan.
 */
typedef enum {
    PLATFORM_FAN_PRIMARY,   /**< Primary fan and tachometer input. */
    PLATFORM_FAN_SECONDARY, /**< Secondary fan and tachometer input. */
} PlatformFan;

/**
 * @brief Completed fan tachometer capture result.
 */
typedef struct {
    uint32_t previous_capture; /**< Previous captured edge timestamp. */
    uint32_t current_capture;  /**< Current captured edge timestamp. */
    bool present;              /**< True when a tachometer signal was captured. */
} PlatformFanTachometer;

/**
 * @brief Initializes fan PWM and tachometer capture.
 *
 * Configures both fan outputs and their capture inputs and schedules alternating capture windows.
 *
 * @param[in] inverted_pwm True when the fan output compare polarity is inverted.
 */
void platform_cooling_init(bool inverted_pwm);

/**
 * @brief Sets both fan PWM duty percentages.
 *
 * Applies the requested duty values using the configured polarity and disable state.
 *
 * @param[in] primary_percent Primary fan duty percentage.
 * @param[in] secondary_percent Secondary fan duty percentage.
 * @param[in] outputs_disabled True to force both outputs inactive.
 */
void platform_cooling_set_duty(uint16_t primary_percent, uint16_t secondary_percent,
                               bool outputs_disabled);

/**
 * @brief Services alternating fan tachometer capture windows from Timer 1.
 *
 * Publishes missing results and starts the next primary or secondary capture when its
 * 50-millisecond deadline is reached. The firmware invokes this from the Timer 1 millisecond
 * callback.
 *
 * @param[in] now_ms Current monotonic time in milliseconds.
 */
void platform_cooling_service(uint32_t now_ms);

/**
 * @brief Takes one completed fan tachometer result.
 *
 * Copies and consumes the pending result for the selected fan.
 *
 * @param[in] fan Fan tachometer channel to inspect.
 * @param[out] tachometer Destination for captured timestamps and signal presence.
 * @return True when a result was available; otherwise false.
 */
bool platform_cooling_take_tachometer(PlatformFan fan, PlatformFanTachometer *tachometer);

#endif
