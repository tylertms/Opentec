#include "wheel/packet_packed.h"

#include <stdbool.h>
#include <stdint.h>

/** @brief Internal packed-packet identifiers and payload offsets. */
enum {
    WHEEL_PACKET_PACKED_MODE_ALTERNATE = 0x0f,     /**< Legacy alternate packet mode. */
    WHEEL_PACKET_PACKED_MODE_COMPATIBILITY = 0x17, /**< Legacy compatibility packet mode. */
    WHEEL_PACKET_COMMAND_AUTHENTICATE = 0xa6,      /**< Authenticated response command. */
    REQUEST_PAYLOAD_OFFSET = 2,                    /**< Request payload offset. */
    REQUEST_AXIS_OUTPUTS_OFFSET = 3,               /**< Axis-output offset within the payload. */
    REQUEST_MOTION_OFFSET = 5,                     /**< Motion offset within the payload. */
    REQUEST_CONTROLS_OFFSET = 6,                   /**< Control offset within the payload. */
    REQUEST_PACKED_AXES_OFFSET = 12,               /**< Packed-axis offset within the payload. */
    REQUEST_RESERVED_AXES_OFFSET = 14,             /**< Reserved-axis offset within the payload. */
    REQUEST_AXIS_VALUES_OFFSET = 16,               /**< Axis-value offset within the payload. */
    REQUEST_MODE_BUTTONS_OFFSET = 20,              /**< Mode-button offset within the payload. */
    REQUEST_AXIS_REPORT_ENABLED_OFFSET = 21,       /**< Axis-report flag offset. */
    REQUEST_AUXILIARY_OFFSET = 22,                 /**< Auxiliary-data offset. */
    REQUEST_REPORT_MODE_OFFSET = 26,               /**< Report-mode offset. */
    REQUEST_RESERVED_REPORT_OFFSET = 27,           /**< Reserved-report offset. */
    REQUEST_REPORT_CAPABILITIES_OFFSET = 28,       /**< Report-capability offset. */
    REQUEST_AXIS_LIMIT_OFFSET = 29,                /**< Axis-limit offset. */
};

/**
 * @brief Reads one little-endian 16-bit packet field.
 *
 * Combines two adjacent bytes without requiring aligned storage.
 *
 * @param[in] data First byte of the field.
 * @return Decoded 16-bit value.
 */
static uint16_t read_little_endian_u16(const uint8_t *data) {
    return (uint16_t)data[0] | (uint16_t)data[1] << 8;
}

/**
 * @brief Reports whether a wheel mode uses the packed packet codec.
 *
 * Selects the legacy-alternate and legacy-compatibility modes that share this layout.
 *
 * @param[in] wheel_mode Selected attached-wheel mode.
 * @return True for modes 0x0F and 0x17.
 */
bool wheel_packet_packed_applies(uint8_t wheel_mode) {
    return wheel_mode == WHEEL_PACKET_PACKED_MODE_ALTERNATE ||
           wheel_mode == WHEEL_PACKET_PACKED_MODE_COMPATIBILITY;
}

/**
 * @brief Clears the packed-family button history.
 *
 * Zeros all three samples and resets the insertion position.
 *
 * @param[out] filter Three-sample button filter to initialize.
 */
void wheel_packet_packed_filter_init(WheelPacketPackedFilter *filter) {
    for (uint8_t sample = 0; sample < WHEEL_PACKET_PACKED_HISTORY_DEPTH; sample++) {
        for (uint8_t button = 0; button < WHEEL_PACKET_PACKED_BUTTON_COUNT; button++) {
            filter->samples[sample][button] = 0;
        }
    }
    filter->next_sample = 0;
}

/**
 * @brief Filters one packed-family button sample.
 *
 * Keeps only button bits present in all three recent samples and advances the insertion position.
 *
 * @param[in,out] filter Three-sample history and insertion position.
 * @param[in,out] input Input whose button bytes are added to the history and filtered in place.
 */
