#include <assert.h>
#include <stdint.h>

#include "cooling/tachometer.h"

static void test_period_conversion(void) {
    assert(fan_tachometer_rpm(1000, 601000) == 3000);
    assert(fan_tachometer_rpm(0, 60000000) == 30);
}

static void test_timer_wraparound(void) {
    assert(fan_tachometer_rpm(UINT32_MAX - 99, 500) == 50880);
}

static void test_sixteen_bit_arithmetic(void) {
    assert(fan_tachometer_rpm(0, 1) == 53760);
    assert(fan_tachometer_rpm(0, 1000) == 30528);
}

int main(void) {
    test_period_conversion();
    test_timer_wraparound();
    test_sixteen_bit_arithmetic();
    return 0;
}
