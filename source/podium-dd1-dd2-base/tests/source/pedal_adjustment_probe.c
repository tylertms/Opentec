#include <assert.h>
#include <stdint.h>
#include <string.h>

#include "pedal/adjustment_probe.h"

static void test_exposes_fixed_query(void) {
    static const uint8_t expected[PEDAL_ADJUSTMENT_PROBE_REQUEST_SIZE] = {
        0x16, 0x0a, 0x02, 0x08, 0x02, 0x20, 0x08, 0xaa, 0x01, 0x0d, 0xba, 0x01, 0x0a,
        0x5a, 0x08, 0x62, 0x06, 0x0a, 0x02, 0x00, 0x02, 0x10, 0x01, 0xb6, 0xf8,
    };
    assert(memcmp(pedal_adjustment_probe_request(), expected, sizeof(expected)) == 0);
}

static void test_rejects_incomplete_responses(void) {
    PedalAdjustmentDisplay display = PEDAL_ADJUSTMENT_DISPLAY_HOLD;
    const uint8_t short_response[4] = {0};
    assert(!pedal_adjustment_probe_classify(0, 5, &display));
    assert(!pedal_adjustment_probe_classify(short_response, sizeof(short_response), &display));
    assert(!pedal_adjustment_probe_classify(short_response, sizeof(short_response), 0));
    assert(display == PEDAL_ADJUSTMENT_DISPLAY_HOLD);
}

static void test_classifies_tail_and_markers_in_priority_order(void) {
    uint8_t response[40] = {0};
    PedalAdjustmentDisplay display;

    response[sizeof(response) - 2] = 0xbf;
    response[sizeof(response) - 1] = 0x77;
    response[0x1f] = 'X';
    response[0x15] = 'X';
    assert(pedal_adjustment_probe_classify(response, sizeof(response), &display));
    assert(display == PEDAL_ADJUSTMENT_DISPLAY_NONE);

    response[sizeof(response) - 2] = 0;
    response[sizeof(response) - 1] = 0;
    assert(pedal_adjustment_probe_classify(response, sizeof(response), &display));
    assert(display == PEDAL_ADJUSTMENT_DISPLAY_CLUTCH);

    response[0x1f] = 0;
    assert(pedal_adjustment_probe_classify(response, sizeof(response), &display));
    assert(display == PEDAL_ADJUSTMENT_DISPLAY_THROTTLE);

    response[0x15] = 0;
    assert(pedal_adjustment_probe_classify(response, sizeof(response), &display));
    assert(display == PEDAL_ADJUSTMENT_DISPLAY_BOTH);
}

static void test_treats_absent_optional_markers_as_unset(void) {
    const uint8_t response[5] = {0};
    PedalAdjustmentDisplay display;
    assert(pedal_adjustment_probe_classify(response, sizeof(response), &display));
    assert(display == PEDAL_ADJUSTMENT_DISPLAY_BOTH);
}

int main(void) {
    test_exposes_fixed_query();
    test_rejects_incomplete_responses();
    test_classifies_tail_and_markers_in_priority_order();
    test_treats_absent_optional_markers_as_unset();
    return 0;
}
