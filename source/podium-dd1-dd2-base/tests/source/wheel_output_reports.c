#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "wheel/output_reports.h"

static void fill_arguments(uint8_t arguments[26], uint8_t action, uint8_t first) {
    arguments[0] = action;
    for (uint8_t index = 1; index < 26; index++) {
        arguments[index] = (uint8_t)(first + index - 1);
    }
}

static void test_encodes_reports_in_priority_order(void) {
    WheelOutputReports reports;
    wheel_output_reports_init(&reports);
    uint8_t arguments[26];
    uint8_t frame[33] = {0};

    fill_arguments(arguments, 0, 0x20);
    assert(wheel_output_reports_apply(&reports, arguments, 0, 0));
    fill_arguments(arguments, 1, 0x10);
    assert(wheel_output_reports_apply(&reports, arguments, 0, 0));

    assert(wheel_output_reports_encode_next(&reports, 0, frame));
    assert(frame[1] == 1);
    for (uint8_t index = 0; index < WHEEL_OUTPUT_REPORT_ONE_SIZE; index++) {
        assert(frame[index + 2] == (uint8_t)(0x10 + index));
    }

    assert(wheel_output_reports_encode_next(&reports, 0, frame));
    assert(frame[1] == 2);
    for (uint8_t index = 0; index < WHEEL_OUTPUT_REPORT_TWO_SIZE; index++) {
        assert(frame[index + 2] == (uint8_t)(0x20 + index));
    }
    assert(!wheel_output_reports_encode_next(&reports, 0, frame));
}

static void test_suppresses_legacy_report_two_while_interface_gate_is_closed(void) {
    WheelOutputReports reports;
    wheel_output_reports_init(&reports);
    uint8_t arguments[26];
    uint8_t frame[33] = {0};

    fill_arguments(arguments, 0, 0x30);
    assert(!wheel_output_reports_apply(&reports, arguments, 0x0f, 0));
    assert(!wheel_output_reports_encode_next(&reports, 0, frame));

    wheel_output_reports_set_interface_mode_gate(&reports, true);
    assert(wheel_output_reports_apply(&reports, arguments, 0x17, 0));
    assert(wheel_output_reports_encode_next(&reports, 0, frame));
    assert(frame[1] == 2);
}

static void test_expands_compact_report_groups(void) {
    WheelOutputReports reports;
    wheel_output_reports_init(&reports);

    const uint8_t first_band[4] = {1, 0, 0, 0};
    assert(wheel_output_reports_queue_packed(&reports, 1, first_band, 0));
    assert(reports.report_one[0] == 0x00);
    assert(reports.report_one[1] == 0x1f);

    const uint8_t second_band[4] = {2, 0, 0, 0};
    assert(wheel_output_reports_queue_packed(&reports, 1, second_band, 0));
    assert(reports.report_one[0] == 0x07);
    assert(reports.report_one[1] == 0xe0);

    const uint8_t third_band[4] = {4, 0, 0, 0};
    assert(wheel_output_reports_queue_packed(&reports, 1, third_band, 0));
    assert(reports.report_one[0] == 0xf8);
    assert(reports.report_one[1] == 0x00);

    const uint8_t all_bands[4] = {0xff, 0xff, 0xff, 0xff};
    assert(wheel_output_reports_queue_packed(&reports, 2, all_bands, 0));
    for (uint8_t index = 0; index < WHEEL_OUTPUT_REPORT_TWO_SIZE; index++) {
        assert(reports.report_two[index] == 0xff);
    }
}

static void test_gates_compact_legacy_report_two(void) {
    WheelOutputReports reports;
    wheel_output_reports_init(&reports);
    const uint8_t packed[4] = {0xff, 0xff, 0xff, 0xff};

    assert(!wheel_output_reports_queue_packed(&reports, 2, packed, 0x0f));
    assert(wheel_output_reports_queue_packed(&reports, 1, packed, 0x17));
    wheel_output_reports_set_interface_mode_gate(&reports, true);
    assert(wheel_output_reports_queue_packed(&reports, 2, packed, 0x17));
    assert(!wheel_output_reports_queue_packed(&reports, 3, packed, 0));
    assert(!wheel_output_reports_queue_packed(NULL, 1, packed, 0));
    assert(!wheel_output_reports_queue_packed(&reports, 1, NULL, 0));
}

