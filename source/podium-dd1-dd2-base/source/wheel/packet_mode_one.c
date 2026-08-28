#include "wheel/packet_mode_one.h"

#include <stdbool.h>
#include <stdint.h>

enum {
    WHEEL_PACKET_COMMAND_SELECT_MODE = 0xa5,
    WHEEL_PACKET_COMMAND_AUTHENTICATE = 0xa6,
    WHEEL_PACKET_AUTHENTICATION_MODE_FIRST = 0x13,
    WHEEL_PACKET_AUTHENTICATION_MODE_LAST = 0x14,
    REQUEST_PAYLOAD_OFFSET = 2,
    REQUEST_BUTTONS_OFFSET = 0,
    REQUEST_AXIS_OUTPUTS_OFFSET = 3,
    REQUEST_MOTION_OFFSET = 5,
    REQUEST_CONTROLS_OFFSET = 6,
    REQUEST_AXIS_VALUES_OFFSET = 16,
    REQUEST_MODE_BUTTONS_OFFSET = 20,
    REQUEST_AXIS_REPORT_ENABLED_OFFSET = 21,
    REQUEST_REPORT_MODE_OFFSET = 26,
    REQUEST_REPORT_CAPABILITIES_OFFSET = 28,
    REQUEST_AXIS_LIMIT_OFFSET = 29,
};

static uint16_t read_little_endian_u16(const uint8_t *data) {
    return (uint16_t)data[0] | (uint16_t)data[1] << 8;
}

/**
 * Tests whether an attached-wheel mode uses the mode-1 packet codec.
 *
 * @param wheel_mode Selected attached-wheel mode.
 * @return True for modes 1, 3, 0x13, and 0x14; otherwise false.
 */
bool wheel_packet_mode_one_applies(uint8_t wheel_mode) {
    return wheel_mode == 1 || wheel_mode == 3 || wheel_mode == 0x13 || wheel_mode == 0x14;
}

/**
 * Clears the standard attached-wheel button history.
 *
 * @param filter Three-sample button filter to initialize.
 */
void wheel_packet_mode_one_button_filter_init(WheelPacketModeOneButtonFilter *filter) {
    for (uint8_t sample = 0; sample < WHEEL_PACKET_MODE_ONE_BUTTON_HISTORY_DEPTH; sample++) {
        for (uint8_t button = 0; button < WHEEL_PACKET_MODE_ONE_BUTTON_COUNT; button++) {
            filter->samples[sample][button] = 0;
        }
    }
    filter->next_sample = 0;
}

/**
 * Accepts one button sample and keeps only bits present in all three recent samples.
 *
 * @param filter Three-sample history and insertion position.
 * @param input Input whose button bytes are added to the history and filtered in place.
 */
void wheel_packet_mode_one_filter_buttons(WheelPacketModeOneButtonFilter *filter,
                                          WheelPacketModeOneInput *input) {
    for (uint8_t button = 0; button < WHEEL_PACKET_MODE_ONE_BUTTON_COUNT; button++) {
        filter->samples[filter->next_sample][button] = input->buttons[button];
        input->buttons[button] =
            filter->samples[0][button] & filter->samples[1][button] & filter->samples[2][button];
    }
    filter->next_sample++;
    if (filter->next_sample == WHEEL_PACKET_MODE_ONE_BUTTON_HISTORY_DEPTH) {
        filter->next_sample = 0;
    }
}

/**
 * Clears the authenticated attached-wheel control-axis history.
 *
 * @param filter Three-sample control-axis filter to initialize.
 */
void wheel_packet_mode_one_control_axis_filter_init(WheelPacketModeOneControlAxisFilter *filter) {
    for (uint8_t sample = 0; sample < WHEEL_PACKET_MODE_ONE_CONTROL_AXIS_HISTORY_DEPTH; sample++) {
        for (uint8_t axis = 0; axis < WHEEL_PACKET_MODE_ONE_CONTROL_AXIS_COUNT; axis++) {
            filter->samples[sample][axis] = 0;
        }
    }
    filter->next_sample = 0;
}

/**
 * Replaces both control axes with their unsigned three-sample moving averages.
 *
 * @param filter Three-sample history and insertion position.
 * @param input Input whose control axes are added to the history and averaged in place.
 */
