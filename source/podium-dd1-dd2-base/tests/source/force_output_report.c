#include <assert.h>
#include <stdint.h>
#include <string.h>

#include "force_feedback/output_report.h"

static void test_positive_output(void) {
    const ForceOutputReport report = {
        .positive_direction = true,
        .primary_magnitude = 0x1234,
        .secondary_magnitude = 0x5678,
    };
    const uint8_t expected[FORCE_OUTPUT_REPORT_SIZE] = {1, 0x34, 0x12, 0x78, 0x56};
    uint8_t output[FORCE_OUTPUT_REPORT_SIZE];

    force_output_report_encode(&report, output);

    assert(memcmp(output, expected, sizeof(output)) == 0);
}

static void test_negative_output(void) {
    const ForceOutputReport report = {
        .positive_direction = false,
        .primary_magnitude = UINT16_MAX,
        .secondary_magnitude = 1,
    };
    const uint8_t expected[FORCE_OUTPUT_REPORT_SIZE] = {0, 0xff, 0xff, 0x01, 0x00};
    uint8_t output[FORCE_OUTPUT_REPORT_SIZE];

    force_output_report_encode(&report, output);

    assert(memcmp(output, expected, sizeof(output)) == 0);
}

static void test_zero_output_preserves_direction(void) {
    const ForceOutputReport report = {
        .positive_direction = true,
    };
    const uint8_t expected[FORCE_OUTPUT_REPORT_SIZE] = {1, 0, 0, 0, 0};
    uint8_t output[FORCE_OUTPUT_REPORT_SIZE];

    force_output_report_encode(&report, output);

    assert(memcmp(output, expected, sizeof(output)) == 0);
}

int main(void) {
    test_positive_output();
    test_negative_output();
    test_zero_output_preserves_direction();
    return 0;
}
