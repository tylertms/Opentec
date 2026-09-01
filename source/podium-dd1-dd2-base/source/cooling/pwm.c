#include "cooling/pwm.h"

#include <stdbool.h>
#include <stdint.h>

/**
 * @brief Fan PWM timing constant.
 *
 * The compare value spans the board's complete 3192-count fan PWM period.
 */
enum {
    FAN_PWM_PERIOD = 3192, /**< Complete fan PWM compare range. */
};

/**
 * @brief Converts a clamped fan duty percentage to its PWM compare value.
 *
 * Applies the active polarity, clamps requests above 100 percent, and selects the inactive compare
 * value while outputs are disabled.
 *
 * @param[in] duty_percent Requested output duty; values above 100 are clamped.
 * @param[in] inverted_pwm True when increasing duty requires a decreasing compare value.
 * @param[in] outputs_disabled True to force the output to its inactive compare value.
 * @return PWM compare value from 0 through 3192.
 */
uint16_t fan_pwm_compare(uint16_t duty_percent, bool inverted_pwm, bool outputs_disabled) {
    if (duty_percent > 100) {
        duty_percent = 100;
    }
    if (outputs_disabled) {
        return inverted_pwm ? FAN_PWM_PERIOD : 0;
    }

    uint16_t active_counts = (uint16_t)((uint32_t)duty_percent * FAN_PWM_PERIOD / 100);
    return inverted_pwm ? FAN_PWM_PERIOD - active_counts : active_counts;
}
