#include <assert.h>
#include <stdint.h>
#include <string.h>

#include "remote_tuning/telemetry.h"

static void write_u32(uint8_t output[4], uint32_t value) {
    output[0] = (uint8_t)value;
    output[1] = (uint8_t)(value >> 8);
    output[2] = (uint8_t)(value >> 16);
    output[3] = (uint8_t)(value >> 24);
}

static void write_float(uint8_t output[4], float value) {
    uint32_t bits;
    memcpy(&bits, &value, sizeof(bits));
    write_u32(output, bits);
}

static void drain_control_records(RemoteTelemetry *telemetry) {
    uint8_t record[REMOTE_TELEMETRY_SUBSCRIPTION_SIZE];
    while (remote_telemetry_take_control_record(telemetry, record)) {
    }
}

static void test_exposes_metric_subscriptions(void) {
    static const struct {
        RemoteTelemetryMetric metric;
        uint16_t keys[2];
        uint8_t selectors[2];
        uint8_t formats[2];
        uint8_t count;
    } cases[] = {
        {REMOTE_TELEMETRY_SPEED, {1}, {0x80}, {0x34}, 1},
        {REMOTE_TELEMETRY_RPM, {2, 3}, {0, 1}, {0x06, 0x06}, 2},
        {REMOTE_TELEMETRY_GEAR, {4}, {0}, {0xa2}, 1},
        {REMOTE_TELEMETRY_POSITION, {501}, {0x80}, {0x24}, 1},
        {REMOTE_TELEMETRY_LAP, {505}, {0x80}, {0x24}, 1},
        {REMOTE_TELEMETRY_FUEL, {5, 6}, {0x80, 0x81}, {0x18, 0x18}, 2},
        {REMOTE_TELEMETRY_DRS, {14, 15}, {0, 1}, {0x41, 0x12}, 2},
        {REMOTE_TELEMETRY_DRIVER_AIDS, {18, 20}, {0, 1}, {0x22, 0x22}, 2},
        {REMOTE_TELEMETRY_ERS, {9}, {0}, {0x09}, 1},
        {REMOTE_TELEMETRY_DELTA, {516}, {0}, {0x1a}, 1},
    };

    RemoteTelemetry telemetry;

    for (uint8_t case_index = 0; case_index < sizeof(cases) / sizeof(cases[0]); case_index++) {
        remote_telemetry_init(&telemetry);
        assert(remote_telemetry_subscription_count(&telemetry) == 0);
        assert(remote_telemetry_select(&telemetry, cases[case_index].metric));
        assert(remote_telemetry_subscription_count(&telemetry) == cases[case_index].count);
        for (uint8_t channel = 0; channel < cases[case_index].count; channel++) {
            RemoteTelemetrySubscription subscription;
            uint8_t encoded[REMOTE_TELEMETRY_SUBSCRIPTION_SIZE];
            uint8_t queued[REMOTE_TELEMETRY_SUBSCRIPTION_SIZE];
            assert(remote_telemetry_subscription(&telemetry, channel, &subscription));
            assert(subscription.key == cases[case_index].keys[channel]);
            assert(subscription.selector == cases[case_index].selectors[channel]);
            assert(subscription.format == cases[case_index].formats[channel]);
            remote_telemetry_encode_subscription(&subscription, encoded);
            assert(encoded[0] == 2);
            assert(encoded[1] == subscription.selector);
            assert(encoded[2] == (uint8_t)subscription.key);
            assert(encoded[3] == (uint8_t)(subscription.key >> 8));
            assert(encoded[4] == subscription.format);
            assert(remote_telemetry_take_control_record(&telemetry, queued));
            assert(memcmp(queued, encoded, sizeof(queued)) == 0);
        }
        uint8_t empty[REMOTE_TELEMETRY_SUBSCRIPTION_SIZE];
        assert(!remote_telemetry_take_control_record(&telemetry, empty));
    }

    assert(remote_telemetry_select(&telemetry, REMOTE_TELEMETRY_NONE));
    assert(remote_telemetry_subscription_count(&telemetry) == 0);
    for (uint8_t channel = 0; channel < cases[sizeof(cases) / sizeof(cases[0]) - 1].count;
         channel++) {
        uint8_t clear[REMOTE_TELEMETRY_SUBSCRIPTION_SIZE];
        assert(remote_telemetry_take_control_record(&telemetry, clear));
        assert(clear[0] == 2 && clear[1] == channel);
        assert(clear[2] == 0xff && clear[3] == 0xff && clear[4] == 0x1a);
    }
    assert(!remote_telemetry_select(&telemetry, (RemoteTelemetryMetric)11));
}

