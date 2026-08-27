#include "input/button_filter.h"

#include <assert.h>
#include <stdint.h>

static void update(WheelButtonFilter *filter, uint8_t first, uint8_t second, uint8_t third,
                   uint8_t output[WHEEL_BUTTON_FILTER_BYTES]) {
    const uint8_t input[WHEEL_BUTTON_FILTER_BYTES] = {first, second, third};
    wheel_button_filter_update(filter, input, output);
}

static void test_requires_three_samples(void) {
    WheelButtonFilter filter = {0};
    uint8_t output[WHEEL_BUTTON_FILTER_BYTES];

    update(&filter, 0x01, 0x80, 0x55, output);
    assert(output[0] == 0);
    assert(output[1] == 0);
    assert(output[2] == 0);

    update(&filter, 0x01, 0x80, 0x55, output);
    assert(output[0] == 0);
    assert(output[1] == 0);
    assert(output[2] == 0);

    update(&filter, 0x01, 0x80, 0x55, output);
    assert(output[0] == 0x01);
    assert(output[1] == 0x80);
    assert(output[2] == 0x55);
}

static void test_ages_out_each_sample_after_three_updates(void) {
    WheelButtonFilter filter = {0};
    uint8_t output[WHEEL_BUTTON_FILTER_BYTES];

    update(&filter, 0xff, 0xff, 0xff, output);
    update(&filter, 0xff, 0xff, 0xff, output);
    update(&filter, 0xff, 0xff, 0xff, output);
    update(&filter, 0xfe, 0x7f, 0xaa, output);

    assert(output[0] == 0xfe);
    assert(output[1] == 0x7f);
    assert(output[2] == 0xaa);

    update(&filter, 0xff, 0xff, 0xff, output);
    assert(output[0] == 0xfe);
    assert(output[1] == 0x7f);
    assert(output[2] == 0xaa);

    update(&filter, 0xff, 0xff, 0xff, output);
    assert(output[0] == 0xfe);
    assert(output[1] == 0x7f);
    assert(output[2] == 0xaa);

    update(&filter, 0xff, 0xff, 0xff, output);
    assert(output[0] == 0xff);
    assert(output[1] == 0xff);
    assert(output[2] == 0xff);
}

static void test_accepts_release_immediately(void) {
    WheelButtonFilter filter = {0};
    uint8_t output[WHEEL_BUTTON_FILTER_BYTES];

    update(&filter, 0x01, 0x02, 0x04, output);
    update(&filter, 0x01, 0x02, 0x04, output);
    update(&filter, 0x01, 0x02, 0x04, output);
    update(&filter, 0x00, 0x00, 0x00, output);

    assert(output[0] == 0);
    assert(output[1] == 0);
    assert(output[2] == 0);
}

int main(void) {
    test_requires_three_samples();
    test_ages_out_each_sample_after_three_updates();
    test_accepts_release_immediately();
    return 0;
}