void wheel_packet_packed_filter_buttons(WheelPacketPackedFilter *filter,
                                        WheelPacketPackedInput *input) {
    for (uint8_t button = 0; button < WHEEL_PACKET_PACKED_BUTTON_COUNT; button++) {
        filter->samples[filter->next_sample][button] = input->buttons[button];
        input->buttons[button] =
            filter->samples[0][button] & filter->samples[1][button] & filter->samples[2][button];
    }
    filter->next_sample++;
    if (filter->next_sample == WHEEL_PACKET_PACKED_HISTORY_DEPTH) {
        filter->next_sample = 0;
    }
}

/**
 * @brief Decodes a packed attached-wheel request.
 *
 * Reads the 30-byte payload into logical button, axis, motion, control, report, and capability
 * fields without relying on packed structures.
 *
 * @param[in] request First 32 request bytes, including the command and reserved prefix.
 * @param[out] input Logical packed-family input populated from the request payload.
 */
void wheel_packet_packed_decode(const uint8_t request[WHEEL_PACKET_PACKED_REQUEST_SIZE],
                                WheelPacketPackedInput *input) {
    const uint8_t *payload = &request[REQUEST_PAYLOAD_OFFSET];
    for (uint8_t index = 0; index < WHEEL_PACKET_PACKED_BUTTON_COUNT; index++) {
        input->buttons[index] = payload[index];
    }
    input->axis_outputs[0] = payload[REQUEST_AXIS_OUTPUTS_OFFSET];
    input->axis_outputs[1] = payload[REQUEST_AXIS_OUTPUTS_OFFSET + 1];
    input->motion = (int8_t)payload[REQUEST_MOTION_OFFSET];
    for (uint8_t index = 0; index < WHEEL_PACKET_PACKED_CONTROL_COUNT; index++) {
        input->controls[index] = payload[REQUEST_CONTROLS_OFFSET + index];
    }
    input->reserved_axes[0] = payload[REQUEST_RESERVED_AXES_OFFSET];
    input->reserved_axes[1] = payload[REQUEST_RESERVED_AXES_OFFSET + 1];
    for (uint8_t index = 0; index < WHEEL_PACKET_PACKED_AXIS_VALUE_COUNT; index++) {
        input->axis_values[index] =
            read_little_endian_u16(&payload[REQUEST_AXIS_VALUES_OFFSET + index * 2]);
    }
    input->mode_buttons = payload[REQUEST_MODE_BUTTONS_OFFSET];
    input->axis_report_enabled = payload[REQUEST_AXIS_REPORT_ENABLED_OFFSET];
    for (uint8_t index = 0; index < 4; index++) {
        input->auxiliary_data[index] = payload[REQUEST_AUXILIARY_OFFSET + index];
    }
    input->report_mode = payload[REQUEST_REPORT_MODE_OFFSET];
    input->reserved_report = payload[REQUEST_RESERVED_REPORT_OFFSET];
    input->report_capabilities = payload[REQUEST_REPORT_CAPABILITIES_OFFSET];
    input->axis_limit = payload[REQUEST_AXIS_LIMIT_OFFSET];
}

/**
 * @brief Normalizes packed-family buttons and auxiliary axes.
 *
 * Exchanges primary button bits 8 and 11, then expands the two high nibbles of the packed axis
 * word into the first two logical control bytes.
 *
 * @param[in,out] input Decoded packed-family input updated in place.
 */
void wheel_packet_packed_normalize(WheelPacketPackedInput *input) {
    uint16_t buttons = (uint16_t)input->buttons[0] | (uint16_t)input->buttons[1] << 8;
    uint16_t bit_eight = buttons & 0x0100u;
    uint16_t bit_eleven = buttons & 0x0800u;
    buttons &= 0xf6ffu;
    buttons |= bit_eight << 3;
    buttons |= bit_eleven >> 3;
    input->buttons[0] = (uint8_t)buttons;
    input->buttons[1] = (uint8_t)(buttons >> 8);

    uint16_t packed_axes = read_little_endian_u16(&input->controls[6]);
    input->controls[0] = (uint8_t)(packed_axes >> 8) & 0x0fu;
    input->controls[1] = (uint8_t)(packed_axes >> 12);
}

