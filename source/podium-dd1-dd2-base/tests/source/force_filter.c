#include <assert.h>
#include <stdint.h>

#include "force_feedback/filter.h"

static void test_intensity_windows(void) {
    static const uint8_t expected[] = {40, 35, 30, 25, 20, 15, 10, 7, 4, 2, 1};
    ForceFilter filter = {0};

    for (uint8_t index = 0; index < sizeof(expected); ++index) {
        force_filter_configure(&filter, index * 10);
        assert(filter.window == expected[index]);
    }

    force_filter_configure(&filter, 55);
    assert(filter.window == 1);
    force_filter_configure(&filter, 101);
    assert(filter.window == 1);
}

static void test_deadline_and_cursor_sequence(void) {
    ForceFilter filter = {0};
    force_filter_configure(&filter, 90);

    assert(force_filter_update(&filter, 100, 0) == 50);
    assert(force_filter_update(&filter, 300, 0) == 50);
    assert(force_filter_update(&filter, 300, 1) == 50);
    assert(force_filter_update(&filter, -100, 2) == 0);
    assert(force_filter_update(&filter, 400, 3) == 150);
}

static void test_signed_samples(void) {
    ForceFilter filter = {0};
    force_filter_configure(&filter, 80);

    assert(force_filter_update(&filter, -400, 0) == -100);
    assert(force_filter_update(&filter, -400, 1) == -200);
    assert(force_filter_update(&filter, -400, 2) == -300);
    assert(force_filter_update(&filter, -400, 3) == -300);
    assert(force_filter_update(&filter, -400, 4) == -400);
}

static void test_configuration_changes(void) {
    ForceFilter filter = {0};
    force_filter_configure(&filter, 90);
    assert(force_filter_update(&filter, 1000, 0) == 500);

    force_filter_configure(&filter, 90);
    assert(filter.index == 1);
    assert(filter.output == 500);

    force_filter_configure(&filter, 100);
    assert(filter.index == 0);
    assert(filter.output == 500);
    assert(force_filter_update(&filter, -250, 0) == 500);
    assert(force_filter_update(&filter, -250, 1) == 0);
    assert(force_filter_update(&filter, -250, 2) == -250);
}

int main(void) {
    test_intensity_windows();
    test_deadline_and_cursor_sequence();
    test_signed_samples();
    test_configuration_changes();
    return 0;
}
