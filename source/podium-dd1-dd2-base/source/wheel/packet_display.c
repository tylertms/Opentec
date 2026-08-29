#include "wheel/packet_display.h"

#include <stdbool.h>
#include <stdint.h>

enum {
    WHEEL_PACKET_DISPLAY_MODE = 0x10,
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
 * @brief Reads one little-endian 16-bit display-packet field.
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
 * @brief Writes one little-endian 16-bit display-packet field.
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
 * @brief Reports whether a wheel mode uses the standard display packet codec.
 *
 * Selects operating mode 0x10.
 *
 * @param[in] wheel_mode Selected attached-wheel mode.
 * @return True for mode 0x10; otherwise false.
 */
bool wheel_packet_display_applies(uint8_t wheel_mode) {
    return wheel_mode == WHEEL_PACKET_DISPLAY_MODE;
}

/**
 * @brief Clears the display-packet input history.
 *
 * Zeros all button and control samples and resets the insertion position.
 *
 * @param[out] filter Three-sample display-packet filter to initialize.
 */
void wheel_packet_display_filter_init(WheelPacketDisplayFilter *filter) {
    for (uint8_t sample = 0; sample < WHEEL_PACKET_DISPLAY_HISTORY_DEPTH; sample++) {
        for (uint8_t field = 0; field < WHEEL_PACKET_DISPLAY_FILTER_WIDTH; field++) {
            filter->samples[sample][field] = 0;
        }
    }
    filter->next_sample = 0;
}

/**
 * @brief Decodes a standard display attached-wheel request.
 *
 * Reads the 30-byte payload into logical button, axis, motion, control, report, and capability
 * fields without relying on packed structures.
 *
 * @param[in] request First 32 request bytes, including the command and reserved prefix.
 * @param[out] input Logical display-family input populated from the request payload.
 */
void wheel_packet_display_decode(const uint8_t request[WHEEL_PACKET_DISPLAY_REQUEST_SIZE],
                                 WheelPacketDisplayInput *input) {
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
 * @brief Filters one standard display-packet sample.
 *
 * Keeps button bits and the first three control-byte bits present in all three recent samples,
 * then advances the shared insertion position.
 *
 * @param[in,out] filter Shared button and control history.
 * @param[in,out] input Input added to the history and filtered in place.
 */
void wheel_packet_display_filter(WheelPacketDisplayFilter *filter, WheelPacketDisplayInput *input) {
    uint8_t fields[WHEEL_PACKET_DISPLAY_FILTER_WIDTH] = {
        input->buttons[0],  input->buttons[1],  input->buttons[2],
        input->controls[0], input->controls[1], input->controls[2],
    };
    for (uint8_t field = 0; field < WHEEL_PACKET_DISPLAY_FILTER_WIDTH; field++) {
        filter->samples[filter->next_sample][field] = fields[field];
        fields[field] =
            filter->samples[0][field] & filter->samples[1][field] & filter->samples[2][field];
    }
    input->buttons[0] = fields[0];
    input->buttons[1] = fields[1];
    input->buttons[2] = fields[2];
    input->controls[0] = fields[3];
    input->controls[1] = fields[4];
    input->controls[2] = fields[5];
    filter->next_sample++;
    if (filter->next_sample == WHEEL_PACKET_DISPLAY_HISTORY_DEPTH) {
        filter->next_sample = 0;
    }
}

/**
 * @brief Builds a standard display-packet change snapshot.
 *
 * Serializes all 30 logical payload bytes after button and control filtering.
 *
 * @param[in] input Filtered display-family input.
 * @param[out] snapshot Thirty-byte change-detection destination.
 */
void wheel_packet_display_snapshot(const WheelPacketDisplayInput *input,
                                   uint8_t snapshot[WHEEL_PACKET_DISPLAY_SNAPSHOT_SIZE]) {
    for (uint8_t index = 0; index < WHEEL_PACKET_COMMON_BUTTON_COUNT; index++) {
        snapshot[index] = input->buttons[index];
    }
    snapshot[3] = input->axis_outputs[0];
    snapshot[4] = input->axis_outputs[1];
    snapshot[5] = (uint8_t)input->motion;
    for (uint8_t index = 0; index < WHEEL_PACKET_COMMON_CONTROL_COUNT; index++) {
        snapshot[REQUEST_CONTROLS_OFFSET + index] = input->controls[index];
    }
    snapshot[14] = input->reserved_axes[0];
    snapshot[15] = input->reserved_axes[1];
    for (uint8_t index = 0; index < WHEEL_PACKET_COMMON_AXIS_VALUE_COUNT; index++) {
        write_little_endian_u16(&snapshot[REQUEST_AXIS_VALUES_OFFSET + index * 2],
                                input->axis_values[index]);
    }
    snapshot[20] = input->mode_buttons;
    snapshot[21] = input->axis_report_enabled;
    for (uint8_t index = 0; index < sizeof(input->auxiliary_data); index++) {
        snapshot[REQUEST_AUXILIARY_OFFSET + index] = input->auxiliary_data[index];
    }
    snapshot[26] = input->report_mode;
    snapshot[27] = input->reserved_report;
    snapshot[28] = input->report_capabilities;
    snapshot[29] = input->axis_limit;
}

/**
 * @brief Encodes the standard display attached-wheel response.
 *
 * Writes command A6, three display glyphs, the third-glyph marker, two vibration channels, and
 * both legacy axes.
 *
 * @param[in] display Current three-glyph display output.
 * @param[in] vibration Two attached-wheel vibration channels.
 * @param[in] legacy_axes Two legacy output axes.
 * @param[out] response Nine-byte response destination.
 */
void wheel_packet_display_encode(const WheelDisplayOutput *display, const uint8_t vibration[2],
                                 const uint8_t legacy_axes[2],
                                 uint8_t response[WHEEL_PACKET_DISPLAY_RESPONSE_SIZE]) {
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
