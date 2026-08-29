#include "remote_tuning/telemetry.h"

#include <stddef.h>
#include <string.h>

enum {
    REMOTE_TELEMETRY_ROUTE = 2,
    REMOTE_TELEMETRY_OVERLAY_SELECTOR = 0x80,
    REMOTE_TELEMETRY_MAXIMUM_KEY = 1007,
    REMOTE_TELEMETRY_FORMAT_TEXT = 1,
    REMOTE_TELEMETRY_FORMAT_UINT8 = 2,
    REMOTE_TELEMETRY_FORMAT_INT8 = 3,
    REMOTE_TELEMETRY_FORMAT_UINT16 = 4,
    REMOTE_TELEMETRY_FORMAT_INT16 = 5,
    REMOTE_TELEMETRY_FORMAT_UINT32 = 6,
    REMOTE_TELEMETRY_FORMAT_INT32 = 7,
    REMOTE_TELEMETRY_FORMAT_FLOAT = 8,
    REMOTE_TELEMETRY_FORMAT_UINT16_ALTERNATE = 9,
    REMOTE_TELEMETRY_FORMAT_SIGNED_FLOAT = 10,
    REMOTE_TELEMETRY_BEHAVIOR_PRIMARY = 1u << 0,
    REMOTE_TELEMETRY_BEHAVIOR_RANGE_SOURCE = 1u << 1,
    REMOTE_TELEMETRY_BEHAVIOR_SCALE_BYTE = 1u << 2,
    REMOTE_TELEMETRY_BEHAVIOR_RANGE_LIMIT = 1u << 3,
    REMOTE_TELEMETRY_BEHAVIOR_GEAR = 1u << 4,
};

typedef struct {
    uint16_t key;
    uint8_t format;
    uint8_t behavior;
    uint8_t source;
    uint8_t overlay_capacity;
    bool overlay_enabled;
} RemoteTelemetryChannelDefinition;

typedef struct {
    RemoteTelemetryChannelDefinition channels[REMOTE_TELEMETRY_CHANNEL_COUNT];
    uint8_t channel_count;
} RemoteTelemetryMetricDefinition;

typedef struct {
    const uint8_t *secondary_payload;
    uint16_t value;
    uint8_t report_id;
    uint8_t payload_offset;
    uint8_t payload_length;
    uint8_t secondary_length;
    uint8_t metadata[3];
    uint8_t scale_limit;
} RemoteTelemetrySourceDefinition;

static const uint8_t display_template[] =
    "---     ---    - ---    ---    ---          --------.--      ";
static const uint8_t speed_label[] = "SPEED";
static const uint8_t rpm_label[] = "RPM";
static const uint8_t gear_label[] = "GEAR";
static const uint8_t position_label[] = "POSITION";
static const uint8_t lap_label[] = "LAP";
static const uint8_t fuel_label[] = "FUEL";
static const uint8_t drs_label[] = "DRS";
static const uint8_t abs_label[] = "ABS";
static const uint8_t traction_control_label[] = "TC";
static const uint8_t ers_label[] = "ERS";
static const uint8_t delta_label[] = "DELTA";

static const RemoteTelemetrySourceDefinition source_definitions[] = {
    {speed_label, 0x1000, 1, 0, 4, sizeof(speed_label) - 1, {0x00, 0x30, 0x00}, 0},
    {rpm_label, 0x1000, 1, 8, 6, sizeof(rpm_label) - 1, {0x00, 0x30, 0x00}, 127},
    {gear_label, 0x1000, 2, 14, 3, sizeof(gear_label) - 1, {0x00, 0x30, 0x00}, 0},
    {position_label, 0x1000, 1, 17, 3, sizeof(position_label) - 1, {0x00, 0x30, 0x00}, 0},
    {lap_label, 0x1000, 1, 24, 3, sizeof(lap_label) - 1, {0x00, 0x30, 0x00}, 0},
    {fuel_label, 0x1000, 1, 31, 5, sizeof(fuel_label) - 1, {0x00, 0x30, 0x00}, 100},
    {drs_label, 0x1000, 1, 40, 4, sizeof(drs_label) - 1, {0x00, 0x30, 0x00}, 1},
    {abs_label, 0x1014, 3, 44, 2, sizeof(abs_label) - 1, {0x14, 0x30, 0x00}, 0},
    {traction_control_label,
     0x1050,
     3,
     46,
     2,
     sizeof(traction_control_label) - 1,
     {0x50, 0x30, 0x00},
     0},
    {ers_label, 0x1000, 1, 48, 3, sizeof(ers_label) - 1, {0x00, 0x30, 0x00}, 100},
    {delta_label, 0x1000, 1, 51, 4, sizeof(delta_label) - 1, {0x00, 0x30, 0x00}, 0},
};