static void test_toggles_interface_gate_from_legacy_button_chord(void) {
    WheelOutputReports reports;
    wheel_output_reports_init(&reports);

    wheel_output_reports_update_interface_mode_gate(&reports, 0x9000, 0);
    assert(!wheel_output_reports_interface_mode_gate(&reports));
    wheel_output_reports_update_interface_mode_gate(&reports, 0x9000, 1);
    assert(wheel_output_reports_interface_mode_gate(&reports));
    wheel_output_reports_update_interface_mode_gate(&reports, 0x9000, 202);
    assert(wheel_output_reports_interface_mode_gate(&reports));

    wheel_output_reports_update_interface_mode_gate(&reports, 0x1000, 202);
    wheel_output_reports_update_interface_mode_gate(&reports, 0x9000, 201);
    assert(wheel_output_reports_interface_mode_gate(&reports));
    wheel_output_reports_update_interface_mode_gate(&reports, 0x9000, 202);
    assert(!wheel_output_reports_interface_mode_gate(&reports));
}

static void test_gates_extended_reports(void) {
    WheelOutputReports reports;
    wheel_output_reports_init(&reports);
    uint8_t arguments[26];
    uint8_t frame[33] = {0};

    fill_arguments(arguments, 2, 0x40);
    wheel_output_reports_apply(&reports, arguments, 0, 0);
    fill_arguments(arguments, 3, 0x50);
    wheel_output_reports_apply(&reports, arguments, 0, 0);
    assert(!wheel_output_reports_encode_next(&reports, 0, frame));

    wheel_output_reports_apply(&reports, arguments, 0, 1);
    fill_arguments(arguments, 2, 0x40);
    wheel_output_reports_apply(&reports, arguments, 0x0f, 0);

    assert(wheel_output_reports_encode_next(&reports, 0, frame));
    assert(frame[1] == 4);
    for (uint8_t index = 0; index < WHEEL_OUTPUT_REPORT_FOUR_SIZE; index++) {
        assert(frame[index + 2] == (uint8_t)(0x40 + index));
    }
    assert(wheel_output_reports_encode_next(&reports, 0, frame));
    assert(frame[1] == 5);
    for (uint8_t index = 0; index < WHEEL_OUTPUT_REPORT_FIVE_SIZE; index++) {
        assert(frame[index + 2] == (uint8_t)(0x50 + index));
    }
}

static void test_ignores_unknown_actions(void) {
    WheelOutputReports reports;
    wheel_output_reports_init(&reports);
    uint8_t arguments[26] = {4};
    uint8_t frame[33] = {0};

    assert(!wheel_output_reports_apply(&reports, arguments, 0x0f, 1));
    assert(!wheel_output_reports_encode_next(&reports, 0, frame));
}

static void test_queues_report_six_from_shared_report_four_payload(void) {
    WheelOutputReports reports;
    wheel_output_reports_init(&reports);
    uint8_t arguments[26];
    uint8_t frame[33] = {0};

    fill_arguments(arguments, WHEEL_OUTPUT_REPORT_ACTION_FOUR, 0x40);
    wheel_output_reports_apply(&reports, arguments, 0, 1);
    wheel_output_reports_queue_six(&reports, 0xa5, 0x5a);

    assert(wheel_output_reports_encode_next(&reports, 0, frame));
    assert(frame[1] == 4);
    assert(frame[2] == 0xa5);
    assert(frame[3] == 0x5a);
    for (uint8_t index = 2; index < WHEEL_OUTPUT_REPORT_FOUR_SIZE; index++) {
        assert(frame[index + 2] == (uint8_t)(0x40 + index));
    }

    assert(wheel_output_reports_encode_next(&reports, 0, frame));
    assert(frame[1] == 6);
    assert(frame[2] == 0xa5);
    assert(frame[3] == 0x5a);
    for (uint8_t index = 2; index < WHEEL_OUTPUT_REPORT_FOUR_SIZE; index++) {
        assert(frame[index + 2] == (uint8_t)(0x40 + index));
    }
    assert(!wheel_output_reports_encode_next(&reports, 0, frame));
}

static void test_streams_report_seventeen_after_direct_reports(void) {
    WheelOutputReports reports;
    wheel_output_reports_init(&reports);
    uint8_t payload[WHEEL_OUTPUT_REPORT_SEVENTEEN_SIZE];
    uint8_t arguments[26];
    uint8_t frame[33] = {0};

    for (uint8_t index = 0; index < WHEEL_OUTPUT_REPORT_SEVENTEEN_SIZE; index++) {
        payload[index] = index;
    }
    wheel_output_reports_queue_seventeen(&reports, payload);
    fill_arguments(arguments, 1, 0x80);
    wheel_output_reports_apply(&reports, arguments, 0, 0);

    assert(wheel_output_reports_encode_next(&reports, 0, frame));
    assert(frame[1] == 1);

    assert(wheel_output_reports_encode_next(&reports, 0, frame));
    assert(frame[1] == 3);
    assert(frame[2] == 0x0f);
    for (uint8_t index = 1; index < 30; index++) {
        assert(frame[index + 2] == index);
    }

    assert(wheel_output_reports_encode_next(&reports, 0, frame));
    assert(frame[1] == 3);
    for (uint8_t index = 0; index < 30; index++) {
        assert(frame[index + 2] == (uint8_t)(index + 30));
    }

    assert(wheel_output_reports_encode_next(&reports, 0, frame));
    assert(frame[1] == 3);
    assert(frame[2] == 0x1e);
    for (uint8_t index = 1; index < 30; index++) {
        assert(frame[index + 2] == index);
    }
    assert(!wheel_output_reports_encode_next(&reports, 0, frame));
}

