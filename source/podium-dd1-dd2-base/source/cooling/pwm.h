#ifndef OPENTEC_BASE_COOLING_PWM_H
#define OPENTEC_BASE_COOLING_PWM_H

#include <stdbool.h>
#include <stdint.h>

/**
 * @brief Converts a fan duty request to its PWM compare value.
 *
 * Clamps the request to 100 percent, applies the configured polarity, and selects the inactive
 * compare value when the fan outputs are disabled.
 *
 * @param[in] duty_percent Requested duty percentage; values above 100 are clamped.
 * @param[in] inverted_pwm True when increasing duty requires a decreasing compare value.
 * @param[in] outputs_disabled True to force the inactive compare value.
 * @return PWM compare value for the board's 3192-count period.
 */
uint16_t fan_pwm_compare(uint16_t duty_percent, bool inverted_pwm, bool outputs_disabled);

#endif