static const RemoteTelemetryMetricDefinition metric_definitions[] = {
    [REMOTE_TELEMETRY_SPEED] = {{{1, 0x34, REMOTE_TELEMETRY_BEHAVIOR_PRIMARY, 0, 3, true}}, 1},
    [REMOTE_TELEMETRY_RPM] =
        {{{2, 0x06, REMOTE_TELEMETRY_BEHAVIOR_PRIMARY | REMOTE_TELEMETRY_BEHAVIOR_RANGE_SOURCE, 1,
           0, false},
          {3, 0x06, REMOTE_TELEMETRY_BEHAVIOR_RANGE_LIMIT, 1, 0, false}},
         2},
    [REMOTE_TELEMETRY_GEAR] = {{{4, 0xa2, REMOTE_TELEMETRY_BEHAVIOR_GEAR, 2, 0, false}}, 1},
    [REMOTE_TELEMETRY_POSITION] = {{{501, 0x24, REMOTE_TELEMETRY_BEHAVIOR_PRIMARY, 3, 4, true}}, 1},
    [REMOTE_TELEMETRY_LAP] = {{{505, 0x24, REMOTE_TELEMETRY_BEHAVIOR_PRIMARY, 4, 4, true}}, 1},
    [REMOTE_TELEMETRY_FUEL] =
        {{{5, 0x18, REMOTE_TELEMETRY_BEHAVIOR_PRIMARY | REMOTE_TELEMETRY_BEHAVIOR_RANGE_SOURCE, 5,
           2, true},
          {6, 0x18, REMOTE_TELEMETRY_BEHAVIOR_RANGE_LIMIT, 5, 0, true}},
         2},
    [REMOTE_TELEMETRY_DRS] = {{{14, 0x41, REMOTE_TELEMETRY_BEHAVIOR_PRIMARY, 6, 0, false},
                               {15, 0x12, REMOTE_TELEMETRY_BEHAVIOR_SCALE_BYTE, 6, 0, false}},
                              2},
    [REMOTE_TELEMETRY_DRIVER_AIDS] = {{{18, 0x22, REMOTE_TELEMETRY_BEHAVIOR_PRIMARY, 7, 0, false},
                                       {20, 0x22, REMOTE_TELEMETRY_BEHAVIOR_PRIMARY, 8, 0, false}},
                                      2},
    [REMOTE_TELEMETRY_ERS] =
        {{{9, 0x09, REMOTE_TELEMETRY_BEHAVIOR_PRIMARY | REMOTE_TELEMETRY_BEHAVIOR_SCALE_BYTE, 9, 0,
           false}},
         1},
    [REMOTE_TELEMETRY_DELTA] = {{{516, 0x1a, REMOTE_TELEMETRY_BEHAVIOR_PRIMARY, 10, 0, false}}, 1},
};

/**
 * @brief Initializes one dynamic telemetry report source.
 *
 * Restores the report identity, template text, secondary label, metadata, and scale defaults for
 * the selected source definition.
 *
 * @param[out] source Dynamic report source to initialize.
 * @param[in] definition Immutable source definition.
 */
static void initialize_source(RemoteTelemetrySource *source,
                              const RemoteTelemetrySourceDefinition *definition) {
    memset(source, 0, sizeof(*source));
    source->report_id = definition->report_id;
    source->payload_length = definition->payload_length;
    memcpy(source->payload, display_template + definition->payload_offset,
           definition->payload_length);
    source->value = definition->value;
    source->secondary_length = definition->secondary_length;
    memcpy(source->secondary_payload, definition->secondary_payload, definition->secondary_length);
    memcpy(source->metadata, definition->metadata, sizeof(source->metadata));
    source->scale_limit = definition->scale_limit;
}

/**
 * @brief Formats a signed integer with an optional minimum width.
 *
 * Writes an unpadded decimal value when width is zero. A nonzero width adds leading zeroes after
 * any minus sign until the requested width is reached.
 *
 * @param[out] output Decimal text destination.
 * @param[in] value Signed value to format.
 * @param[in] width Minimum output width.
 * @return Number of bytes written.
 */
