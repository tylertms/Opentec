#include "board/led_pattern.h"

#include <stdint.h>

enum { LED_PATTERN_BUCKET_SHIFT = 2 };

/** @brief PWM duties for the 64 host-selectable brightness buckets. */
static const uint16_t pwm_duties[] = {
    0,   1,   1,   2,   2,   2,   2,   2,   3,   3,   3,   4,   4,   5,   5,   6,
    6,   7,   8,   9,   10,  11,  12,  13,  15,  17,  19,  21,  23,  26,  29,  32,
    36,  40,  44,  49,  55,  61,  68,  76,  85,  94,  105, 117, 131, 146, 162, 181,
    202, 225, 250, 279, 311, 346, 386, 430, 479, 534, 595, 663, 739, 824, 918, 1023,
};

/**
 * @brief Converts a host LED pattern to its PWM duty.
 *
 * Quantizes the eight-bit pattern into 64 brightness buckets and applies the board's nonlinear
 * duty curve. The resulting duty spans zero through the 10-bit PWM period.
 *
 * @param[in] pattern Host-selected LED brightness pattern.
 * @return Ten-bit PWM duty for the pattern.
 */
uint16_t led_pattern_pwm_duty(uint8_t pattern) {
    return pwm_duties[pattern >> LED_PATTERN_BUCKET_SHIFT];
}
