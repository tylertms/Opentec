#include <assert.h>
#include <math.h>
#include <stdbool.h>
#include <stdint.h>

#include "cooling/temperature.h"

static void assert_near(float actual, float expected) { assert(fabsf(actual - expected) < 0.001f); }

static void test_lookup_boundaries(void) {
    assert_near(cooling_temperature_from_resistance(336851.0f), -99.9f);
    assert_near(cooling_temperature_from_resistance(256115.796875f), -10.0f);
    assert_near(cooling_temperature_from_resistance(47000.0f), 25.0f);
    assert_near(cooling_temperature_from_resistance(864.9000244140625f), 150.0f);
    assert_near(cooling_temperature_from_resistance(800.0f), 999.9f);
}

static void test_lookup_interpolation(void) {
    float midpoint = (47000.0f + 37925.30078125f) * 0.5f;
    assert_near(cooling_temperature_from_resistance(midpoint), 27.5f);
}

static void test_adc_conversion(void) { assert(cooling_temperature_from_adc_total(2048000) == 64); }

static void test_sample_window(void) {
    CoolingTemperatureMonitor monitor;
    cooling_temperature_monitor_init(&monitor);

    for (uint16_t sample = 1; sample < COOLING_TEMPERATURE_SAMPLE_COUNT; sample++) {
        assert(!cooling_temperature_monitor_add(&monitor, 2048, 2048));
    }
    assert(cooling_temperature_monitor_add(&monitor, 2048, 2048));
    assert(monitor.temperatures_c[0] == 64);
    assert(monitor.temperatures_c[1] == 64);
    assert(monitor.adc_totals[0] == 0);
    assert(monitor.adc_totals[1] == 0);
    assert(monitor.sample_count == 0);
}

int main(void) {
    test_lookup_boundaries();
    test_lookup_interpolation();
    test_adc_conversion();
    test_sample_window();
    return 0;
}