static void test_repeats_remote_telemetry_after_report_seventeen(void) {
    WheelOutputReports reports;
    wheel_output_reports_init(&reports);
    uint8_t report_seventeen[WHEEL_OUTPUT_REPORT_SEVENTEEN_SIZE] = {0};
    uint8_t telemetry[WHEEL_OUTPUT_REMOTE_TELEMETRY_SIZE];
    uint8_t replacement[WHEEL_OUTPUT_REMOTE_TELEMETRY_SIZE] = {0};
    uint8_t frame[33] = {0};

    for (uint8_t index = 0; index < WHEEL_OUTPUT_REMOTE_TELEMETRY_SIZE; index++) {
        telemetry[index] = (uint8_t)(0x60 + index);
    }
    wheel_output_reports_queue_seventeen(&reports, report_seventeen);
    assert(wheel_output_reports_queue_remote_telemetry(&reports, telemetry));
    assert(wheel_output_reports_remote_telemetry_pending(&reports));
    assert(!wheel_output_reports_queue_remote_telemetry(&reports, replacement));

    for (uint8_t index = 0; index < 3; index++) {
        assert(wheel_output_reports_encode_next(&reports, 0, frame));
    }
    assert(wheel_output_reports_remote_telemetry_pending(&reports));

    for (uint8_t transmission = 0; transmission < 3; transmission++) {
        assert(wheel_output_reports_encode_next(&reports, 0, frame));
        assert(frame[1] == 3);
        for (uint8_t index = 0; index < WHEEL_OUTPUT_REMOTE_TELEMETRY_SIZE; index++) {
            assert(frame[index + 2] == telemetry[index]);
        }
    }
    assert(!wheel_output_reports_remote_telemetry_pending(&reports));
    assert(!wheel_output_reports_encode_next(&reports, 0, frame));
}

static void test_sends_native_display_command_after_report_seventeen(void) {
    WheelOutputReports reports;
    wheel_output_reports_init(&reports);
    uint8_t report_seventeen[WHEEL_OUTPUT_REPORT_SEVENTEEN_SIZE] = {0};
    uint8_t frame[33];
    memset(frame, 0xff, sizeof(frame));

    wheel_output_reports_queue_seventeen(&reports, report_seventeen);
    wheel_output_reports_queue_display_command(&reports, 0x0a);
    for (uint8_t transmission = 0; transmission < 3; transmission++) {
        assert(wheel_output_reports_encode_next(&reports, 0x10, frame));
    }

    assert(wheel_output_reports_encode_next(&reports, 0x10, frame));
    assert(frame[0] == 0xa6);
    assert(frame[1] == 0x82);
    assert(frame[2] == 0x0a);
    for (uint8_t index = 3; index < 32; index++) {
        assert(frame[index] == 0);
    }
    assert(!wheel_output_reports_encode_next(&reports, 0x10, frame));
}

static void test_sends_button_illumination_changes_to_remote_tuning_modes(void) {
    WheelOutputReports reports;
    wheel_output_reports_init(&reports);
    uint8_t frame[33] = {0};

    wheel_output_reports_set_button_illumination(&reports, true);
    assert(!wheel_output_reports_encode_next(&reports, 0, frame));
    assert(wheel_output_reports_encode_next(&reports, 0x0e, frame));
    assert(frame[1] == 0x16);
    assert(frame[2] == 1);
    assert(!wheel_output_reports_encode_next(&reports, 0x0e, frame));

    wheel_output_reports_set_button_illumination(&reports, false);
    assert(!wheel_output_reports_encode_next(&reports, 0, frame));
    assert(wheel_output_reports_encode_next(&reports, 0x1c, frame));
    assert(frame[1] == 0x16);
    assert(frame[2] == 0);
}

int main(void) {
    test_encodes_reports_in_priority_order();
    test_suppresses_legacy_report_two_while_interface_gate_is_closed();
    test_expands_compact_report_groups();
    test_gates_compact_legacy_report_two();
    test_toggles_interface_gate_from_legacy_button_chord();
    test_gates_extended_reports();
    test_ignores_unknown_actions();
    test_queues_report_six_from_shared_report_four_payload();
    test_streams_report_seventeen_after_direct_reports();
    test_sends_native_display_command_after_report_seventeen();
    test_repeats_remote_telemetry_after_report_seventeen();
    test_sends_button_illumination_changes_to_remote_tuning_modes();
    return 0;
}