static void test_formats_speed_and_overlay(void) {
    RemoteTelemetry telemetry;
    uint8_t report[REMOTE_TELEMETRY_REPORT_SIZE];
    const uint8_t speed[] = {123, 0};
    const uint8_t overlay[] = {'m', 'p', 'h'};

    remote_telemetry_init(&telemetry);
    assert(remote_telemetry_select(&telemetry, REMOTE_TELEMETRY_SPEED));
    assert(remote_telemetry_apply_primary(&telemetry, 0, 1, speed, sizeof(speed)) ==
           REMOTE_TELEMETRY_RECORD_APPLIED);
    assert(remote_telemetry_apply_overlay(&telemetry, 0, 1, overlay, sizeof(overlay)) ==
           REMOTE_TELEMETRY_RECORD_APPLIED);
    assert(remote_telemetry_take_report(&telemetry, report));
    assert(report[0] == 1);
    assert(report[1] == 6);
    assert(memcmp(report + 2, "123mph", 6) == 0);
    assert(report[14] == 0x00 && report[15] == 0x10);
    assert(report[16] == 5);
    assert(memcmp(report + 17, "SPEED", 5) == 0);
    assert(report[25] == 0x00 && report[26] == 0x30 && report[27] == 0x00);
    assert(report[28] == 0 && report[29] == 0);
    assert(!remote_telemetry_take_report(&telemetry, report));
}

static void test_scales_rpm_and_services_both_channels(void) {
    RemoteTelemetry telemetry;
    uint8_t report[REMOTE_TELEMETRY_REPORT_SIZE];
    uint8_t rpm[4];
    uint8_t limit[4];
    write_u32(rpm, 6000);
    write_u32(limit, 8000);

    remote_telemetry_init(&telemetry);
    assert(remote_telemetry_select(&telemetry, REMOTE_TELEMETRY_RPM));
    assert(remote_telemetry_apply_primary(&telemetry, 0, 2, rpm, sizeof(rpm)) ==
           REMOTE_TELEMETRY_RECORD_APPLIED);
    assert(remote_telemetry_apply_primary(&telemetry, 1, 3, limit, sizeof(limit)) ==
           REMOTE_TELEMETRY_RECORD_APPLIED);
    assert(remote_telemetry_take_report(&telemetry, report));
    assert(report[1] == 4 && memcmp(report + 2, "6000", 4) == 0);
    assert(report[16] == 3 && memcmp(report + 17, "RPM", 3) == 0);
    assert(report[28] == 127 && report[29] == 96);
    assert(remote_telemetry_take_report(&telemetry, report));
    assert(report[28] == 127 && report[29] == 96);
    assert(!remote_telemetry_take_report(&telemetry, report));
}

static void test_formats_specialized_metrics(void) {
    RemoteTelemetry telemetry;
    uint8_t report[REMOTE_TELEMETRY_REPORT_SIZE];

    const uint8_t reverse = UINT8_MAX;
    remote_telemetry_init(&telemetry);
    assert(remote_telemetry_select(&telemetry, REMOTE_TELEMETRY_GEAR));
    assert(remote_telemetry_apply_primary(&telemetry, 0, 4, &reverse, 1) ==
           REMOTE_TELEMETRY_RECORD_APPLIED);
    assert(remote_telemetry_take_report(&telemetry, report));
    assert(report[0] == 2 && report[1] == 3 && memcmp(report + 2, " r ", 3) == 0);

    const uint8_t drs[] = {'O', 'N'};
    const uint8_t enabled = 1;
    assert(remote_telemetry_select(&telemetry, REMOTE_TELEMETRY_DRS));
    assert(remote_telemetry_apply_primary(&telemetry, 0, 14, drs, sizeof(drs)) ==
           REMOTE_TELEMETRY_RECORD_APPLIED);
    assert(remote_telemetry_apply_primary(&telemetry, 1, 15, &enabled, 1) ==
           REMOTE_TELEMETRY_RECORD_APPLIED);
    assert(remote_telemetry_take_report(&telemetry, report));
    assert(report[1] == 4 && memcmp(report + 2, "ON  ", 4) == 0 && report[29] == 1);

    uint8_t delta[4];
    write_float(delta, -1.25f);
    assert(remote_telemetry_select(&telemetry, REMOTE_TELEMETRY_DELTA));
    assert(remote_telemetry_apply_primary(&telemetry, 0, 516, delta, sizeof(delta)) ==
           REMOTE_TELEMETRY_RECORD_APPLIED);
    assert(remote_telemetry_take_report(&telemetry, report));
    assert(report[1] == 5 && memcmp(report + 2, "-1.25", 5) == 0);
}