static uint8_t format_integer(uint8_t *output, int32_t value, uint8_t width) {
    uint8_t reversed[11];
    uint8_t digits = 0;
    bool negative = value < 0;
    uint32_t magnitude = negative ? (uint32_t)(-(value + 1)) + 1u : (uint32_t)value;

    do {
        reversed[digits++] = (uint8_t)('0' + magnitude % 10u);
        magnitude /= 10u;
    } while (magnitude != 0);

    uint8_t length = digits + (negative ? 1u : 0u);
    while (length < width) {
        reversed[digits++] = '0';
        length++;
    }
    uint8_t offset = 0;
    if (negative) {
        output[offset++] = '-';
    }
    while (digits != 0) {
        output[offset++] = reversed[--digits];
    }
    return offset;
}

/**
 * @brief Formats a finite float with fixed decimal precision.
 *
 * Uses the absolute value, rounds the fractional digits to the requested precision, and omits a
 * sign. Callers that need a sign add it explicitly.
 *
 * @param[out] output Decimal text destination.
 * @param[in] value Value to format.
 * @param[in] precision Number of fractional digits.
 * @return Number of bytes written.
 */
static uint8_t format_float(uint8_t *output, float value, uint8_t precision) {
    float magnitude = value < 0.0f ? -value : value;
    uint32_t integer = (uint32_t)magnitude;
    uint32_t factor = 1;
    for (uint8_t index = 0; index < precision; index++) {
        factor *= 10;
    }
    uint32_t fraction = (uint32_t)((magnitude - (float)integer) * (float)factor + 0.5f);
    if (fraction >= factor && precision != 0) {
        integer++;
        fraction = 0;
    }

    uint8_t length = format_integer(output, (int32_t)integer, 0);
    if (precision != 0) {
        output[length++] = '.';
        length += format_integer(output + length, (int32_t)fraction, precision);
    }
    return length;
}

/**
 * @brief Reads a little-endian 16-bit value.
 *
 * Combines two consecutive payload bytes without requiring aligned access.
 *
 * @param[in] payload Source bytes.
 * @return Decoded unsigned value.
 */
static uint16_t read_u16(const uint8_t *payload) {
    return (uint16_t)payload[0] | (uint16_t)payload[1] << 8;
}

/**
 * @brief Reads a little-endian 32-bit value.
 *
 * Combines four consecutive payload bytes without requiring aligned access.
 *
 * @param[in] payload Source bytes.
 * @return Decoded unsigned value.
 */
static uint32_t read_u32(const uint8_t *payload) {
    return (uint32_t)payload[0] | (uint32_t)payload[1] << 8 | (uint32_t)payload[2] << 16 |
           (uint32_t)payload[3] << 24;
}

/**
 * @brief Reads a little-endian single-precision value.
 *
 * Preserves the incoming IEEE-754 bit pattern while avoiding unaligned access.
 *
 * @param[in] payload Source bytes.
 * @return Decoded float value.
 */
static float read_float(const uint8_t *payload) {
    union {
        uint32_t bits;
        float value;
    } decoded = {.bits = read_u32(payload)};
    return decoded.value;
}

/**
 * @brief Returns the payload size required by one telemetry format.
 *
 * Text accepts any payload length. Numeric formats require their native one-, two-, or four-byte
 * representation.
 *
 * @param[in] format Telemetry format identifier.
 * @return Required byte count, or zero for text and unsupported formats.
 */
static uint8_t required_payload_size(uint8_t format) {
    switch (format) {
    case REMOTE_TELEMETRY_FORMAT_UINT8:
    case REMOTE_TELEMETRY_FORMAT_INT8:
        return 1;
    case REMOTE_TELEMETRY_FORMAT_UINT16:
    case REMOTE_TELEMETRY_FORMAT_INT16:
    case REMOTE_TELEMETRY_FORMAT_UINT16_ALTERNATE:
        return 2;
    case REMOTE_TELEMETRY_FORMAT_UINT32:
    case REMOTE_TELEMETRY_FORMAT_INT32:
    case REMOTE_TELEMETRY_FORMAT_FLOAT:
    case REMOTE_TELEMETRY_FORMAT_SIGNED_FLOAT:
        return 4;
    default:
        return 0;
    }
}

