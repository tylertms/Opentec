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
    INTERFACE_MODE_PODIUM_DD = 0,
    INTERFACE_MODE_XBOX_GIP = 6,
    INTERFACE_MODE_PLAYSTATION_4 = 7,
};

static uint16_t read_little_endian_u16(const uint8_t *data) {
    return (uint16_t)data[0] | (uint16_t)data[1] << 8;
}

static uint8_t read_bit(uint8_t value, uint8_t bit) { return (value >> bit) & 1u; }

static void assign_bit(uint8_t *value, uint8_t destination_bit, uint8_t source) {
    uint8_t mask = (uint8_t)(1u << destination_bit);
    *value = (uint8_t)((*value & (uint8_t)~mask) | ((source & 1u) << destination_bit));
}

static void merge_bit(uint8_t *value, uint8_t destination_bit, uint8_t source) {
    *value |= (uint8_t)((source & 1u) << destination_bit);
}

static void map_standard_buttons(WheelPacketCrcInput *input) {
    static const uint8_t first_destinations[8] = {3, 0, 4, 1, 5, 2, 6, 7};
    static const uint8_t second_destinations[8] = {5, 7, 6, 0, 3, 1, 2, 4};
    for (uint8_t bit = 0; bit < 8; bit++) {
        merge_bit(&input->buttons[1], first_destinations[bit], read_bit(input->controls[2], bit));
        merge_bit(&input->buttons[0], second_destinations[bit], read_bit(input->controls[3], bit));
    }
}

static void map_xbox_direct_buttons(WheelPacketCrcInput *input) {
    assign_bit(&input->buttons[1], 7,
               read_bit(input->controls[1], 1) | read_bit(input->controls[2], 7));
    assign_bit(&input->buttons[0], 4,
               read_bit(input->controls[1], 2) | read_bit(input->controls[2], 6));
    assign_bit(&input->buttons[0], 6,
               read_bit(input->controls[1], 3) | read_bit(input->controls[3], 2));
    assign_bit(&input->buttons[1], 1,
               read_bit(input->controls[1], 4) | read_bit(input->controls[2], 3));
    assign_bit(&input->buttons[1], 2,
               read_bit(input->controls[1], 6) | read_bit(input->controls[2], 5));
    assign_bit(&input->buttons[1], 5,
               read_bit(input->controls[0], 0) | read_bit(input->controls[2], 4));
    assign_bit(&input->buttons[2], 6,
               read_bit(input->controls[2], 1) | read_bit(input->controls[1], 5));
    assign_bit(&input->buttons[2], 7,
               read_bit(input->controls[2], 0) | read_bit(input->controls[0], 1));
    assign_bit(&input->buttons[1], 4,
               read_bit(input->controls[1], 2) | read_bit(input->controls[2], 2));
    assign_bit(&input->buttons[0], 7,
               read_bit(input->controls[1], 6) | read_bit(input->controls[3], 1));
    assign_bit(&input->buttons[0], 5,
               read_bit(input->controls[1], 7) | read_bit(input->controls[3], 0));
    assign_bit(&input->buttons[1], 6,
               read_bit(input->controls[1], 0) | read_bit(input->controls[2], 6));
    merge_bit(&input->buttons[2], 3, read_bit(input->controls[1], 4));
}

static void map_xbox_buttons(WheelPacketCrcInput *input, bool adapter_connected) {
    static const uint8_t first_destinations[6] = {4, 1, 5, 2, 6, 7};
    for (uint8_t bit = 2; bit < 8; bit++) {
        assign_bit(&input->buttons[1], first_destinations[bit - 2],
                   read_bit(input->controls[2], bit));
    }
    static const uint8_t second_destinations[3] = {5, 7, 6};
    for (uint8_t bit = 0; bit < 3; bit++) {
        assign_bit(&input->buttons[0], second_destinations[bit], read_bit(input->controls[3], bit));
    }
    static const uint8_t merged_destinations[4] = {1, 2, 0, 3};
    static const uint8_t merged_sources[4] = {5, 6, 3, 4};
    for (uint8_t index = 0; index < 4; index++) {
        merge_bit(&input->buttons[0], merged_destinations[index],
                  read_bit(input->controls[3], merged_sources[index]));
    }
    assign_bit(&input->buttons[0], 4, read_bit(input->controls[3], 7));
    if (adapter_connected) {
        assign_bit(&input->buttons[2], 6, read_bit(input->controls[2], 1));
        assign_bit(&input->buttons[2], 7, read_bit(input->controls[2], 0));
    } else {
        map_xbox_direct_buttons(input);
    }
    merge_bit(&input->buttons[1], 3, read_bit(input->controls[4], 1));
    merge_bit(&input->buttons[1], 0, read_bit(input->controls[4], 3));
}