static void test_scales_fuel_and_classifies_stale_records(void) {
    RemoteTelemetry telemetry;
    uint8_t report[REMOTE_TELEMETRY_REPORT_SIZE];
    uint8_t fuel[4];
    uint8_t capacity[4];
    const uint8_t overlay[] = {' ', 'L'};
    write_float(fuel, 25.5f);
    write_float(capacity, 50.0f);

    remote_telemetry_init(&telemetry);
    assert(remote_telemetry_select(&telemetry, REMOTE_TELEMETRY_FUEL));
    assert(remote_telemetry_apply_primary(&telemetry, 0, 5, fuel, sizeof(fuel)) ==
           REMOTE_TELEMETRY_RECORD_APPLIED);
    assert(remote_telemetry_apply_overlay(&telemetry, 0, 5, overlay, sizeof(overlay)) ==
           REMOTE_TELEMETRY_RECORD_APPLIED);
    assert(remote_telemetry_apply_primary(&telemetry, 1, 6, capacity, sizeof(capacity)) ==
           REMOTE_TELEMETRY_RECORD_APPLIED);
    assert(remote_telemetry_take_report(&telemetry, report));
    assert(report[1] == 6 && memcmp(report + 2, "25.5 L", 6) == 0);
    assert(report[28] == 100 && report[29] == 51);

    drain_control_records(&telemetry);
    assert(remote_telemetry_apply_primary(&telemetry, 0, 14, fuel, sizeof(fuel)) ==
           REMOTE_TELEMETRY_CLEAR_REQUESTED);
    uint8_t clear[REMOTE_TELEMETRY_SUBSCRIPTION_SIZE];
    const uint8_t primary_clear[] = {2, 0, 0xff, 0xff, 0};
    assert(remote_telemetry_take_control_record(&telemetry, clear));
    assert(memcmp(clear, primary_clear, sizeof(clear)) == 0);
    assert(remote_telemetry_apply_overlay(&telemetry, 0, 14, overlay, sizeof(overlay)) ==
           REMOTE_TELEMETRY_RECORD_IGNORED);
    assert(remote_telemetry_apply_overlay(&telemetry, 1, 14, overlay, sizeof(overlay)) ==
           REMOTE_TELEMETRY_RECORD_IGNORED);

    assert(remote_telemetry_select(&telemetry, REMOTE_TELEMETRY_DRS));
    drain_control_records(&telemetry);
    assert(remote_telemetry_apply_overlay(&telemetry, 0, 5, overlay, sizeof(overlay)) ==
           REMOTE_TELEMETRY_CLEAR_REQUESTED);
    const uint8_t overlay_clear[] = {2, 0x80, 0xff, 0xff, 0};
    assert(remote_telemetry_take_control_record(&telemetry, clear));
    assert(memcmp(clear, overlay_clear, sizeof(clear)) == 0);
}

static void test_queues_encoded_host_controls(void) {
    RemoteTelemetry telemetry;
    remote_telemetry_init(&telemetry);
    const uint8_t record[] = {2, 0x81, 0x34, 0x12, 0x56};
    assert(remote_telemetry_queue_control_record(&telemetry, record));

    uint8_t output[REMOTE_TELEMETRY_SUBSCRIPTION_SIZE];
    assert(remote_telemetry_take_control_record(&telemetry, output));
    assert(memcmp(output, record, sizeof(output)) == 0);
}