/**
 * @brief Applies primary text or numeric content to a report source.
 *
 * Formats the channel payload according to its format and width or precision nibble. Signed delta
 * values receive an explicit leading sign and two fractional digits.
 *
 * @param[in,out] source Dynamic report source.
 * @param[in] channel Selected telemetry channel.
 * @param[in] payload Host-provided value bytes.
 * @param[in] payload_length Number of available value bytes.
 * @return True when report content was updated.
 */
static bool apply_primary_content(RemoteTelemetrySource *source,
                                  const RemoteTelemetryChannel *channel, const uint8_t *payload,
                                  uint8_t payload_length) {
    uint8_t format = channel->format & 0x0f;
    uint8_t scale = channel->format >> 4;
    uint8_t required = required_payload_size(format);
    if (format != REMOTE_TELEMETRY_FORMAT_TEXT && (required == 0 || payload_length < required)) {
        return false;
    }

    switch (format) {
    case REMOTE_TELEMETRY_FORMAT_TEXT:
        source->payload_length = scale;
        memset(source->payload, ' ', scale);
        if (payload_length > scale) {
            payload_length = scale;
        }
        memcpy(source->payload, payload, payload_length);
        return true;
    case REMOTE_TELEMETRY_FORMAT_UINT8:
        source->payload_length = format_integer(source->payload, payload[0], scale);
        return true;
    case REMOTE_TELEMETRY_FORMAT_INT8:
        source->payload_length = format_integer(source->payload, (int8_t)payload[0], scale);
        return true;
    case REMOTE_TELEMETRY_FORMAT_UINT16:
    case REMOTE_TELEMETRY_FORMAT_UINT16_ALTERNATE:
        source->payload_length = format_integer(source->payload, read_u16(payload), scale);
        return true;
    case REMOTE_TELEMETRY_FORMAT_INT16:
        source->payload_length = format_integer(source->payload, (int16_t)read_u16(payload), scale);
        return true;
    case REMOTE_TELEMETRY_FORMAT_UINT32:
    case REMOTE_TELEMETRY_FORMAT_INT32:
        source->payload_length = format_integer(source->payload, (int32_t)read_u32(payload), scale);
        return true;
    case REMOTE_TELEMETRY_FORMAT_FLOAT:
        source->payload_length = format_float(source->payload, read_float(payload), scale);
        return true;
    case REMOTE_TELEMETRY_FORMAT_SIGNED_FLOAT: {
        float value = read_float(payload);
        source->payload[0] = value > 0.0f ? '+' : '-';
        source->payload_length = 1 + format_float(source->payload + 1, value, 2);
        return scale == 1;
    }
    default:
        return false;
    }
}

/**
 * @brief Recomputes a shared RPM or fuel scale.
 *
 * Uses the primary channel value and secondary range limit to update the selected source's scale
 * byte. RPM uses a 127-step scale and fuel uses a percentage scale.
 *
 * @param[in,out] telemetry Selected telemetry state.
 * @param[in] channel_index Channel whose cached value changed.
 */
static void update_range_scale(RemoteTelemetry *telemetry, uint8_t channel_index) {
    RemoteTelemetryChannel *primary = &telemetry->channels[0];
    RemoteTelemetryChannel *limit = &telemetry->channels[1];
    RemoteTelemetrySource *source = &telemetry->sources[primary->source_slot];
    uint8_t format = telemetry->channels[channel_index].format & 0x0f;

    if (format == REMOTE_TELEMETRY_FORMAT_UINT32) {
        source->scale_limit = 127;
        uint32_t step = limit->cached_integer / 127u;
        if (primary->cached_integer != 0 && step != 0) {
            source->scale_value = (uint8_t)(primary->cached_integer / step);
        }
    } else if (format == REMOTE_TELEMETRY_FORMAT_FLOAT) {
        source->scale_limit = 100;
        if (primary->cached_float != 0.0f && limit->cached_float != 0.0f) {
            source->scale_value = (uint8_t)((primary->cached_float / limit->cached_float) * 100.0f);
        }
    }
}

/**
 * @brief Encodes one dynamic telemetry source as a wheel report.
 *
 * Writes the fixed 30-byte field layout and appends the primary channel overlay after the source
 * payload when it fits in the report payload area.
 *
 * @param[in] telemetry Selected telemetry state.
 * @param[in] channel_index Channel whose source supplies the report.
 * @param[out] output Encoded report.
 * @return True when the payload and overlay fit the report.
 */
