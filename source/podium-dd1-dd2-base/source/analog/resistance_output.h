#ifndef OPENTEC_BASE_ANALOG_RESISTANCE_OUTPUT_H
#define OPENTEC_BASE_ANALOG_RESISTANCE_OUTPUT_H

#include <stdbool.h>
#include <stdint.h>

uint16_t resistance_output_compare(uint16_t duty_percent, bool inverted_pwm, bool outputs_disabled);

#endif
