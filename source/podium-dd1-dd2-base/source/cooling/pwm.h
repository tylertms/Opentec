#ifndef OPENTEC_BASE_COOLING_PWM_H
#define OPENTEC_BASE_COOLING_PWM_H

#include <stdbool.h>
#include <stdint.h>

uint16_t fan_pwm_compare(uint16_t duty_percent, bool inverted_pwm, bool outputs_disabled);

#endif