static bool encode_report(const RemoteTelemetry *telemetry, uint8_t channel_index,
                          uint8_t output[REMOTE_TELEMETRY_REPORT_SIZE]) {
    const RemoteTelemetryChannel *channel = &telemetry->channels[channel_index];
    const RemoteTelemetryChannel *overlay = &telemetry->channels[0];
    const RemoteTelemetrySource *source = &telemetry->sources[channel->source_slot];
    uint8_t payload_length = source->payload_length + overlay->overlay_length;
    if (payload_length > sizeof(source->payload)) {
        return false;
    }

    memset(output, 0, REMOTE_TELEMETRY_REPORT_SIZE);
    output[0] = source->report_id;
    output[1] = payload_length;
    memcpy(output + 2, source->payload, source->payload_length);
    memcpy(output + 2 + source->payload_length, overlay->overlay, overlay->overlay_length);
    output[14] = (uint8_t)source->value;
    output[15] = (uint8_t)(source->value >> 8);
    output[16] = source->secondary_length;
    memcpy(output + 17, source->secondary_payload, source->secondary_length);
    memcpy(output + 25, source->metadata, sizeof(source->metadata));
    output[28] = source->scale_limit;
    output[29] = source->scale_value;
    return true;
}

/**
 * @brief Queues one host telemetry control record.
 *
 * Appends route, selector, little-endian key, and format to the 32-entry arrival-order queue.
 *
 * @param[in,out] telemetry Telemetry state that owns the control queue.
 * @param[in] selector Channel and selector-bank bits.
 * @param[in] key Subscription key or 0xFFFF clear value.
 * @param[in] format Subscription format.
 * @return True when a queue slot was available.
 */
static bool queue_control_record(RemoteTelemetry *telemetry, uint8_t selector, uint16_t key,
                                 uint8_t format) {
    uint8_t record[REMOTE_TELEMETRY_SUBSCRIPTION_SIZE] = {
        REMOTE_TELEMETRY_ROUTE, selector, (uint8_t)key, (uint8_t)(key >> 8), format,
    };
    return remote_telemetry_queue_control_record(telemetry, record);
}

/**
 * @brief Initializes remote telemetry state.
 *
 * Clears the selected metric, channel mappings, dynamic reports, control queue, and report
 * scheduler.
 *
 * @param[out] telemetry Telemetry state to initialize.
 */
void remote_telemetry_init(RemoteTelemetry *telemetry) { memset(telemetry, 0, sizeof(*telemetry)); }

/**
 * @brief Selects one remote telemetry metric.
 *
 * Queues clears for the previous mapping, replaces the active channels with the metric's one- or
 * two-channel definition, restores every referenced report source, and queues the new
 * subscriptions. Selection zero clears the service without adding subscriptions. The change is
 * atomic when the control queue lacks space.
 *
 * @param[in,out] telemetry Telemetry state to configure.
 * @param[in] metric Requested metric.
 * @return True when the metric is supported and the complete change was accepted.
 */