static void test_rejects_invalid_api_inputs(void) {
    RemoteTelemetry telemetry;
    RemoteTelemetrySubscription subscription;
    uint8_t payload[REMOTE_TELEMETRY_SUBSCRIPTION_SIZE] = {0};
    uint8_t output[REMOTE_TELEMETRY_REPORT_SIZE];

    remote_telemetry_init(&telemetry);
    assert(!remote_telemetry_select(NULL, REMOTE_TELEMETRY_SPEED));
    assert(remote_telemetry_subscription_count(NULL) == 0);
    assert(!remote_telemetry_subscription(NULL, 0, &subscription));
    assert(!remote_telemetry_subscription(&telemetry, 0, NULL));
    assert(!remote_telemetry_subscription(&telemetry, 0, &subscription));
    assert(!remote_telemetry_queue_control_record(NULL, payload));
    assert(!remote_telemetry_queue_control_record(&telemetry, NULL));
    assert(!remote_telemetry_take_control_record(NULL, payload));
    assert(!remote_telemetry_take_control_record(&telemetry, NULL));
    assert(remote_telemetry_apply_primary(NULL, 0, 1, payload, sizeof(payload)) ==
           REMOTE_TELEMETRY_RECORD_IGNORED);
    assert(remote_telemetry_apply_primary(&telemetry, 0, 1, NULL, sizeof(payload)) ==
           REMOTE_TELEMETRY_RECORD_IGNORED);
    assert(remote_telemetry_apply_primary(&telemetry, 2, 1, payload, sizeof(payload)) ==
           REMOTE_TELEMETRY_RECORD_IGNORED);
    assert(remote_telemetry_apply_primary(&telemetry, 0, 0, payload, sizeof(payload)) ==
           REMOTE_TELEMETRY_RECORD_IGNORED);
    assert(remote_telemetry_apply_primary(&telemetry, 0, 1008, payload, sizeof(payload)) ==
           REMOTE_TELEMETRY_RECORD_IGNORED);
    assert(remote_telemetry_apply_overlay(NULL, 0, 1, payload, sizeof(payload)) ==
           REMOTE_TELEMETRY_RECORD_IGNORED);
    assert(remote_telemetry_apply_overlay(&telemetry, 0, 1, NULL, sizeof(payload)) ==
           REMOTE_TELEMETRY_RECORD_IGNORED);
    assert(remote_telemetry_apply_overlay(&telemetry, 2, 1, payload, sizeof(payload)) ==
           REMOTE_TELEMETRY_RECORD_IGNORED);
    assert(remote_telemetry_apply_overlay(&telemetry, 0, 0, payload, sizeof(payload)) ==
           REMOTE_TELEMETRY_RECORD_IGNORED);
    assert(remote_telemetry_apply_overlay(&telemetry, 0, 1008, payload, sizeof(payload)) ==
           REMOTE_TELEMETRY_RECORD_IGNORED);
    assert(!remote_telemetry_take_report(NULL, output));
    assert(!remote_telemetry_take_report(&telemetry, NULL));
    assert(!remote_telemetry_take_report(&telemetry, output));
}

