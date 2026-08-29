#include "board/led_pattern.h"

#include <assert.h>
#include <stddef.h>
#include <stdint.h>

static void test_maps_every_pattern_bucket(void) {
    static const uint16_t expected[] = {
        0,   1,   1,   2,   2,   2,   2,   2,   3,   3,   3,   4,   4,   5,   5,   6,
        6,   7,   8,   9,   10,  11,  12,  13,  15,  17,  19,  21,  23,  26,  29,  32,
        36,  40,  44,  49,  55,  61,  68,  76,  85,  94,  105, 117, 131, 146, 162, 181,
        202, 225, 250, 279, 311, 346, 386, 430, 479, 534, 595, 663, 739, 824, 918, 1023,
    };

    for (size_t bucket = 0; bucket < sizeof(expected) / sizeof(expected[0]); bucket++) {
        for (uint8_t offset = 0; offset < 4; offset++) {
            assert(led_pattern_pwm_duty((uint8_t)(bucket * 4 + offset)) == expected[bucket]);
        }
    }
}

int main(void) {
    test_maps_every_pattern_bucket();
    return 0;
}