bool remote_telemetry_select(RemoteTelemetry *telemetry, RemoteTelemetryMetric metric) {
    if (telemetry == NULL || metric > REMOTE_TELEMETRY_DELTA) {
        return false;
    }
    if (telemetry->metric == metric) {
        return true;
    }

    uint8_t old_count = remote_telemetry_subscription_count(telemetry);
    uint8_t new_count =
        metric == REMOTE_TELEMETRY_NONE ? 0 : metric_definitions[metric].channel_count;
    if ((uint8_t)(old_count + new_count) >
        REMOTE_TELEMETRY_CONTROL_QUEUE_COUNT - telemetry->control_count) {
        return false;
    }
    for (uint8_t channel_index = 0; channel_index < old_count; channel_index++) {
        const RemoteTelemetryChannel *channel = &telemetry->channels[channel_index];
        uint8_t selector =
            channel_index | (channel->overlay_enabled ? REMOTE_TELEMETRY_OVERLAY_SELECTOR : 0);
        (void)queue_control_record(telemetry, selector, UINT16_MAX, channel->format);
    }

    memset(telemetry->sources, 0, sizeof(telemetry->sources));
    memset(telemetry->channels, 0, sizeof(telemetry->channels));
    telemetry->metric = metric;
    telemetry->source_count = 0;
    telemetry->report_channel = 0;
    if (metric == REMOTE_TELEMETRY_NONE) {
        return true;
    }

    const RemoteTelemetryMetricDefinition *definition = &metric_definitions[metric];
    uint8_t source_indices[REMOTE_TELEMETRY_CHANNEL_COUNT] = {0};
    for (uint8_t channel_index = 0; channel_index < definition->channel_count; channel_index++) {
        const RemoteTelemetryChannelDefinition *channel_definition =
            &definition->channels[channel_index];
        RemoteTelemetryChannel *channel = &telemetry->channels[channel_index];
        channel->key = channel_definition->key;
        channel->format = channel_definition->format;
        channel->behavior = channel_definition->behavior;
        channel->overlay_capacity = channel_definition->overlay_capacity;
        channel->overlay_enabled = channel_definition->overlay_enabled;

        uint8_t source_slot = 0;
        while (source_slot < telemetry->source_count &&
               source_indices[source_slot] != channel_definition->source) {
            source_slot++;
        }
        if (source_slot == telemetry->source_count) {
            source_indices[source_slot] = channel_definition->source;
            initialize_source(&telemetry->sources[source_slot],
                              &source_definitions[channel_definition->source]);
            telemetry->source_count++;
        }
        channel->source_slot = source_slot;
        uint8_t selector =
            channel_index | (channel->overlay_enabled ? REMOTE_TELEMETRY_OVERLAY_SELECTOR : 0);
        (void)queue_control_record(telemetry, selector, channel->key, channel->format);
    }
    return true;
}

/**
 * @brief Returns the active telemetry subscription count.
 *
 * Reports zero for no selection and otherwise returns the selected metric's channel count.
 *
 * @param[in] telemetry Selected telemetry state.
 * @return Number of active subscriptions.
 */
uint8_t remote_telemetry_subscription_count(const RemoteTelemetry *telemetry) {
    if (telemetry == NULL || telemetry->metric == REMOTE_TELEMETRY_NONE) {
        return 0;
    }
    return metric_definitions[telemetry->metric].channel_count;
}

/**
 * @brief Reads one active telemetry subscription.
 *
 * Exposes the expected key and format with the channel index in the selector low nibble and the
 * overlay capability in the selector high bit.
 *
 * @param[in] telemetry Selected telemetry state.
 * @param[in] channel Active channel index.
 * @param[out] subscription Logical subscription fields.
 * @return True when the requested channel is active.
 */
bool remote_telemetry_subscription(const RemoteTelemetry *telemetry, uint8_t channel,
                                   RemoteTelemetrySubscription *subscription) {
    if (telemetry == NULL || subscription == NULL ||
        channel >= remote_telemetry_subscription_count(telemetry)) {
        return false;
    }
    const RemoteTelemetryChannel *mapping = &telemetry->channels[channel];
    subscription->key = mapping->key;
    subscription->selector =
        channel | (mapping->overlay_enabled ? REMOTE_TELEMETRY_OVERLAY_SELECTOR : 0);
    subscription->format = mapping->format;
    return true;
}

/**
 * @brief Encodes one telemetry subscription record.
 *
 * Writes route two, selector, little-endian key, and format as the five-byte host record.
 *
 * @param[in] subscription Logical subscription fields.
 * @param[out] output Encoded five-byte record.
 */
void remote_telemetry_encode_subscription(const RemoteTelemetrySubscription *subscription,
                                          uint8_t output[REMOTE_TELEMETRY_SUBSCRIPTION_SIZE]) {
    output[0] = REMOTE_TELEMETRY_ROUTE;
    output[1] = subscription->selector;
    output[2] = (uint8_t)subscription->key;
    output[3] = (uint8_t)(subscription->key >> 8);
    output[4] = subscription->format;
}

/**
 * @brief Queues one encoded telemetry control record for the host.
 *
 * Appends the five bytes unchanged to the 32-entry arrival-order queue. A full queue rejects the
 * record without changing any retained entry.
 *
 * @param[in,out] telemetry Telemetry state that owns the control queue.
 * @param[in] input Complete five-byte control record.
 * @return True when the record was queued.
 */
