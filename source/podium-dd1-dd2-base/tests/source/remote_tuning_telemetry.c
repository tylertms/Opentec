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

int main(void) {
    test_exposes_metric_subscriptions();
    test_formats_speed_and_overlay();
    test_scales_rpm_and_services_both_channels();
    test_formats_specialized_metrics();
    test_scales_fuel_and_classifies_stale_records();
    return 0;
}