static void merge_adapter_input(WheelPacketCrcInput *input, uint8_t interface_mode,
                                WheelPacketCrcAdapter *adapter) {
    merge_bit(&input->buttons[0], 1, read_bit(adapter->buttons[0], 1));
    merge_bit(&input->buttons[0], 2, read_bit(adapter->buttons[0], 2));
    merge_bit(&input->buttons[0], 0, read_bit(adapter->buttons[0], 0));
    merge_bit(&input->buttons[0], 3, read_bit(adapter->buttons[0], 3));
    merge_bit(&input->buttons[2], 5, read_bit(adapter->buttons[2], 4));
    if (adapter->mode != 1) {
        merge_bit(&input->buttons[2], 2, read_bit(adapter->buttons[2], 2));
        merge_bit(&input->buttons[2], 6, read_bit(adapter->buttons[2], 3));
        merge_bit(&input->buttons[1], 6, read_bit(adapter->buttons[1], 0));
    }
    merge_bit(&input->buttons[1], 7, read_bit(adapter->buttons[1], 5));
    if (adapter->mode != 1) {
        if (interface_mode == INTERFACE_MODE_XBOX_GIP) {
            merge_bit(&input->buttons[0], 7, read_bit(adapter->buttons[1], 1));
        } else {
            merge_bit(&input->buttons[0], 6, read_bit(adapter->buttons[1], 1));
            merge_bit(&input->buttons[1], 2, read_bit(adapter->buttons[0], 7));
        }
    }

    input->axis_outputs[0] = (uint8_t)(0x7fu - adapter->axes[1]);
    input->axis_outputs[1] = (uint8_t)(0x7fu - adapter->axes[0]);
    adapter->buttons_active =
        adapter->buttons[0] != 0 || adapter->buttons[1] != 0 || adapter->buttons[2] != 0;
    if (adapter->primary_delta > 0) {
        input->motion = 1;
        adapter->primary_delta--;
    } else if (adapter->primary_delta < 0) {
        input->motion = -1;
        adapter->primary_delta++;
    } else {
        input->motion = 0;
    }
}

static void write_snapshot(const WheelPacketCrcInput *input,
                           uint8_t snapshot[WHEEL_PACKET_CRC_SNAPSHOT_SIZE]) {
    for (uint8_t index = 0; index < WHEEL_PACKET_CRC_BUTTON_COUNT; index++) {
        snapshot[index] = input->buttons[index];
    }
    snapshot[REQUEST_AXIS_OUTPUTS_OFFSET] = input->axis_outputs[0];
    snapshot[REQUEST_AXIS_OUTPUTS_OFFSET + 1] = input->axis_outputs[1];
    snapshot[REQUEST_MOTION_OFFSET] = (uint8_t)input->motion;
    for (uint8_t index = 0; index < WHEEL_PACKET_CRC_CONTROL_COUNT; index++) {
        snapshot[REQUEST_CONTROLS_OFFSET + index] = input->controls[index];
    }
    snapshot[REQUEST_RESERVED_AXES_OFFSET] = input->reserved_axes[0];
    snapshot[REQUEST_RESERVED_AXES_OFFSET + 1] = input->reserved_axes[1];
    for (uint8_t index = 0; index < WHEEL_PACKET_CRC_AXIS_VALUE_COUNT; index++) {
        snapshot[REQUEST_AXIS_VALUES_OFFSET + index * 2] = (uint8_t)input->axis_values[index];
        snapshot[REQUEST_AXIS_VALUES_OFFSET + index * 2 + 1] =
            (uint8_t)(input->axis_values[index] >> 8);
    }
    snapshot[REQUEST_MODE_BUTTONS_OFFSET] = input->mode_buttons;
    snapshot[REQUEST_AXIS_REPORT_ENABLED_OFFSET] = input->axis_report_enabled;
    for (uint8_t index = 0; index < 4; index++) {
        snapshot[REQUEST_AUXILIARY_DATA_OFFSET + index] = input->auxiliary_data[index];
    }
    snapshot[REQUEST_REPORT_MODE_OFFSET] = input->report_mode;
    snapshot[REQUEST_RESERVED_REPORT_OFFSET] = input->reserved_report;
    snapshot[REQUEST_REPORT_CAPABILITIES_OFFSET] = input->report_capabilities;
    snapshot[REQUEST_AXIS_LIMIT_OFFSET] = input->axis_limit;
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
 * Zeros the three primary-button, five-byte control, and two-byte axis histories, then resets all
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
        for (uint8_t axis = 0; axis < WHEEL_PACKET_CRC_AXIS_VALUE_COUNT; axis++) {
            filter->axis_samples[sample][axis] = 0;
        }
    }
    filter->next_button_sample = 0;
    filter->next_control_sample = 0;
    filter->next_axis_sample = 0;
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
 * @brief Prepares CRC-family input for history filtering.
 *
 * In authenticated mode on the Podium DD interface, moves two auxiliary button bits into the third
 * primary button byte and clears their original and superseded positions.
 *
 * @param[in,out] input Decoded CRC-family input updated before filtering.
 * @param[in] wheel_mode Selected attached-wheel mode.
 * @param[in] interface_mode Active wheel interface mode.
 */