bool remote_telemetry_queue_control_record(
    RemoteTelemetry *telemetry, const uint8_t input[REMOTE_TELEMETRY_SUBSCRIPTION_SIZE]) {
    if (telemetry == NULL || input == NULL ||
        telemetry->control_count >= REMOTE_TELEMETRY_CONTROL_QUEUE_COUNT) {
        return false;
    }
    uint8_t tail = (uint8_t)((telemetry->control_head + telemetry->control_count) %
                             REMOTE_TELEMETRY_CONTROL_QUEUE_COUNT);
    memcpy(telemetry->control_records[tail], input, REMOTE_TELEMETRY_SUBSCRIPTION_SIZE);
    telemetry->control_count++;
    return true;
}

/**
 * @brief Takes the oldest queued telemetry control record.
 *
 * Copies and consumes one five-byte subscription or clear record while preserving the order of
 * all remaining records.
 *
 * @param[in,out] telemetry Telemetry state that owns the control queue.
 * @param[out] output Oldest encoded control record.
 * @return True when a record was available.
 */
bool remote_telemetry_take_control_record(RemoteTelemetry *telemetry,
                                          uint8_t output[REMOTE_TELEMETRY_SUBSCRIPTION_SIZE]) {
    if (telemetry == NULL || output == NULL || telemetry->control_count == 0) {
        return false;
    }
    memcpy(output, telemetry->control_records[telemetry->control_head],
           REMOTE_TELEMETRY_SUBSCRIPTION_SIZE);
    memset(telemetry->control_records[telemetry->control_head], 0,
           REMOTE_TELEMETRY_SUBSCRIPTION_SIZE);
    telemetry->control_head =
        (uint8_t)((telemetry->control_head + 1) % REMOTE_TELEMETRY_CONTROL_QUEUE_COUNT);
    telemetry->control_count--;
    return true;
}

/**
 * @brief Applies a primary telemetry value record.
 *
 * Validates the channel key, formats primary content, updates scale or range state, and marks the
 * affected channel for report emission. A valid but unexpected key requests that the host clear
 * the stale subscription.
 *
 * @param[in,out] telemetry Selected telemetry state.
 * @param[in] channel Target channel index.
 * @param[in] key Host record key.
 * @param[in] payload Host record payload.
 * @param[in] payload_length Number of payload bytes.
 * @return Record disposition.
 */
RemoteTelemetryRecordResult remote_telemetry_apply_primary(RemoteTelemetry *telemetry,
                                                           uint8_t channel, uint16_t key,
                                                           const uint8_t *payload,
                                                           uint8_t payload_length) {
    if (telemetry == NULL || payload == NULL || channel >= REMOTE_TELEMETRY_CHANNEL_COUNT ||
        key == 0 || key > REMOTE_TELEMETRY_MAXIMUM_KEY) {
        return REMOTE_TELEMETRY_RECORD_IGNORED;
    }
    RemoteTelemetryChannel *mapping = &telemetry->channels[channel];
    if (mapping->key != key) {
        (void)queue_control_record(telemetry, channel, UINT16_MAX, 0);
        return REMOTE_TELEMETRY_CLEAR_REQUESTED;
    }

    RemoteTelemetrySource *source = &telemetry->sources[mapping->source_slot];
    bool applied = false;
    if ((mapping->behavior & REMOTE_TELEMETRY_BEHAVIOR_PRIMARY) != 0) {
        applied = apply_primary_content(source, mapping, payload, payload_length);
    }
    uint8_t format = mapping->format & 0x0f;
    if ((mapping->behavior & REMOTE_TELEMETRY_BEHAVIOR_SCALE_BYTE) != 0 && payload_length >= 1 &&
        (format == REMOTE_TELEMETRY_FORMAT_UINT8 ||
         format == REMOTE_TELEMETRY_FORMAT_UINT16_ALTERNATE)) {
        source->scale_value = payload[0];
        applied = true;
    }
    if ((mapping->behavior & REMOTE_TELEMETRY_BEHAVIOR_RANGE_SOURCE) != 0 &&
        payload_length >= required_payload_size(format)) {
        if (format == REMOTE_TELEMETRY_FORMAT_UINT32) {
            mapping->cached_integer = read_u32(payload);
        } else if (format == REMOTE_TELEMETRY_FORMAT_FLOAT) {
            mapping->cached_float = read_float(payload);
        }
        update_range_scale(telemetry, channel);
        applied = true;
    }
    if ((mapping->behavior & REMOTE_TELEMETRY_BEHAVIOR_RANGE_LIMIT) != 0 &&
        payload_length >= required_payload_size(format)) {
        if (format == REMOTE_TELEMETRY_FORMAT_UINT32) {
            mapping->cached_integer = read_u32(payload);
        } else if (format == REMOTE_TELEMETRY_FORMAT_FLOAT) {
            mapping->cached_float = read_float(payload);
        }
        update_range_scale(telemetry, channel);
        applied = true;
    }
    if ((mapping->behavior & REMOTE_TELEMETRY_BEHAVIOR_GEAR) != 0 && payload_length >= 1 &&
        format == REMOTE_TELEMETRY_FORMAT_UINT8) {
        uint8_t length = 1;
        source->payload[0] = ' ';
        if (payload[0] >= 1 && payload[0] <= 9) {
            source->payload[length++] = (uint8_t)('0' + payload[0]);
        } else if (payload[0] == 0) {
            source->payload[length++] = 'n';
        } else if (payload[0] == UINT8_MAX) {
            source->payload[length++] = 'r';
        }
        source->payload[length] = ' ';
        applied = true;
    }

    if (applied) {
        mapping->dirty = true;
    }
    return applied ? REMOTE_TELEMETRY_RECORD_APPLIED : REMOTE_TELEMETRY_RECORD_IGNORED;
}

