#include "wheel/packet_mode_four.h"

#include <stdint.h>

enum {
    WHEEL_PACKET_COMMAND_SELECT_MODE = 0xa5,
    REQUEST_PAYLOAD_OFFSET = 2,
    REQUEST_AXIS_OUTPUTS_OFFSET = 3,
    REQUEST_MOTION_OFFSET = 5,
    REQUEST_CONTROLS_OFFSET = 6,
    REQUEST_CONTROL_DATA_OFFSET = 10,
    REQUEST_RESERVED_AXES_OFFSET = 14,
    REQUEST_AXIS_VALUES_OFFSET = 16,
    REQUEST_MODE_BUTTONS_OFFSET = 20,
    REQUEST_AXIS_REPORT_ENABLED_OFFSET = 21,
    REQUEST_AUXILIARY_DATA_OFFSET = 22,
    REQUEST_REPORT_MODE_OFFSET = 26,
    REQUEST_RESERVED_REPORT_OFFSET = 27,
    REQUEST_REPORT_CAPABILITIES_OFFSET = 28,
    REQUEST_AXIS_LIMIT_OFFSET = 29,
    INTERFACE_MODE_XBOX_GIP = 6,
    INTERFACE_MODE_PLAYSTATION_4 = 7,
    INTERFACE_MODE_LOGITECH_G27 = 4,
};

/**
 * @brief Reads a two-byte little-endian value.
 *
 * Combines the low byte followed by the high byte used for each mode-4 axis value.
 *
 * @param[in] data Two source bytes in low-byte-first order.
 * @return Decoded unsigned 16-bit value.
 */
static uint16_t read_little_endian_u16(const uint8_t *data) {
    return (uint16_t)data[0] | (uint16_t)data[1] << 8;
}

/**
 * @brief Assigns one source bit to one destination bit.
 *
 * Replaces the selected destination bit while preserving every other bit in the byte.
 *
 * @param[in,out] destination Byte containing the destination bit.
 * @param[in] destination_bit Destination bit position from zero through seven.
 * @param[in] source Byte containing the source bit.
 * @param[in] source_bit Source bit position from zero through seven.
 */
static void assign_bit(uint8_t *destination, uint8_t destination_bit, uint8_t source,
                       uint8_t source_bit) {
    uint8_t mask = (uint8_t)(1u << destination_bit);
    *destination = (uint8_t)((*destination & (uint8_t)~mask) |
                             (((source >> source_bit) & 1u) << destination_bit));
}

/**
 * @brief Maps legacy control bits into the two button bytes.
 *
 * Applies the interface-mode-zero-through-four button permutation from control bytes two and
 * three, replacing the corresponding button bits.
 *
 * @param[in,out] input Filtered mode-4 input containing controls and buttons.
 */
static void map_legacy_controls(WheelPacketModeFourInput *input) {
    static const uint8_t first_destinations[8] = {3, 0, 4, 1, 5, 2, 6, 7};
    static const uint8_t second_destinations[8] = {5, 7, 6, 0, 3, 1, 2, 4};
    for (uint8_t bit = 0; bit < 8; bit++) {
        assign_bit(&input->buttons[1], first_destinations[bit], input->controls[2], bit);
        assign_bit(&input->buttons[0], second_destinations[bit], input->controls[3], bit);
    }
}

/**
 * @brief Writes the normalized mode-4 request payload.
 *
 * Serializes every logical field into the 30-byte payload view with low-byte-first axis values.
 *
 * @param[in] input Normalized logical mode-4 input.
 * @param[out] snapshot Thirty-byte protocol payload destination.
 */