static void test_formats_all_primary_encodings_and_lengths(void) {
    static const uint8_t formats[] = {0x41, 0x42, 0x43, 0x44, 0x45, 0x46,
                                      0x47, 0x28, 0x29, 0x1a, 0x0b};
    static const uint8_t required_sizes[] = {0, 1, 1, 2, 2, 4, 4, 4, 2, 4, 0};
    uint8_t payload[16] = {0x85, 0xff, 0xff, 0x7f};

    for (uint8_t index = 0; index < sizeof(formats); index++) {
        RemoteTelemetry telemetry;
        uint8_t report[REMOTE_TELEMETRY_REPORT_SIZE];
        remote_telemetry_init(&telemetry);
        telemetry.metric = REMOTE_TELEMETRY_SPEED;
        telemetry.source_count = 1;
        telemetry.sources[0].report_id = 1;
        telemetry.channels[0].key = 1;
        telemetry.channels[0].format = formats[index];
        telemetry.channels[0].behavior = 1;
        telemetry.channels[0].source_slot = 0;

        if (required_sizes[index] != 0) {
            assert(remote_telemetry_apply_primary(&telemetry, 0, 1, payload,
                                                  required_sizes[index] - 1) ==
                   REMOTE_TELEMETRY_RECORD_IGNORED);
        }
        RemoteTelemetryRecordResult expected = formats[index] == 0x0b
                                                   ? REMOTE_TELEMETRY_RECORD_IGNORED
                                                   : REMOTE_TELEMETRY_RECORD_APPLIED;
        assert(remote_telemetry_apply_primary(&telemetry, 0, 1, payload, sizeof(payload)) ==
               expected);
        if (expected == REMOTE_TELEMETRY_RECORD_APPLIED) {
            assert(remote_telemetry_take_report(&telemetry, report));
            assert(report[1] != 0);
        }
    }

    RemoteTelemetry telemetry;
    uint8_t report[REMOTE_TELEMETRY_REPORT_SIZE];
    remote_telemetry_init(&telemetry);
    telemetry.metric = REMOTE_TELEMETRY_SPEED;
    telemetry.source_count = 1;
    telemetry.channels[0].key = 1;
    telemetry.channels[0].format = 0x11;
    telemetry.channels[0].behavior = 1;
    telemetry.channels[0].source_slot = 0;
    assert(remote_telemetry_apply_primary(&telemetry, 0, 1, payload, sizeof(payload)) ==
           REMOTE_TELEMETRY_RECORD_APPLIED);
    assert(telemetry.sources[0].payload_length == 1);
    telemetry.channels[0].overlay_length = 4;
    telemetry.sources[0].payload_length = sizeof(telemetry.sources[0].payload);
    assert(!remote_telemetry_take_report(&telemetry, report));
}

static void test_handles_queue_capacity_and_selection_atomicity(void) {
    RemoteTelemetry telemetry;
    uint8_t input[REMOTE_TELEMETRY_SUBSCRIPTION_SIZE] = {2, 0, 1, 0, 0x34};
    uint8_t output[REMOTE_TELEMETRY_SUBSCRIPTION_SIZE];
    remote_telemetry_init(&telemetry);

    for (uint8_t index = 0; index < REMOTE_TELEMETRY_CONTROL_QUEUE_COUNT; index++) {
        input[1] = index;
        assert(remote_telemetry_queue_control_record(&telemetry, input));
    }
    assert(!remote_telemetry_queue_control_record(&telemetry, input));
    assert(!remote_telemetry_select(&telemetry, REMOTE_TELEMETRY_SPEED));
    for (uint8_t index = 0; index < REMOTE_TELEMETRY_CONTROL_QUEUE_COUNT; index++) {
        assert(remote_telemetry_take_control_record(&telemetry, output));
        assert(output[1] == index);
    }

    for (uint8_t pass = 0; pass < 2; pass++) {
        for (uint8_t index = 0; index < REMOTE_TELEMETRY_CONTROL_QUEUE_COUNT / 2; index++) {
            input[1] = (uint8_t)(pass * 16 + index);
            assert(remote_telemetry_queue_control_record(&telemetry, input));
        }
        for (uint8_t index = 0; index < REMOTE_TELEMETRY_CONTROL_QUEUE_COUNT / 2; index++) {
            assert(remote_telemetry_take_control_record(&telemetry, output));
            assert(output[1] == (uint8_t)(pass * 16 + index));
        }
    }
    assert(remote_telemetry_select(&telemetry, REMOTE_TELEMETRY_SPEED));
    assert(remote_telemetry_select(&telemetry, REMOTE_TELEMETRY_SPEED));
}

int main(void) {
    test_exposes_metric_subscriptions();
    test_formats_speed_and_overlay();
    test_scales_rpm_and_services_both_channels();
    test_formats_specialized_metrics();
    test_scales_fuel_and_classifies_stale_records();
    test_queues_encoded_host_controls();
    test_rejects_invalid_api_inputs();
    test_formats_all_primary_encodings_and_lengths();
    test_handles_queue_capacity_and_selection_atomicity();
    return 0;
}
