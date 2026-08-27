#include <assert.h>
#include <stdint.h>

#include "force_feedback/filter.h"

static void test_intensity_windows(void) {
    static const uint8_t expected[] = {40, 35, 30, 25, 20, 15, 10, 7, 4, 2, 1};
    ForceFilter filter;

    for (uint8_t index = 0; index < sizeof(expected); ++index) {
        force_filter_configure(&filter, index * 10);
        assert(filter.window == expected[index]);
    }

    force_filter_configure(&filter, 55);
    assert(filter.window == 1);
    force_filter_configure(&filter, 101);
    assert(filter.window == 1);
}

static void test_startup_and_rolling_average(void) {
    ForceFilter filter;
    force_filter_configure(&filter, 90);

    assert(force_filter_update(&filter, 100) == 50);
    assert(force_filter_update(&filter, 300) == 200);
    assert(force_filter_update(&filter, -100) == 100);
}

static void test_signed_samples(void) {
    ForceFilter filter;
    force_filter_configure(&filter, 80);

    assert(force_filter_update(&filter, -400) == -100);
    assert(force_filter_update(&filter, -400) == -200);
    assert(force_filter_update(&filter, -400) == -300);
    assert(force_filter_update(&filter, -400) == -400);
    assert(force_filter_update(&filter, 400) == -200);
}

static void test_reconfiguration_clears_history(void) {
    ForceFilter filter;
    force_filter_configure(&filter, 90);
    assert(force_filter_update(&filter, 1000) == 500);

    force_filter_configure(&filter, 100);
    assert(force_filter_update(&filter, -250) == -250);
}

int main(void) {
    test_intensity_windows();
    test_startup_and_rolling_average();
    test_signed_samples();
    test_reconfiguration_clears_history();
    return 0;
}
