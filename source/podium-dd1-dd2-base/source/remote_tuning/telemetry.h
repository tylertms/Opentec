#ifndef OPENTEC_BASE_REMOTE_TUNING_TELEMETRY_H
#define OPENTEC_BASE_REMOTE_TUNING_TELEMETRY_H

#include <stdbool.h>
#include <stdint.h>

/** @brief Fixed capacities of the remote telemetry service. */
enum {
    REMOTE_TELEMETRY_CHANNEL_COUNT = 2,        /**< Maximum channels in one metric. */
    REMOTE_TELEMETRY_CONTROL_QUEUE_COUNT = 32, /**< Number of queued control records. */
    REMOTE_TELEMETRY_REPORT_SIZE = 30,         /**< Encoded wheel report size in bytes. */
    REMOTE_TELEMETRY_SUBSCRIPTION_SIZE = 5,    /**< Encoded subscription record size in bytes. */
};

/** @brief Selectable remote telemetry metrics. */
typedef enum {
    REMOTE_TELEMETRY_NONE = 0,        /**< No metric selected. */
    REMOTE_TELEMETRY_SPEED = 1,       /**< Vehicle speed. */
    REMOTE_TELEMETRY_RPM = 2,         /**< Engine speed. */
    REMOTE_TELEMETRY_GEAR = 3,        /**< Current gear. */
    REMOTE_TELEMETRY_POSITION = 4,    /**< Race position. */
    REMOTE_TELEMETRY_LAP = 5,         /**< Current lap. */
    REMOTE_TELEMETRY_FUEL = 6,        /**< Fuel level. */
    REMOTE_TELEMETRY_DRS = 7,         /**< Drag reduction system state. */
    REMOTE_TELEMETRY_DRIVER_AIDS = 8, /**< Driver-aid states. */
    REMOTE_TELEMETRY_ERS = 9,         /**< Energy recovery system state. */
    REMOTE_TELEMETRY_DELTA = 10,      /**< Lap delta. */
} RemoteTelemetryMetric;

/** @brief Result of applying one host telemetry record. */
typedef enum {
    REMOTE_TELEMETRY_RECORD_IGNORED,  /**< Record was rejected or had no effect. */
    REMOTE_TELEMETRY_RECORD_APPLIED,  /**< Record content was applied. */
    REMOTE_TELEMETRY_CLEAR_REQUESTED, /**< Record key was stale and a clear was queued. */
} RemoteTelemetryRecordResult;

/** @brief One logical host subscription for a telemetry channel. */
typedef struct {
    uint16_t key;     /**< Host subscription key. */
    uint8_t selector; /**< Channel and overlay selector bits. */
    uint8_t format;   /**< Host value format and width or precision. */
} RemoteTelemetrySubscription;

/** @brief Dynamic content of one attached-wheel telemetry report. */
typedef struct {
    uint8_t payload[12];          /**< Primary report text or value bytes. */
    uint8_t secondary_payload[8]; /**< Secondary report label bytes. */
    uint16_t value;               /**< Fixed report value field. */
    uint8_t report_id;            /**< Attached-wheel report identifier. */
    uint8_t payload_length;       /**< Number of valid primary payload bytes. */
    uint8_t secondary_length;     /**< Number of valid secondary payload bytes. */
    uint8_t metadata[3];          /**< Report metadata bytes. */
    uint8_t scale_limit;          /**< Maximum scale value. */
    uint8_t scale_value;          /**< Current scale value. */
} RemoteTelemetrySource;

/** @brief Host mapping and update state for one telemetry channel. */
typedef struct {
    uint32_t cached_integer;  /**< Cached integer used for range scaling. */
    float cached_float;       /**< Cached float used for range scaling. */
    uint16_t key;             /**< Expected host subscription key. */
    uint8_t format;           /**< Host value format and width or precision. */
    uint8_t behavior;         /**< Internal channel behavior flags. */
    uint8_t source_slot;      /**< Index of the report source used by this channel. */
    uint8_t overlay[4];       /**< Current overlay bytes. */
    uint8_t overlay_capacity; /**< Maximum accepted overlay bytes. */
    uint8_t overlay_length;   /**< Configured fixed-width overlay bytes. */
    bool overlay_enabled;     /**< True when overlay records are accepted. */
    bool dirty;               /**< True when a report is waiting to be emitted. */
} RemoteTelemetryChannel;

/** @brief Selected telemetry metric and its attached-wheel report state. */
typedef struct {
    RemoteTelemetrySource sources[REMOTE_TELEMETRY_CHANNEL_COUNT]; /**< Dynamic report sources. */
    RemoteTelemetryChannel
        channels[REMOTE_TELEMETRY_CHANNEL_COUNT]; /**< Active channel mappings. */
    RemoteTelemetryMetric metric;                 /**< Currently selected metric. */
    uint8_t control_records[REMOTE_TELEMETRY_CONTROL_QUEUE_COUNT]
                           [REMOTE_TELEMETRY_SUBSCRIPTION_SIZE]; /**< Queued control records. */
    uint8_t source_count;   /**< Number of active report sources. */
    uint8_t report_channel; /**< Channel scheduled for the next report. */
    uint8_t control_head;   /**< Queue index of the oldest control record. */
    uint8_t control_count;  /**< Number of queued control records. */
} RemoteTelemetry;