void wheel_packet_crc_prepare(WheelPacketCrcInput *input, uint8_t wheel_mode,
                              uint8_t interface_mode) {
    if (interface_mode != INTERFACE_MODE_PODIUM_DD ||
        wheel_mode != WHEEL_PACKET_CRC_AUTHENTICATED_MODE) {
        return;
    }
    assign_bit(&input->buttons[2], 2, read_bit(input->controls[1], 5));
    assign_bit(&input->buttons[2], 3, read_bit(input->controls[0], 1));
    input->buttons[1] &= 0xf6u;
    input->controls[1] &= 0xdfu;
    input->controls[0] &= 0xfdu;
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
 * @brief Smooths the CRC-family auxiliary axis pair.
 *
 * Stores the current two axis bytes in a three-sample circular history and replaces each byte with
 * the unsigned arithmetic mean of its history. Empty startup samples contribute zero.
 *
 * @param[in,out] filter Axis histories and their shared insertion position.
 * @param[in,out] input Normalized CRC-family input whose two axis bytes are averaged in place.
 */
void wheel_packet_crc_smooth_axes(WheelPacketCrcFilter *filter, WheelPacketCrcInput *input) {
    for (uint8_t axis = 0; axis < WHEEL_PACKET_CRC_AXIS_VALUE_COUNT; axis++) {
        filter->axis_samples[filter->next_axis_sample][axis] = input->controls[axis + 4];
        uint16_t total = 0;
        for (uint8_t sample = 0; sample < WHEEL_PACKET_CRC_HISTORY_DEPTH; sample++) {
            total += filter->axis_samples[sample][axis];
        }
        input->controls[axis + 4] = (uint8_t)(total / WHEEL_PACKET_CRC_HISTORY_DEPTH);
    }
    filter->next_axis_sample++;
    if (filter->next_axis_sample == WHEEL_PACKET_CRC_HISTORY_DEPTH) {
        filter->next_axis_sample = 0;
    }
}

/**
 * @brief Normalizes CRC-family wheel and adapter input.
 *
 * Applies the standard or Xbox interface button map, masks primary buttons when an adapter is
 * connected, retains the defined control bits, and merges adapter buttons, axes, and motion.
 *
 * @param[in,out] input Filtered CRC-family input updated to its normalized values.
 * @param[in] wheel_mode Selected attached-wheel mode.
 * @param[in] interface_mode Active wheel interface mode.
 * @param[in,out] adapter Adapter input and its consumed motion delta, or null for a direct wheel.
 */
void wheel_packet_crc_normalize(WheelPacketCrcInput *input, uint8_t wheel_mode,
                                uint8_t interface_mode, WheelPacketCrcAdapter *adapter) {
    bool adapter_connected = adapter != 0 && adapter->connected;
    if (adapter_connected) {
        input->buttons[0] &= 0x0fu;
        input->buttons[1] &= 0x09u;
    }
    if (interface_mode == INTERFACE_MODE_XBOX_GIP) {
        map_xbox_buttons(input, adapter_connected);
    } else {
        map_standard_buttons(input);
        if (wheel_mode != WHEEL_PACKET_CRC_AUTHENTICATED_MODE) {
            merge_bit(&input->buttons[2], 2, read_bit(input->controls[1], 3));
        }
        if (interface_mode == INTERFACE_MODE_PLAYSTATION_4) {
            merge_bit(&input->buttons[1], 3, read_bit(input->controls[4], 1));
            merge_bit(&input->buttons[1], 0, read_bit(input->controls[4], 3));
        }
    }
    input->controls[0] &= 0x38u;
    input->controls[1] &= 0x80u;
    input->controls[2] = 0;
    input->controls[3] = 0;
    if (adapter_connected) {
        merge_adapter_input(input, interface_mode, adapter);
    }
}

/**
 * @brief Encodes normalized CRC-family input as a protocol request view.
 *
 * Writes the logical button, axis, motion, control, report, and capability fields into their
 * 30-byte payload positions.
 *
 * @param[in] input Normalized CRC-family input to encode.
 * @param[out] snapshot Thirty-byte protocol request view.
 */
void wheel_packet_crc_snapshot(const WheelPacketCrcInput *input,
                               uint8_t snapshot[WHEEL_PACKET_CRC_SNAPSHOT_SIZE]) {
    write_snapshot(input, snapshot);
}

/**
 * @brief Encodes a CRC-family attached-wheel response payload.
 *
 * Writes the mode-specific command, three display glyphs, optional third-glyph marker, two
 * vibration channels, two legacy axes, report state, and one-shot status-update marker. The caller
 * supplies the CRC byte.
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
    response[5] = output->vibration[0];
    response[6] = output->vibration[1];
    response[7] = output->legacy_axes[0];
    response[8] = output->legacy_axes[1];
    response[9] = output->report_state;
    response[10] = output->status_update_pending ? UINT8_MAX : 0;
    output->status_update_pending = false;
}