static void write_snapshot(const WheelPacketModeFourInput *input,
                           uint8_t snapshot[WHEEL_PACKET_MODE_FOUR_SNAPSHOT_SIZE]) {
    for (uint8_t index = 0; index < WHEEL_PACKET_MODE_FOUR_BUTTON_COUNT; index++) {
        snapshot[index] = input->buttons[index];
    }
    for (uint8_t index = 0; index < WHEEL_PACKET_MODE_FOUR_AXIS_OUTPUT_COUNT; index++) {
        snapshot[REQUEST_AXIS_OUTPUTS_OFFSET + index] = input->axis_outputs[index];
    }
    snapshot[REQUEST_MOTION_OFFSET] = (uint8_t)input->motion;
    for (uint8_t index = 0; index < WHEEL_PACKET_MODE_FOUR_CONTROL_COUNT; index++) {
        snapshot[REQUEST_CONTROLS_OFFSET + index] = input->controls[index];
        snapshot[REQUEST_CONTROL_DATA_OFFSET + index] = input->control_data[index];
    }
    for (uint8_t index = 0; index < 2; index++) {
        snapshot[REQUEST_RESERVED_AXES_OFFSET + index] = input->reserved_axes[index];
    }
    for (uint8_t index = 0; index < WHEEL_PACKET_MODE_FOUR_AXIS_VALUE_COUNT; index++) {
        snapshot[REQUEST_AXIS_VALUES_OFFSET + index * 2] = (uint8_t)input->axis_values[index];
        snapshot[REQUEST_AXIS_VALUES_OFFSET + index * 2 + 1] =
            (uint8_t)(input->axis_values[index] >> 8);
    }
    snapshot[REQUEST_MODE_BUTTONS_OFFSET] = input->mode_buttons;
    snapshot[REQUEST_AXIS_REPORT_ENABLED_OFFSET] = input->axis_report_enabled;
    for (uint8_t index = 0; index < WHEEL_PACKET_MODE_FOUR_AUXILIARY_DATA_COUNT; index++) {
        snapshot[REQUEST_AUXILIARY_DATA_OFFSET + index] = input->auxiliary_data[index];
    }
    snapshot[REQUEST_REPORT_MODE_OFFSET] = input->report_mode;
    snapshot[REQUEST_RESERVED_REPORT_OFFSET] = input->reserved_report;
    snapshot[REQUEST_REPORT_CAPABILITIES_OFFSET] = input->report_capabilities;
    snapshot[REQUEST_AXIS_LIMIT_OFFSET] = input->axis_limit;
}

/**
 * @brief Clears the mode-4 input histories.
 *
 * Zeros the three button samples and four control samples and resets both insertion positions.
 *
 * @param[out] filter Mode-4 filter state to initialize.
 */
void wheel_packet_mode_four_filter_init(WheelPacketModeFourFilter *filter) {
    for (uint8_t sample = 0; sample < WHEEL_PACKET_MODE_FOUR_BUTTON_HISTORY_DEPTH; sample++) {
        for (uint8_t button = 0; button < WHEEL_PACKET_MODE_FOUR_BUTTON_COUNT; button++) {
            filter->button_samples[sample][button] = 0;
        }
    }
    for (uint8_t sample = 0; sample < WHEEL_PACKET_MODE_FOUR_CONTROL_HISTORY_DEPTH; sample++) {
        for (uint8_t control = 0; control < WHEEL_PACKET_MODE_FOUR_CONTROL_COUNT; control++) {
            filter->control_samples[sample][control] = 0;
        }
    }
    filter->next_button_sample = 0;
    filter->next_control_sample = 0;
}

/**
 * @brief Decodes a mode-4 attached-wheel request.
 *
 * Reads the 30-byte payload into logical button, axis, motion, control, report, and capability
 * fields without depending on packed structures.
 *
 * @param[in] request First 32 request bytes, including the command and reserved prefix.
 * @param[out] input Logical mode-4 input populated from the request payload.
 */
void wheel_packet_mode_four_decode(const uint8_t request[WHEEL_PACKET_MODE_FOUR_REQUEST_SIZE],
                                   WheelPacketModeFourInput *input) {
    const uint8_t *payload = &request[REQUEST_PAYLOAD_OFFSET];
    for (uint8_t index = 0; index < WHEEL_PACKET_MODE_FOUR_BUTTON_COUNT; index++) {
        input->buttons[index] = payload[index];
    }
    for (uint8_t index = 0; index < WHEEL_PACKET_MODE_FOUR_AXIS_OUTPUT_COUNT; index++) {
        input->axis_outputs[index] = payload[REQUEST_AXIS_OUTPUTS_OFFSET + index];
    }
    input->motion = (int8_t)payload[REQUEST_MOTION_OFFSET];
    for (uint8_t index = 0; index < WHEEL_PACKET_MODE_FOUR_CONTROL_COUNT; index++) {
        input->controls[index] = payload[REQUEST_CONTROLS_OFFSET + index];
        input->control_data[index] = payload[REQUEST_CONTROL_DATA_OFFSET + index];
    }
    for (uint8_t index = 0; index < 2; index++) {
        input->reserved_axes[index] = payload[REQUEST_RESERVED_AXES_OFFSET + index];
    }
    for (uint8_t index = 0; index < WHEEL_PACKET_MODE_FOUR_AXIS_VALUE_COUNT; index++) {
        input->axis_values[index] =
            read_little_endian_u16(&payload[REQUEST_AXIS_VALUES_OFFSET + index * 2]);
    }
    input->mode_buttons = payload[REQUEST_MODE_BUTTONS_OFFSET];
    input->axis_report_enabled = payload[REQUEST_AXIS_REPORT_ENABLED_OFFSET];
    for (uint8_t index = 0; index < WHEEL_PACKET_MODE_FOUR_AUXILIARY_DATA_COUNT; index++) {
        input->auxiliary_data[index] = payload[REQUEST_AUXILIARY_DATA_OFFSET + index];
    }
    input->report_mode = payload[REQUEST_REPORT_MODE_OFFSET];
    input->reserved_report = payload[REQUEST_RESERVED_REPORT_OFFSET];
    input->report_capabilities = payload[REQUEST_REPORT_CAPABILITIES_OFFSET];
    input->axis_limit = payload[REQUEST_AXIS_LIMIT_OFFSET];
}

