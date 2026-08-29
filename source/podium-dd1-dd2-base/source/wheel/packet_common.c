#include "wheel/packet_common.h"

#include <stdint.h>

enum {
    WHEEL_PACKET_COMMAND_AUTHENTICATE = 0xa6,
    REQUEST_PAYLOAD_OFFSET = 2,
    REQUEST_AXIS_OUTPUTS_OFFSET = 3,
    REQUEST_MOTION_OFFSET = 5,
    REQUEST_CONTROLS_OFFSET = 6,
    REQUEST_RESERVED_AXES_OFFSET = 14,
    REQUEST_AXIS_VALUES_OFFSET = 16,
    REQUEST_MODE_BUTTONS_OFFSET = 20,
    REQUEST_AXIS_REPORT_ENABLED_OFFSET = 21,
    REQUEST_AUXILIARY_OFFSET = 22,
    REQUEST_REPORT_MODE_OFFSET = 26,
    REQUEST_RESERVED_REPORT_OFFSET = 27,
    REQUEST_REPORT_CAPABILITIES_OFFSET = 28,
    REQUEST_AXIS_LIMIT_OFFSET = 29,
};

/**
 * @brief Reads one little-endian 16-bit common-packet field.
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
 * @brief Writes one little-endian 16-bit common-packet field.
 *
 * Splits a logical value into two adjacent bytes without requiring aligned storage.
 *
 * @param[out] data First byte of the field.
 * @param[in] value Logical value to encode.
 */
static void write_little_endian_u16(uint8_t *data, uint16_t value) {
    data[0] = (uint8_t)value;
    data[1] = (uint8_t)(value >> 8);
}

/**
 * @brief Decodes a common attached-wheel request.
 *
 * Reads the 30-byte payload into logical button, axis, motion, control, report, and capability
 * fields without relying on packed structures.
 *
 * @param[in] request First 32 request bytes, including the command and reserved prefix.
 * @param[out] input Logical common input populated from the request payload.
 */
void wheel_packet_common_decode(const uint8_t request[WHEEL_PACKET_COMMON_REQUEST_SIZE],
                                WheelPacketCommonInput *input) {
    const uint8_t *payload = &request[REQUEST_PAYLOAD_OFFSET];
    for (uint8_t index = 0; index < WHEEL_PACKET_COMMON_BUTTON_COUNT; index++) {
        input->buttons[index] = payload[index];
    }
    input->axis_outputs[0] = payload[REQUEST_AXIS_OUTPUTS_OFFSET];
    input->axis_outputs[1] = payload[REQUEST_AXIS_OUTPUTS_OFFSET + 1];
    input->motion = (int8_t)payload[REQUEST_MOTION_OFFSET];
    for (uint8_t index = 0; index < WHEEL_PACKET_COMMON_CONTROL_COUNT; index++) {
        input->controls[index] = payload[REQUEST_CONTROLS_OFFSET + index];
    }
    input->reserved_axes[0] = payload[REQUEST_RESERVED_AXES_OFFSET];
    input->reserved_axes[1] = payload[REQUEST_RESERVED_AXES_OFFSET + 1];
    for (uint8_t index = 0; index < WHEEL_PACKET_COMMON_AXIS_VALUE_COUNT; index++) {
        input->axis_values[index] =
            read_little_endian_u16(&payload[REQUEST_AXIS_VALUES_OFFSET + index * 2]);
    }
    input->mode_buttons = payload[REQUEST_MODE_BUTTONS_OFFSET];
    input->axis_report_enabled = payload[REQUEST_AXIS_REPORT_ENABLED_OFFSET];
    for (uint8_t index = 0; index < sizeof(input->auxiliary_data); index++) {
        input->auxiliary_data[index] = payload[REQUEST_AUXILIARY_OFFSET + index];
    }
    input->report_mode = payload[REQUEST_REPORT_MODE_OFFSET];
    input->reserved_report = payload[REQUEST_RESERVED_REPORT_OFFSET];
    input->report_capabilities = payload[REQUEST_REPORT_CAPABILITIES_OFFSET];
    input->axis_limit = payload[REQUEST_AXIS_LIMIT_OFFSET];
}

/**
 * @brief Builds a common attached-wheel change snapshot.
 *
 * Serializes all 30 logical payload fields after packet-family filtering and normalization.
 *
 * @param[in] input Logical common input.
 * @param[out] snapshot Thirty-byte change-detection destination.
 */
void wheel_packet_common_snapshot(const WheelPacketCommonInput *input,
                                  uint8_t snapshot[WHEEL_PACKET_COMMON_SNAPSHOT_SIZE]) {
    for (uint8_t index = 0; index < WHEEL_PACKET_COMMON_BUTTON_COUNT; index++) {
        snapshot[index] = input->buttons[index];
    }
    snapshot[REQUEST_AXIS_OUTPUTS_OFFSET] = input->axis_outputs[0];
    snapshot[REQUEST_AXIS_OUTPUTS_OFFSET + 1] = input->axis_outputs[1];
    snapshot[REQUEST_MOTION_OFFSET] = (uint8_t)input->motion;
    for (uint8_t index = 0; index < WHEEL_PACKET_COMMON_CONTROL_COUNT; index++) {
        snapshot[REQUEST_CONTROLS_OFFSET + index] = input->controls[index];
    }
    snapshot[REQUEST_RESERVED_AXES_OFFSET] = input->reserved_axes[0];
    snapshot[REQUEST_RESERVED_AXES_OFFSET + 1] = input->reserved_axes[1];
    for (uint8_t index = 0; index < WHEEL_PACKET_COMMON_AXIS_VALUE_COUNT; index++) {
        write_little_endian_u16(&snapshot[REQUEST_AXIS_VALUES_OFFSET + index * 2],
                                input->axis_values[index]);
    }
    snapshot[REQUEST_MODE_BUTTONS_OFFSET] = input->mode_buttons;
    snapshot[REQUEST_AXIS_REPORT_ENABLED_OFFSET] = input->axis_report_enabled;
    for (uint8_t index = 0; index < sizeof(input->auxiliary_data); index++) {
        snapshot[REQUEST_AUXILIARY_OFFSET + index] = input->auxiliary_data[index];
    }
    snapshot[REQUEST_REPORT_MODE_OFFSET] = input->report_mode;
    snapshot[REQUEST_RESERVED_REPORT_OFFSET] = input->reserved_report;
    snapshot[REQUEST_REPORT_CAPABILITIES_OFFSET] = input->report_capabilities;
    snapshot[REQUEST_AXIS_LIMIT_OFFSET] = input->axis_limit;
}

/**
 * @brief Encodes the common authenticated attached-wheel response.
 *
 * Writes command A6, three display glyphs, the third-glyph marker, two vibration channels, and
 * both legacy axes.
 *
 * @param[in] display Current three-glyph display output.
 * @param[in] vibration Two attached-wheel vibration channels.
 * @param[in] legacy_axes Two legacy output axes.
 * @param[out] response Nine-byte response destination.
 */
void wheel_packet_common_response_encode(const WheelDisplayOutput *display,
                                         const uint8_t vibration[2], const uint8_t legacy_axes[2],
                                         uint8_t response[WHEEL_PACKET_COMMON_RESPONSE_SIZE]) {
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
