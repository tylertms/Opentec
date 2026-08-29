#include "wheel/packet_mode_one.h"

#include <stdbool.h>
#include <stdint.h>

enum {
    WHEEL_PACKET_COMMAND_SELECT_MODE = 0xa5,
    WHEEL_PACKET_COMMAND_AUTHENTICATE = 0xa6,
    WHEEL_PACKET_VENDOR_MODE = 2,
    WHEEL_PACKET_AUTHENTICATION_MODE_FIRST = 0x13,
    WHEEL_PACKET_AUTHENTICATION_MODE_LAST = 0x14,
    WHEEL_PACKET_AUTHENTICATED_VENDOR_MODE = 0x16,
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
 * @brief Tests whether an attached-wheel mode uses the standard packet codec.
 *
 * Recognizes the standard and vendor modes that share the same input layout and processing rules.
 *
 * @param[in] wheel_mode Selected attached-wheel mode.
 * @return True for modes 1, 2, 3, 0x13, 0x14, and 0x16; otherwise false.
 */
bool wheel_packet_mode_one_applies(uint8_t wheel_mode) {
    return wheel_mode == 1 || wheel_mode == WHEEL_PACKET_VENDOR_MODE || wheel_mode == 3 ||
           wheel_mode == WHEEL_PACKET_AUTHENTICATION_MODE_FIRST ||
           wheel_mode == WHEEL_PACKET_AUTHENTICATION_MODE_LAST ||
           wheel_mode == WHEEL_PACKET_AUTHENTICATED_VENDOR_MODE;
}

/**
 * @brief Clears the standard attached-wheel button history.
 *
 * Zeros all three samples and resets the insertion position.
 *
 * @param[out] filter Three-sample button filter to initialize.
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
 * @brief Filters one standard attached-wheel button sample.
 *
 * Keeps only button bits present in all three recent samples and advances the insertion position.
 *
 * @param[in,out] filter Three-sample history and insertion position.
 * @param[in,out] input Input whose button bytes are added to the history and filtered in place.
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
 * @brief Clears the authenticated attached-wheel control-axis history.
 *
 * Zeros both axes in all three samples and resets the insertion position.
 *
 * @param[out] filter Three-sample control-axis filter to initialize.
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
 * @brief Filters authenticated attached-wheel control axes.
 *
 * Replaces both axes with their unsigned three-sample moving averages and advances the insertion
 * position.
 *
 * @param[in,out] filter Three-sample history and insertion position.
 * @param[in,out] input Input whose control axes are added to the history and averaged in place.
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
 * @brief Decodes a standard attached-wheel command-2 request.
 *
 * Reads the standard button, axis, motion, control, mode, and report fields from the request.
 *
 * @param[in] request First 32 request bytes, including the command and reserved prefix.
 * @param[out] input Logical input fields populated from the request payload.
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
 * @brief Synchronizes authenticated button-latch representations.
 *
 * Mirrors either active latch source into its corresponding button and control bit.
 *
 * @param[in,out] input Standard packet input containing both latch representations.
 */
static void synchronize_latched_buttons(WheelPacketModeOneInput *input) {
    if ((input->buttons[1] & 0x01u) != 0 || (input->controls.latch_flags & 0x01u) != 0) {
        input->buttons[1] |= 0x01u;
        input->controls.latch_flags |= 0x01u;
    }
    if ((input->buttons[1] & 0x08u) != 0 || (input->controls.latch_flags & 0x02u) != 0) {
        input->buttons[1] |= 0x08u;
        input->controls.latch_flags |= 0x02u;
    }
}

/**
 * @brief Normalizes standard attached-wheel input.
 *
 * Applies authenticated button latching, clears unsupported logical fields, and builds the
 * thirty-byte change-detection snapshot.
 *
 * @param[in,out] input Decoded input updated to the normalized logical values.
 * @param[in] authenticated True for operating modes 0x13 and 0x14.
 * @param[in] button_latch_enabled True when alternate button latching is active.
 * @param[in] profile_transition_pending True while a profile transition suppresses button
 * latching.
 * @param[out] snapshot Thirty-byte normalized destination.
 */
void wheel_packet_mode_one_normalize(WheelPacketModeOneInput *input, bool authenticated,
                                     bool button_latch_enabled, bool profile_transition_pending,
                                     uint8_t snapshot[WHEEL_PACKET_MODE_ONE_SNAPSHOT_SIZE]) {
    if (authenticated && button_latch_enabled && !profile_transition_pending) {
        synchronize_latched_buttons(input);
    }

    input->controls.values[0] = 0;
    input->controls.values[1] = 0;
    input->controls.enabled = 0;
    if (!authenticated) {
        input->controls.latch_flags = 0;
    }
    input->controls.x = 0;
    input->controls.y = 0;
    input->controls.mode = 0;
    input->controls.packed_values = 0;
    input->axis_values[0] = 0;
    input->axis_values[1] = 0;
    input->mode_buttons = 0;
    input->axis_report_enabled = 0;

    for (uint8_t index = 0; index < WHEEL_PACKET_MODE_ONE_SNAPSHOT_SIZE; index++) {
        snapshot[index] = 0;
    }
    for (uint8_t index = 0; index < WHEEL_PACKET_MODE_ONE_BUTTON_COUNT; index++) {
        snapshot[index] = input->buttons[index];
    }
    for (uint8_t index = 0; index < WHEEL_PACKET_MODE_ONE_AXIS_OUTPUT_COUNT; index++) {
        snapshot[3 + index] = input->axis_outputs[index];
    }
    snapshot[5] = (uint8_t)input->motion;
    if (authenticated) {
        snapshot[9] = input->controls.latch_flags;
    }
    snapshot[29] = input->axis_limit;
}

/**
 * @brief Encodes the shared standard attached-wheel response.
 *
 * Writes the command, display output, vibration channels, and legacy axes for the standard and
 * vendor packet modes. Modes 0x13, 0x14, and 0x16 use the authentication command byte.
 *
 * @param[in] wheel_mode Selected attached-wheel mode.
 * @param[in] output Current display output, vibration channels, and legacy axes.
 * @param[out] response Nine-byte destination buffer.
 */
void wheel_packet_mode_one_encode(uint8_t wheel_mode, const WheelPacketModeOneOutput *output,
                                  uint8_t response[WHEEL_PACKET_MODE_ONE_RESPONSE_SIZE]) {
    bool authenticated = (wheel_mode >= WHEEL_PACKET_AUTHENTICATION_MODE_FIRST &&
                          wheel_mode <= WHEEL_PACKET_AUTHENTICATION_MODE_LAST) ||
                         wheel_mode == WHEEL_PACKET_AUTHENTICATED_VENDOR_MODE;
    response[0] =
        authenticated ? WHEEL_PACKET_COMMAND_AUTHENTICATE : WHEEL_PACKET_COMMAND_SELECT_MODE;
    response[1] = 0;
    for (uint8_t index = 0; index < WHEEL_DISPLAY_GLYPH_COUNT; index++) {
        response[index + 2] = output->display.glyphs[index];
    }
    if (output->display.third_glyph_marker) {
        response[4] |= 0x80u;
    }
    response[5] = output->vibration[0];
    response[6] = output->vibration[1];
    response[7] = output->legacy_axes[0];
    response[8] = output->legacy_axes[1];
}