/**
 * @brief Filters mode-4 buttons and auxiliary controls.
 *
 * Keeps button bits present in all three recent button samples and control bits present in all
 * four recent control samples, then advances the independent circular histories.
 *
 * @param[in,out] filter Button and control histories with their insertion positions.
 * @param[in,out] input Input added to the histories and filtered in place.
 */
void wheel_packet_mode_four_filter(WheelPacketModeFourFilter *filter,
                                   WheelPacketModeFourInput *input) {
    for (uint8_t button = 0; button < WHEEL_PACKET_MODE_FOUR_BUTTON_COUNT; button++) {
        filter->button_samples[filter->next_button_sample][button] = input->buttons[button];
        input->buttons[button] = filter->button_samples[0][button] &
                                 filter->button_samples[1][button] &
                                 filter->button_samples[2][button];
    }
    filter->next_button_sample++;
    if (filter->next_button_sample == WHEEL_PACKET_MODE_FOUR_BUTTON_HISTORY_DEPTH) {
        filter->next_button_sample = 0;
    }

    for (uint8_t control = 0; control < WHEEL_PACKET_MODE_FOUR_CONTROL_COUNT; control++) {
        filter->control_samples[filter->next_control_sample][control] = input->controls[control];
        input->controls[control] =
            filter->control_samples[0][control] & filter->control_samples[1][control] &
            filter->control_samples[2][control] & filter->control_samples[3][control];
    }
    filter->next_control_sample++;
    if (filter->next_control_sample == WHEEL_PACKET_MODE_FOUR_CONTROL_HISTORY_DEPTH) {
        filter->next_control_sample = 0;
    }
}

/**
 * @brief Normalizes filtered mode-4 input for the protocol request view.
 *
 * Applies the interface-specific legacy button mappings, accumulates the mode-button extension,
 * latches axis-report availability, masks the retained control bytes, and emits the 30-byte view.
 *
 * @param[in,out] input Filtered mode-4 input updated to its normalized values.
 * @param[in] interface_mode Active wheel interface mode.
 * @param[in,out] runtime Persistent extension and axis-report latches.
 * @param[out] snapshot Normalized 30-byte protocol request view.
 */
void wheel_packet_mode_four_normalize(WheelPacketModeFourInput *input, uint8_t interface_mode,
                                      WheelPacketModeFourRuntime *runtime,
                                      uint8_t snapshot[WHEEL_PACKET_MODE_FOUR_SNAPSHOT_SIZE]) {
    if (interface_mode <= INTERFACE_MODE_LOGITECH_G27) {
        map_legacy_controls(input);
    }
    if (interface_mode == INTERFACE_MODE_PLAYSTATION_4) {
        assign_bit(&input->buttons[1], 3, input->mode_buttons, 1);
        assign_bit(&input->buttons[1], 0, input->mode_buttons, 3);
    }
    if (interface_mode == INTERFACE_MODE_XBOX_GIP ||
        interface_mode == INTERFACE_MODE_PLAYSTATION_4) {
        runtime->extended_buttons |= input->mode_buttons & 0x05u;
    }

    input->controls[0] &= 0x38u;
    input->controls[1] &= 0x80u;
    input->controls[2] = 0;
    input->controls[3] = 0;
    runtime->axis_report_enabled = runtime->axis_report_enabled != 0 ||
                                   input->axis_report_enabled != 0 || input->mode_buttons != 0;
    write_snapshot(input, snapshot);
}

/**
 * @brief Encodes a mode-4 attached-wheel response.
 *
 * Writes command A5, three display glyphs, the optional third-glyph marker, two vibration
 * bytes, and two legacy-axis bytes.
 *
 * @param[in] output Current display output, vibration channels, and legacy axes.
 * @param[out] response Nine-byte destination buffer.
 */
void wheel_packet_mode_four_encode(const WheelPacketModeFourOutput *output,
                                   uint8_t response[WHEEL_PACKET_MODE_FOUR_RESPONSE_SIZE]) {
    response[0] = WHEEL_PACKET_COMMAND_SELECT_MODE;
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