/**
 * @brief Applies an overlay telemetry record.
 *
 * Copies at most the configured overlay capacity and marks the channel for report emission. A
 * valid unexpected key requests a clear only for channels that do not accept overlays.
 *
 * @param[in,out] telemetry Selected telemetry state.
 * @param[in] channel Target channel index.
 * @param[in] key Host record key.
 * @param[in] payload Host record payload.
 * @param[in] payload_length Number of payload bytes.
 * @return Record disposition.
 */
RemoteTelemetryRecordResult remote_telemetry_apply_overlay(RemoteTelemetry *telemetry,
                                                           uint8_t channel, uint16_t key,
                                                           const uint8_t *payload,
                                                           uint8_t payload_length) {
    if (telemetry == NULL || payload == NULL || channel >= REMOTE_TELEMETRY_CHANNEL_COUNT ||
        key == 0 || key > REMOTE_TELEMETRY_MAXIMUM_KEY) {
        return REMOTE_TELEMETRY_RECORD_IGNORED;
    }
    RemoteTelemetryChannel *mapping = &telemetry->channels[channel];
    if (mapping->key != key) {
        if (mapping->overlay_enabled) {
            return REMOTE_TELEMETRY_RECORD_IGNORED;
        }
        (void)queue_control_record(telemetry, channel | REMOTE_TELEMETRY_OVERLAY_SELECTOR,
                                   UINT16_MAX, 0);
        return REMOTE_TELEMETRY_CLEAR_REQUESTED;
    }
    if (!mapping->overlay_enabled) {
        return REMOTE_TELEMETRY_RECORD_IGNORED;
    }

    if (payload_length > mapping->overlay_capacity) {
        payload_length = mapping->overlay_capacity;
    }
    memcpy(mapping->overlay, payload, payload_length);
    mapping->overlay_length = payload_length;
    mapping->dirty = true;
    return REMOTE_TELEMETRY_RECORD_APPLIED;
}

/**
 * @brief Takes the next dirty telemetry wheel report.
 *
 * Services channel zero and then channel one on alternating calls when both are configured. Either
 * channel being dirty can cause the current channel's report to be emitted; only the emitted
 * channel's dirty state is consumed.
 *
 * @param[in,out] telemetry Selected telemetry state and report scheduler.
 * @param[out] output Encoded 30-byte wheel report.
 * @return True when a report was produced.
 */
bool remote_telemetry_take_report(RemoteTelemetry *telemetry,
                                  uint8_t output[REMOTE_TELEMETRY_REPORT_SIZE]) {
    if (telemetry == NULL || output == NULL) {
        return false;
    }
    uint8_t count = remote_telemetry_subscription_count(telemetry);
    if (count == 0) {
        return false;
    }

    uint8_t channel = telemetry->report_channel;
    bool dirty = telemetry->channels[0].dirty || (count == 2 && telemetry->channels[1].dirty);
    telemetry->report_channel = count == 2 && channel == 0 ? 1 : 0;
    if (!dirty || !encode_report(telemetry, channel, output)) {
        return false;
    }
    telemetry->channels[channel].dirty = false;
    return true;
}
