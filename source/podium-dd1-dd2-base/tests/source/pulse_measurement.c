#include "analog/pulse_measurement.h"

#include <assert.h>
#include <stdint.h>

static void test_period_conversion(void) {
    assert(pulse_measurement_units(1000, 601000) == 3000);
    assert(pulse_measurement_units(0, 60000000) == 30);
}

static void test_timer_wraparound(void) {
    assert(pulse_measurement_units(UINT32_MAX - 99, 500) == 50880);
}

static void test_sixteen_bit_arithmetic(void) {
    assert(pulse_measurement_units(0, 1) == 53760);
    assert(pulse_measurement_units(0, 1000) == 30528);
}

int main(void) {
    test_period_conversion();
    test_timer_wraparound();
    test_sixteen_bit_arithmetic();
    return 0;
}
