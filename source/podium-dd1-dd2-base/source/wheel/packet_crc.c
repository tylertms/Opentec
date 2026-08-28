#include "wheel/packet_crc.h"

#include <stdbool.h>
#include <stdint.h>

enum {
    WHEEL_PACKET_COMMAND_SELECT_MODE = 0xa5,
    WHEEL_PACKET_COMMAND_AUTHENTICATE = 0xa6,
    WHEEL_PACKET_CRC_MODE = 6,
    WHEEL_PACKET_CRC_AUTHENTICATED_MODE = 0x15,
    REQUEST_PAYLOAD_OFFSET = 2,
    REQUEST_AXIS_OUTPUTS_OFFSET = 3,
    REQUEST_MOTION_OFFSET = 5,
    REQUEST_CONTROLS_OFFSET = 6,
    REQUEST_RESERVED_AXES_OFFSET = 14,
    REQUEST_AXIS_VALUES_OFFSET = 16,
    REQUEST_MODE_BUTTONS_OFFSET = 20,
    REQUEST_AXIS_REPORT_ENABLED_OFFSET = 21,
    REQUEST_AUXILIARY_DATA_OFFSET = 22,
    REQUEST_REPORT_MODE_OFFSET = 26,
    REQUEST_RESERVED_REPORT_OFFSET = 27,
    REQUEST_REPORT_CAPABILITIES_OFFSET = 28,
    REQUEST_AXIS_LIMIT_OFFSET = 29,
};

static uint16_t read_little_endian_u16(const uint8_t *data) {
    return (uint16_t)data[0] | (uint16_t)data[1] << 8;
}

/**
 * @brief Reports whether a wheel mode uses the shared CRC packet codec.
 *
 * Selects the standard mode-6 exchange and its authenticated mode-0x15 variant.
 *
 * @param[in] wheel_mode Selected attached-wheel mode.
 * @return True for mode 6 or mode 0x15.
 */
bool wheel_packet_crc_applies(uint8_t wheel_mode) {
    return wheel_mode == WHEEL_PACKET_CRC_MODE || wheel_mode == WHEEL_PACKET_CRC_AUTHENTICATED_MODE;
}

/**
 * @brief Clears the CRC-family input histories.
 *
 * Zeros the three primary-button samples and three five-byte control samples, then resets both
 * insertion positions.
 *
 * @param[out] filter CRC-family filter state to initialize.
 */
void wheel_packet_crc_filter_init(WheelPacketCrcFilter *filter) {
    for (uint8_t sample = 0; sample < WHEEL_PACKET_CRC_HISTORY_DEPTH; sample++) {
        for (uint8_t button = 0; button < WHEEL_PACKET_CRC_BUTTON_COUNT; button++) {
            filter->button_samples[sample][button] = 0;
        }
        for (uint8_t control = 0; control < WHEEL_PACKET_CRC_FILTERED_CONTROL_COUNT; control++) {
            filter->control_samples[sample][control] = 0;
        }
    }
    filter->next_button_sample = 0;
    filter->next_control_sample = 0;
}

/**
 * @brief Decodes a CRC-family attached-wheel request.
 *
 * Reads the 30-byte payload into logical button, axis, motion, control, report, and capability
 * fields without depending on packed structures.
 *
 * @param[in] request First 33 request bytes, including the command, reserved prefix, and CRC.
 * @param[out] input Logical CRC-family input populated from the request payload.
 */
void wheel_packet_crc_decode(const uint8_t request[WHEEL_PACKET_CRC_REQUEST_SIZE],
                             WheelPacketCrcInput *input) {
    const uint8_t *payload = &request[REQUEST_PAYLOAD_OFFSET];
    for (uint8_t index = 0; index < WHEEL_PACKET_CRC_BUTTON_COUNT; index++) {
        input->buttons[index] = payload[index];
    }
    input->axis_outputs[0] = payload[REQUEST_AXIS_OUTPUTS_OFFSET];
    input->axis_outputs[1] = payload[REQUEST_AXIS_OUTPUTS_OFFSET + 1];
    input->motion = (int8_t)payload[REQUEST_MOTION_OFFSET];
    for (uint8_t index = 0; index < WHEEL_PACKET_CRC_CONTROL_COUNT; index++) {
        input->controls[index] = payload[REQUEST_CONTROLS_OFFSET + index];
    }
    input->reserved_axes[0] = payload[REQUEST_RESERVED_AXES_OFFSET];
    input->reserved_axes[1] = payload[REQUEST_RESERVED_AXES_OFFSET + 1];
    for (uint8_t index = 0; index < WHEEL_PACKET_CRC_AXIS_VALUE_COUNT; index++) {
        input->axis_values[index] =
            read_little_endian_u16(&payload[REQUEST_AXIS_VALUES_OFFSET + index * 2]);
    }
    input->mode_buttons = payload[REQUEST_MODE_BUTTONS_OFFSET];
    input->axis_report_enabled = payload[REQUEST_AXIS_REPORT_ENABLED_OFFSET];
    for (uint8_t index = 0; index < 4; index++) {
        input->auxiliary_data[index] = payload[REQUEST_AUXILIARY_DATA_OFFSET + index];
    }
    input->report_mode = payload[REQUEST_REPORT_MODE_OFFSET];
    input->reserved_report = payload[REQUEST_RESERVED_REPORT_OFFSET];
    input->report_capabilities = payload[REQUEST_REPORT_CAPABILITIES_OFFSET];
    input->axis_limit = payload[REQUEST_AXIS_LIMIT_OFFSET];
}