/**
 * @brief Encodes normalized packed-family input as a protocol request view.
 *
 * Writes every logical field into its defined position in the 30-byte payload.
 *
 * @param[in] input Normalized packed-family input.
 * @param[out] snapshot Thirty-byte protocol request view.
 */
void wheel_packet_packed_snapshot(const WheelPacketPackedInput *input,
                                  uint8_t snapshot[WHEEL_PACKET_PACKED_SNAPSHOT_SIZE]) {
    for (uint8_t index = 0; index < WHEEL_PACKET_PACKED_BUTTON_COUNT; index++) {
        snapshot[index] = input->buttons[index];
    }
    snapshot[REQUEST_AXIS_OUTPUTS_OFFSET] = input->axis_outputs[0];
    snapshot[REQUEST_AXIS_OUTPUTS_OFFSET + 1] = input->axis_outputs[1];
    snapshot[REQUEST_MOTION_OFFSET] = (uint8_t)input->motion;
    for (uint8_t index = 0; index < WHEEL_PACKET_PACKED_CONTROL_COUNT; index++) {
        snapshot[REQUEST_CONTROLS_OFFSET + index] = input->controls[index];
    }
    snapshot[REQUEST_RESERVED_AXES_OFFSET] = input->reserved_axes[0];
    snapshot[REQUEST_RESERVED_AXES_OFFSET + 1] = input->reserved_axes[1];
    for (uint8_t index = 0; index < WHEEL_PACKET_PACKED_AXIS_VALUE_COUNT; index++) {
        snapshot[REQUEST_AXIS_VALUES_OFFSET + index * 2] = (uint8_t)input->axis_values[index];
        snapshot[REQUEST_AXIS_VALUES_OFFSET + index * 2 + 1] =
            (uint8_t)(input->axis_values[index] >> 8);
    }
    snapshot[REQUEST_MODE_BUTTONS_OFFSET] = input->mode_buttons;
    snapshot[REQUEST_AXIS_REPORT_ENABLED_OFFSET] = input->axis_report_enabled;
    for (uint8_t index = 0; index < 4; index++) {
        snapshot[REQUEST_AUXILIARY_OFFSET + index] = input->auxiliary_data[index];
    }
    snapshot[REQUEST_REPORT_MODE_OFFSET] = input->report_mode;
    snapshot[REQUEST_RESERVED_REPORT_OFFSET] = input->reserved_report;
    snapshot[REQUEST_REPORT_CAPABILITIES_OFFSET] = input->report_capabilities;
    snapshot[REQUEST_AXIS_LIMIT_OFFSET] = input->axis_limit;
}

/**
 * @brief Encodes a packed-family attached-wheel response.
 *
 * Writes command A6, three display glyphs, the optional third-glyph marker, two vibration
 * channels, and two legacy-axis values.
 *
 * @param[in] display Current display output.
 * @param[in] vibration Two vibration-channel values.
 * @param[in] legacy_axes Two legacy-axis values.
 * @param[out] response Nine-byte destination buffer.
 */
void wheel_packet_packed_encode(const WheelDisplayOutput *display, const uint8_t vibration[2],
                                const uint8_t legacy_axes[2],
                                uint8_t response[WHEEL_PACKET_PACKED_RESPONSE_SIZE]) {
    response[0] = WHEEL_PACKET_COMMAND_AUTHENTICATE;
    response[1] = 0;
    for (uint8_t index = 0; index < WHEEL_DISPLAY_GLYPH_COUNT; index++) {
        response[index + 2] = display->glyphs[index];
    }
    if (display->third_glyph_marker) {
        response[4] |= 0x80u;
    }
    response[5] = vibration[0];
    response[6] = vibration[1];
    response[7] = legacy_axes[0];
    response[8] = legacy_axes[1];
}