void wheel_packet_mode_one_filter_control_axes(WheelPacketModeOneControlAxisFilter *filter,
                                               WheelPacketModeOneInput *input) {
    uint8_t axes[WHEEL_PACKET_MODE_ONE_CONTROL_AXIS_COUNT] = {input->controls.x, input->controls.y};
    for (uint8_t axis = 0; axis < WHEEL_PACKET_MODE_ONE_CONTROL_AXIS_COUNT; axis++) {
        filter->samples[filter->next_sample][axis] = axes[axis];
        uint16_t sum = 0;
        for (uint8_t sample = 0; sample < WHEEL_PACKET_MODE_ONE_CONTROL_AXIS_HISTORY_DEPTH;
             sample++) {
            sum += filter->samples[sample][axis];
        }
        axes[axis] = (uint8_t)(sum / WHEEL_PACKET_MODE_ONE_CONTROL_AXIS_HISTORY_DEPTH);
    }
    input->controls.x = axes[0];
    input->controls.y = axes[1];
    filter->next_sample++;
    if (filter->next_sample == WHEEL_PACKET_MODE_ONE_CONTROL_AXIS_HISTORY_DEPTH) {
        filter->next_sample = 0;
    }
}

/**
 * Decodes the standard attached-wheel input fields from a command-2 request.
 *
 * @param request First 32 request bytes, including the command and reserved prefix.
 * @param input Logical input fields populated from the request payload.
 */
void wheel_packet_mode_one_decode(const uint8_t request[WHEEL_PACKET_MODE_ONE_REQUEST_SIZE],
                                  WheelPacketModeOneInput *input) {
    const uint8_t *payload = &request[REQUEST_PAYLOAD_OFFSET];
    for (uint8_t index = 0; index < WHEEL_PACKET_MODE_ONE_BUTTON_COUNT; index++) {
        input->buttons[index] = payload[REQUEST_BUTTONS_OFFSET + index];
    }
    for (uint8_t index = 0; index < WHEEL_PACKET_MODE_ONE_AXIS_OUTPUT_COUNT; index++) {
        input->axis_outputs[index] = payload[REQUEST_AXIS_OUTPUTS_OFFSET + index];
    }
    input->motion = (int8_t)payload[REQUEST_MOTION_OFFSET];
    input->controls.values[0] = payload[REQUEST_CONTROLS_OFFSET];
    input->controls.values[1] = payload[REQUEST_CONTROLS_OFFSET + 1];
    input->controls.enabled = payload[REQUEST_CONTROLS_OFFSET + 2];
    input->controls.latch_flags = payload[REQUEST_CONTROLS_OFFSET + 3];
    input->controls.x = payload[REQUEST_CONTROLS_OFFSET + 4];
    input->controls.y = payload[REQUEST_CONTROLS_OFFSET + 5];
    input->controls.mode = payload[REQUEST_CONTROLS_OFFSET + 6];
    input->controls.packed_values = payload[REQUEST_CONTROLS_OFFSET + 7];
    for (uint8_t index = 0; index < WHEEL_PACKET_MODE_ONE_AXIS_VALUE_COUNT; index++) {
        input->axis_values[index] =
            read_little_endian_u16(&payload[REQUEST_AXIS_VALUES_OFFSET + index * 2]);
    }
    input->mode_buttons = payload[REQUEST_MODE_BUTTONS_OFFSET];
    input->axis_report_enabled = payload[REQUEST_AXIS_REPORT_ENABLED_OFFSET];
    input->report_mode = payload[REQUEST_REPORT_MODE_OFFSET];
    input->report_capabilities = payload[REQUEST_REPORT_CAPABILITIES_OFFSET];
    input->axis_limit = payload[REQUEST_AXIS_LIMIT_OFFSET];
}

/**
 * Encodes the shared nine-byte output used by attached-wheel modes 1, 3, 0x13, and 0x14.
 *
 * @param output Current operating mode, display output, display state, and link status.
 * @param response Nine-byte destination buffer.
 */
void wheel_packet_mode_one_encode(const WheelPacketModeOneOutput *output,
                                  uint8_t response[WHEEL_PACKET_MODE_ONE_RESPONSE_SIZE]) {
    response[0] = output->operating_mode >= WHEEL_PACKET_AUTHENTICATION_MODE_FIRST &&
                          output->operating_mode <= WHEEL_PACKET_AUTHENTICATION_MODE_LAST
                      ? WHEEL_PACKET_COMMAND_AUTHENTICATE
                      : WHEEL_PACKET_COMMAND_SELECT_MODE;
    response[1] = 0;
    for (uint8_t index = 0; index < WHEEL_DISPLAY_GLYPH_COUNT; index++) {
        response[index + 2] = output->display.glyphs[index];
    }
    if (output->display.third_glyph_marker) {
        response[4] |= 0x80u;
    }
    response[5] = output->display_state[0];
    response[6] = output->display_state[1];
    response[7] = output->link_status[0];
    response[8] = output->link_status[1];
}
