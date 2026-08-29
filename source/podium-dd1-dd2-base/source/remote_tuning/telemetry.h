#ifndef OPENTEC_BASE_REMOTE_TUNING_TELEMETRY_H
#define OPENTEC_BASE_REMOTE_TUNING_TELEMETRY_H

#include <stdbool.h>
#include <stdint.h>

enum {
    REMOTE_TELEMETRY_CHANNEL_COUNT = 2,
    REMOTE_TELEMETRY_CONTROL_QUEUE_COUNT = 32,
    REMOTE_TELEMETRY_REPORT_SIZE = 30,
    REMOTE_TELEMETRY_SUBSCRIPTION_SIZE = 5,
};

/** @brief Selectable remote telemetry metrics. */
typedef enum {
    REMOTE_TELEMETRY_NONE = 0,
    REMOTE_TELEMETRY_SPEED = 1,
    REMOTE_TELEMETRY_RPM = 2,
    REMOTE_TELEMETRY_GEAR = 3,
    REMOTE_TELEMETRY_POSITION = 4,
    REMOTE_TELEMETRY_LAP = 5,
    REMOTE_TELEMETRY_FUEL = 6,
    REMOTE_TELEMETRY_DRS = 7,
    REMOTE_TELEMETRY_DRIVER_AIDS = 8,
    REMOTE_TELEMETRY_ERS = 9,
    REMOTE_TELEMETRY_DELTA = 10,
} RemoteTelemetryMetric;

/** @brief Result of applying one host telemetry record. */
typedef enum {
    REMOTE_TELEMETRY_RECORD_IGNORED,
    REMOTE_TELEMETRY_RECORD_APPLIED,
    REMOTE_TELEMETRY_CLEAR_REQUESTED,
} RemoteTelemetryRecordResult;

/** @brief One logical host subscription for a telemetry channel. */
typedef struct {
    uint16_t key;
    uint8_t selector;
    uint8_t format;
} RemoteTelemetrySubscription;

/** @brief Dynamic content of one attached-wheel telemetry report. */
typedef struct {
    uint8_t payload[12];
    uint8_t secondary_payload[8];
    uint16_t value;
    uint8_t report_id;
    uint8_t payload_length;
    uint8_t secondary_length;
    uint8_t metadata[3];
    uint8_t scale_limit;
    uint8_t scale_value;
} RemoteTelemetrySource;

/** @brief Host mapping and update state for one telemetry channel. */
typedef struct {
    uint32_t cached_integer;
    float cached_float;
    uint16_t key;
    uint8_t format;
    uint8_t behavior;
    uint8_t source_slot;
    uint8_t overlay[4];
    uint8_t overlay_capacity;
    uint8_t overlay_length;
    bool overlay_enabled;
    bool dirty;
} RemoteTelemetryChannel;

/** @brief Selected telemetry metric and its attached-wheel report state. */
typedef struct {
    RemoteTelemetrySource sources[REMOTE_TELEMETRY_CHANNEL_COUNT];
    RemoteTelemetryChannel channels[REMOTE_TELEMETRY_CHANNEL_COUNT];
    RemoteTelemetryMetric metric;
    uint8_t control_records[REMOTE_TELEMETRY_CONTROL_QUEUE_COUNT]
                           [REMOTE_TELEMETRY_SUBSCRIPTION_SIZE];
    uint8_t source_count;
    uint8_t report_channel;
    uint8_t control_head;
    uint8_t control_count;
} RemoteTelemetry;

void remote_telemetry_init(RemoteTelemetry *telemetry);
bool remote_telemetry_select(RemoteTelemetry *telemetry, RemoteTelemetryMetric metric);
uint8_t remote_telemetry_subscription_count(const RemoteTelemetry *telemetry);
bool remote_telemetry_subscription(const RemoteTelemetry *telemetry, uint8_t channel,
                                   RemoteTelemetrySubscription *subscription);
void remote_telemetry_encode_subscription(const RemoteTelemetrySubscription *subscription,
                                          uint8_t output[REMOTE_TELEMETRY_SUBSCRIPTION_SIZE]);
bool remote_telemetry_queue_control_record(RemoteTelemetry *telemetry,
                                           const uint8_t input[REMOTE_TELEMETRY_SUBSCRIPTION_SIZE]);
bool remote_telemetry_take_control_record(RemoteTelemetry *telemetry,
                                          uint8_t output[REMOTE_TELEMETRY_SUBSCRIPTION_SIZE]);
RemoteTelemetryRecordResult remote_telemetry_apply_primary(RemoteTelemetry *telemetry,
                                                           uint8_t channel, uint16_t key,
                                                           const uint8_t *payload,
                                                           uint8_t payload_length);
RemoteTelemetryRecordResult remote_telemetry_apply_overlay(RemoteTelemetry *telemetry,
                                                           uint8_t channel, uint16_t key,
                                                           const uint8_t *payload,
                                                           uint8_t payload_length);
bool remote_telemetry_take_report(RemoteTelemetry *telemetry,
                                  uint8_t output[REMOTE_TELEMETRY_REPORT_SIZE]);

#endif
