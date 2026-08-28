#include "analog/resistance_output.h"

#include <stdbool.h>
#include <stdint.h>

enum {
    RESISTANCE_OUTPUT_PWM_PERIOD = 3192,
};

/**
 * @brief Converts a clamped resistance duty percentage to its PWM compare value.
 * @param[in] duty_percent Requested output duty; values above 100 are clamped.
 * @param[in] inverted_pwm True when increasing duty requires a decreasing compare value.
 * @param[in] outputs_disabled True to force the output to its inactive compare value.
 * @return PWM compare value from 0 through 3192.
 */
uint16_t resistance_output_compare(uint16_t duty_percent, bool inverted_pwm,
                                   bool outputs_disabled) {
    if (duty_percent > 100) {
        duty_percent = 100;
    }
    if (outputs_disabled) {
        return inverted_pwm ? RESISTANCE_OUTPUT_PWM_PERIOD : 0;
    }

    uint16_t active_counts =
        (uint16_t)((uint32_t)duty_percent * RESISTANCE_OUTPUT_PWM_PERIOD / 100);
    return inverted_pwm ? RESISTANCE_OUTPUT_PWM_PERIOD - active_counts : active_counts;
}
