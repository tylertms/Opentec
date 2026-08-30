#include "pedal/v4_status.h"

#include <stddef.h>
#include <stdint.h>

enum {
    PEDAL_V4_RECORD_TAG = 0x0a,
    PEDAL_V4_SELECTOR_FIELD = 1,
    PEDAL_V4_VALUE_FIELD = 2,
    PEDAL_V4_WIRE_VARINT = 0,
    PEDAL_V4_WIRE_DELIMITED = 2,
};

typedef struct {
    uint32_t value;
    uint16_t offset;
} ParsedVarint;

static const uint8_t status_request[PEDAL_V4_STATUS_REQUEST_SIZE] = {
    0x12, 0x0a, 0x02, 0x08, 0x02, 0x18, 0x01, 0x20, 0x08, 0xaa, 0x01,
    0x07, 0xba, 0x01, 0x04, 0x52, 0x02, 0x72, 0x00, 0xd7, 0xfb,
};

/**
 * @brief Provides the V4 pedal status query.
 *
 * Returns the fixed group-zero payload used to request the current three-axis pedal state.
 *
 * @return Immutable 21-byte status request.
 */
const uint8_t *pedal_v4_status_request(void) { return status_request; }

/**
 * @brief Decodes one bounded unsigned variable-length integer.
 *
 * Accumulates up to 32 value bits and returns the first offset after the encoded integer.
 *
 * @param[in] data Response payload containing the integer.
 * @param[in] end Exclusive payload boundary.
 * @param[in] offset Initial integer offset.
 * @return Parsed value and next payload offset.
 */
static ParsedVarint parse_varint(const uint8_t *data, uint16_t end, uint16_t offset) {
    ParsedVarint result = {.value = 0, .offset = offset};
    uint8_t shift = 0;

    while (result.offset < end) {
        uint8_t byte = data[result.offset++];
        if (shift < 32) {
            result.value |= (uint32_t)(byte & 0x7f) << shift;
        }
        if ((byte & 0x80) == 0) {
            break;
        }
        shift += 7;
    }
    return result;
}

/**
 * @brief Skips an unrecognized bounded status field.
 *
 * Advances across variable-length integers and length-delimited fields without crossing the record
 * boundary. Unsupported wire types consume the rest of the record.
 *
 * @param[in] data Response payload containing the field.
 * @param[in] end Exclusive record boundary.
 * @param[in] offset Initial field-value offset.
 * @param[in] wire_type Encoded field wire type.
 * @return Offset after the skipped field.
 */
static uint16_t skip_field(const uint8_t *data, uint16_t end, uint16_t offset, uint8_t wire_type) {
    if (wire_type == PEDAL_V4_WIRE_VARINT) {
        return parse_varint(data, end, offset).offset;
    }
    if (wire_type == PEDAL_V4_WIRE_DELIMITED && offset < end) {
        uint16_t field_length = data[offset++];
        uint16_t remaining = end - offset;
        return offset + (field_length < remaining ? field_length : remaining);
    }
    return end;
}

/**
 * @brief Extracts a selector and value from one V4 status record.
 *
 * Stores values for selectors one through three and ignores unsupported selectors and fields.
 *
 * @param[in] data Response payload containing the record.
 * @param[in] start First record byte.
 * @param[in] end Exclusive record boundary.
 * @param[in,out] values Accumulated values indexed by one-based selector.
 */
static void parse_record(const uint8_t *data, uint16_t start, uint16_t end,
                         uint32_t values[PEDAL_V4_STATUS_AXIS_COUNT]) {
    uint16_t offset = start;
    uint8_t selector = 0;
    uint32_t value = 0;

    while (offset < end) {
        uint8_t key = data[offset++];
        uint8_t field = key >> 3;
        uint8_t wire_type = key & 0x07;

        if (field == PEDAL_V4_SELECTOR_FIELD && wire_type == PEDAL_V4_WIRE_VARINT) {
            ParsedVarint parsed = parse_varint(data, end, offset);
            selector = (uint8_t)parsed.value;
            offset = parsed.offset;
        } else if (field == PEDAL_V4_VALUE_FIELD && wire_type == PEDAL_V4_WIRE_VARINT) {
            ParsedVarint parsed = parse_varint(data, end, offset);
            value = parsed.value;
            offset = parsed.offset;
        } else {
            offset = skip_field(data, end, offset, wire_type);
        }
    }

    if (selector >= 1 && selector <= PEDAL_V4_STATUS_AXIS_COUNT) {
        values[selector - 1] = value;
    }
}

/**
 * @brief Extracts the three pedal axes from a V4 status response.
 *
 * Parses selector/value records after the 25-byte envelope and publishes selectors two, one, and
 * three as the primary, secondary, and tertiary pedal axes.
 *
 * @param[in] data Complete V4 response payload.
 * @param[in] length Response payload length.
 * @param[in,out] axes Axis destination ordered as primary, secondary, and tertiary input; retained
 * unchanged when the response has no record payload.
 */
void pedal_v4_status_parse(const uint8_t *data, uint16_t length,
                           uint16_t axes[PEDAL_V4_STATUS_AXIS_COUNT]) {
    if (data == NULL || length <= PEDAL_V4_STATUS_ENVELOPE_SIZE) {
        return;
    }

    uint32_t values[PEDAL_V4_STATUS_AXIS_COUNT] = {0};
    uint16_t offset = PEDAL_V4_STATUS_ENVELOPE_SIZE;
    while (offset < length) {
        if (data[offset++] != PEDAL_V4_RECORD_TAG || offset >= length) {
            break;
        }

        uint16_t record_length = data[offset++];
        uint16_t remaining = length - offset;
        if (record_length > remaining) {
            break;
        }

        uint16_t record_end = offset + record_length;
        parse_record(data, offset, record_end, values);
        offset = record_end;
    }

    axes[0] = (uint16_t)values[1];
    axes[1] = (uint16_t)values[0];
    axes[2] = (uint16_t)values[2];
}