/**
 * @brief Filters CRC-family buttons and auxiliary controls.
 *
 * Keeps bits present in all three recent samples of the three primary button bytes and the first
 * five control bytes, then advances the two independent circular histories.
 *
 * @param[in,out] filter Primary-button and control histories with their insertion positions.
 * @param[in,out] input Input added to the histories and filtered in place.
 */
void wheel_packet_crc_filter(WheelPacketCrcFilter *filter, WheelPacketCrcInput *input) {
    for (uint8_t button = 0; button < WHEEL_PACKET_CRC_BUTTON_COUNT; button++) {
        filter->button_samples[filter->next_button_sample][button] = input->buttons[button];
        input->buttons[button] = filter->button_samples[0][button] &
                                 filter->button_samples[1][button] &
                                 filter->button_samples[2][button];
    }
    filter->next_button_sample++;
    if (filter->next_button_sample == WHEEL_PACKET_CRC_HISTORY_DEPTH) {
        filter->next_button_sample = 0;
    }

    for (uint8_t control = 0; control < WHEEL_PACKET_CRC_FILTERED_CONTROL_COUNT; control++) {
        filter->control_samples[filter->next_control_sample][control] = input->controls[control];
        input->controls[control] = filter->control_samples[0][control] &
                                   filter->control_samples[1][control] &
                                   filter->control_samples[2][control];
    }
    filter->next_control_sample++;
    if (filter->next_control_sample == WHEEL_PACKET_CRC_HISTORY_DEPTH) {
        filter->next_control_sample = 0;
    }
}

/**
 * @brief Reduces CRC-family control bytes to their retained protocol bits.
 *
 * Keeps bits 3 through 5 of the first control byte and bit 7 of the second, clears the next two
 * control bytes, and leaves the remaining four bytes unchanged.
 *
 * @param[in,out] input CRC-family input whose control bytes are sanitized in place.
 */
void wheel_packet_crc_sanitize_controls(WheelPacketCrcInput *input) {
    input->controls[0] &= 0x38u;
    input->controls[1] &= 0x80u;
    input->controls[2] = 0;
    input->controls[3] = 0;
}

/**
 * @brief Encodes a CRC-family attached-wheel response payload.
 *
 * Writes the mode-specific command, three display glyphs, optional third-glyph marker, two legacy
 * axes, report state, and one-shot status-update marker. The caller supplies the CRC byte.
 *
 * @param[in] wheel_mode Selected mode 6 or mode 0x15.
 * @param[in,out] output Current response values whose pending status marker is consumed.
 * @param[out] response Thirty-three-byte destination buffer for the encoded response fields.
 */
void wheel_packet_crc_encode(uint8_t wheel_mode, WheelPacketCrcOutput *output,
                             uint8_t response[WHEEL_PACKET_CRC_RESPONSE_SIZE]) {
    response[0] = wheel_mode == WHEEL_PACKET_CRC_AUTHENTICATED_MODE
                      ? WHEEL_PACKET_COMMAND_AUTHENTICATE
                      : WHEEL_PACKET_COMMAND_SELECT_MODE;
    response[1] = 0;
    for (uint8_t index = 0; index < WHEEL_DISPLAY_GLYPH_COUNT; index++) {
        response[index + 2] = output->display.glyphs[index];
    }
    if (output->display.third_glyph_marker) {
        response[4] |= 0x80u;
    }
    response[7] = output->legacy_axes[0];
    response[8] = output->legacy_axes[1];
    response[9] = output->report_state;
    response[10] = output->status_update_pending ? UINT8_MAX : 0;
    output->status_update_pending = false;
}
