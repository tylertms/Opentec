#include <assert.h>
#include <stdbool.h>
#include <stdint.h>

#include "wheel/auxiliary_output.h"

static void test_combines_report_and_latched_bands(void) {
    WheelAuxiliaryOutput output = {
        .report = 0x0049,
    };
    assert(wheel_auxiliary_output_encode(&output) == 0x51);

    output.report = 0;
    output.latched_bands = 0x07;
    assert(wheel_auxiliary_output_encode(&output) == 0x51);
}

static void test_encodes_cumulative_code_patterns(void) {
    static const uint16_t reports[] = {
        0x0100, 0x0180, 0x01c0, 0x01e0, 0x01f0, 0x01f8, 0x01fc, 0x01fe, 0x01ff,
    };
    static const uint8_t expected[] = {
        0x50, 0x50, 0x50, 0x40, 0x40, 0x40, 0x01, 0x01, 0x51,
    };
    WheelAuxiliaryOutput output = {.code_mode = true};

    for (uint8_t index = 0; index < sizeof(reports) / sizeof(reports[0]); index++) {
        output.report = reports[index];
        assert(wheel_auxiliary_output_encode(&output) == expected[index]);
    }
    output.report = 0x017f;
    assert(wheel_auxiliary_output_encode(&output) == 0);
    output.latched_bands = 0x01;
    assert(wheel_auxiliary_output_encode(&output) == 0x40);
}

static void test_prioritizes_exclusive_bands(void) {
    WheelAuxiliaryOutput output = {
        .report = 0x01c8,
        .exclusive_mode = true,
    };
    assert(wheel_auxiliary_output_encode(&output) == 0x40);

    output.report = 0x01c1;
    assert(wheel_auxiliary_output_encode(&output) == 0x01);
    output.report = 0x0007;
    assert(wheel_auxiliary_output_encode(&output) == 0x51);
    output.report = 0;
    output.latched_bands = 0x02;
    assert(wheel_auxiliary_output_encode(&output) == 0x50);
}

static void test_option_one_and_missing_output_are_clear(void) {
    WheelAuxiliaryOutput output = {
        .report = 0x01ff,
        .option = 1,
    };
    assert(wheel_auxiliary_output_encode(&output) == 0);
    output.option = 2;
    assert(wheel_auxiliary_output_encode(&output) == 0x51);
    output.option = UINT8_MAX;
    assert(wheel_auxiliary_output_encode(&output) == 0x51);
    assert(wheel_auxiliary_output_encode(NULL) == 0);
}

int main(void) {
    test_combines_report_and_latched_bands();
    test_encodes_cumulative_code_patterns();
    test_prioritizes_exclusive_bands();
    test_option_one_and_missing_output_are_clear();
    return 0;
}