/**
 * @brief Initializes remote telemetry state.
 *
 * Clears the selected metric, channel mappings, report sources, control queue, and scheduler.
 *
 * @param[out] telemetry Telemetry state to initialize.
 */
void remote_telemetry_init(RemoteTelemetry *telemetry);

/**
 * @brief Selects one remote telemetry metric.
 *
 * When the metric changes, replaces active channel mappings and queues clears and subscriptions
 * for the selected metric.
 *
 * @param[in,out] telemetry Telemetry state to update.
 * @param[in] metric Metric to select, or REMOTE_TELEMETRY_NONE to clear selection.
 * @return true when metric is supported and selected or queued; false otherwise.
 */
bool remote_telemetry_select(RemoteTelemetry *telemetry, RemoteTelemetryMetric metric);

/**
 * @brief Returns the active subscription count.
 *
 * Reports the number of channels defined by the selected metric.
 *
 * @param[in] telemetry Telemetry state to inspect.
 * @return Active channel count, or zero when telemetry is null or no metric is selected.
 */
uint8_t remote_telemetry_subscription_count(const RemoteTelemetry *telemetry);

/**
 * @brief Reads one active subscription.
 *
 * Copies the channel key, selector, and format for the requested active channel.
 *
 * @param[in] telemetry Telemetry state to inspect.
 * @param[in] channel Active channel index.
 * @param[out] subscription Subscription to fill.
 * @return true when channel is active and subscription is filled; false otherwise.
 */
bool remote_telemetry_subscription(const RemoteTelemetry *telemetry, uint8_t channel,
                                   RemoteTelemetrySubscription *subscription);

/**
 * @brief Encodes one subscription record.
 *
 * Writes route, selector, little-endian key, and format into the fixed-size output buffer.
 *
 * @param[in] subscription Subscription fields to encode.
 * @param[out] output Buffer receiving REMOTE_TELEMETRY_SUBSCRIPTION_SIZE bytes.
 */
void remote_telemetry_encode_subscription(const RemoteTelemetrySubscription *subscription,
                                          uint8_t output[REMOTE_TELEMETRY_SUBSCRIPTION_SIZE]);

/**
 * @brief Queues one host telemetry control record.
 *
 * Appends the fixed-size input record in arrival order when queue capacity remains.
 *
 * @param[in,out] telemetry Telemetry state owning the queue.
 * @param[in] input Encoded subscription or clear record.
 * @return true when input is queued; false when inputs are invalid or the queue is full.
 */
bool remote_telemetry_queue_control_record(RemoteTelemetry *telemetry,
                                           const uint8_t input[REMOTE_TELEMETRY_SUBSCRIPTION_SIZE]);

/**
 * @brief Takes the oldest queued control record.
 *
 * Copies and consumes one queued record while preserving the order of remaining records.
 *
 * @param[in,out] telemetry Telemetry state owning the queue.
 * @param[out] output Buffer receiving the oldest encoded record.
 * @return true when a record was available and copied; false otherwise.
 */
bool remote_telemetry_take_control_record(RemoteTelemetry *telemetry,
                                          uint8_t output[REMOTE_TELEMETRY_SUBSCRIPTION_SIZE]);

/**
 * @brief Applies one primary telemetry record.
 *
 * Validates the channel key, updates formatted content or scaling state, and marks the report
 * dirty.
 *
 * @param[in,out] telemetry Telemetry state to update.
 * @param[in] channel Target channel index.
 * @param[in] key Host record key.
 * @param[in] payload Host record payload.
 * @param[in] payload_length Number of payload bytes.
 * @return Record result: applied when the payload is accepted, clear requested for a stale key, or
 * ignored otherwise.
 */
RemoteTelemetryRecordResult remote_telemetry_apply_primary(RemoteTelemetry *telemetry,
                                                           uint8_t channel, uint16_t key,
                                                           const uint8_t *payload,
                                                           uint8_t payload_length);

/**
 * @brief Applies one overlay telemetry record.
 *
 * Copies accepted overlay bytes up to the configured fixed width, preserving trailing bytes, and
 * marks its report dirty.
 *
 * @param[in,out] telemetry Telemetry state to update.
 * @param[in] channel Target channel index.
 * @param[in] key Host record key.
 * @param[in] payload Overlay payload bytes.
 * @param[in] payload_length Number of payload bytes.
 * @return Record result: applied when accepted, clear requested for an unexpected non-overlay key,
 * or ignored otherwise.
 */
RemoteTelemetryRecordResult remote_telemetry_apply_overlay(RemoteTelemetry *telemetry,
                                                           uint8_t channel, uint16_t key,
                                                           const uint8_t *payload,
                                                           uint8_t payload_length);

/**
 * @brief Takes the next scheduled wheel report.
 *
 * Encodes the scheduled channel when any selected channel is dirty and consumes that channel's
 * dirty flag.
 *
 * @param[in,out] telemetry Telemetry state and report scheduler.
 * @param[out] output Buffer receiving REMOTE_TELEMETRY_REPORT_SIZE bytes.
 * @return true when a report was produced; false when no selected channel is dirty, the report
 * cannot be encoded, or inputs are invalid.
 */
bool remote_telemetry_take_report(RemoteTelemetry *telemetry,
                                  uint8_t output[REMOTE_TELEMETRY_REPORT_SIZE]);

#endif
