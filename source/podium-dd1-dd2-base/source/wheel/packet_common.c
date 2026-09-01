#include "wheel/packet_common.h"

#include <stdint.h>

/** @brief Internal common-packet identifiers, offsets, and normalization masks. */
enum {
    WHEEL_PACKET_COMMAND_AUTHENTICATE = 0xa6, /**< Authenticated response command. */
    REQUEST_PAYLOAD_OFFSET = 2,               /**< Request payload offset. */
    REQUEST_AXIS_OUTPUTS_OFFSET = 3,          /**< Axis-output offset within the payload. */
    REQUEST_MOTION_OFFSET = 5,                /**< Motion offset within the payload. */
    REQUEST_CONTROLS_OFFSET = 6,              /**< Control offset within the payload. */
    REQUEST_RESERVED_AXES_OFFSET = 14,        /**< Reserved-axis offset within the payload. */
    REQUEST_AXIS_VALUES_OFFSET = 16,          /**< Axis-value offset within the payload. */
    REQUEST_MODE_BUTTONS_OFFSET = 20,         /**< Mode-button offset within the payload. */
    REQUEST_AXIS_REPORT_ENABLED_OFFSET = 21,  /**< Axis-report flag offset. */
    REQUEST_AUXILIARY_OFFSET = 22,            /**< Auxiliary-data offset. */
    REQUEST_REPORT_MODE_OFFSET = 26,          /**< Report-mode offset. */
    REQUEST_RESERVED_REPORT_OFFSET = 27,      /**< Reserved-report offset. */
    REQUEST_REPORT_CAPABILITIES_OFFSET = 28,  /**< Report-capability offset. */
    REQUEST_AXIS_LIMIT_OFFSET = 29,           /**< Axis-limit offset. */
    FILTERED_AXIS_FIRST_CONTROL = 4, /**< First control index treated as an auxiliary axis. */
    PACKED_OUTPUT_CONTROL = 7,       /**< Control index carrying packed output bits. */
    FIRST_LATCH_BUTTON = 0x01,       /**< First latch button mask. */
    SECOND_LATCH_BUTTON = 0x08,      /**< Second latch button mask. */
    FIRST_LATCH_FLAG = 0x01,         /**< First latch-control flag. */
    SECOND_LATCH_FLAG = 0x02,        /**< Second latch-control flag. */
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
 * @brief Clears the common packet histories.
 *
 * Zeros the three button and analog-axis samples and returns the shared insertion position to the
 * first sample.
 *
 * @param[out] filter Common packet filter state to initialize.
 */
void wheel_packet_common_filter_init(WheelPacketCommonFilter *filter) {
    for (uint8_t sample = 0; sample < WHEEL_PACKET_COMMON_HISTORY_DEPTH; sample++) {
        for (uint8_t button = 0; button < WHEEL_PACKET_COMMON_BUTTON_COUNT; button++) {
            filter->button_samples[sample][button] = 0;
        }
        for (uint8_t axis = 0; axis < WHEEL_PACKET_COMMON_FILTERED_AXIS_COUNT; axis++) {
            filter->axis_samples[sample][axis] = 0;
        }
    }
    filter->next_sample = 0;
}

/**
 * @brief Filters common packet buttons and analog axes.
 *
 * Keeps button bits present in all three recent samples and replaces control bytes four and five
 * with their unsigned three-sample means. Empty startup samples contribute zero.
 *
 * @param[in,out] filter Shared button and analog-axis histories.
 * @param[in,out] input Decoded common input filtered in place.
 */
void wheel_packet_common_filter(WheelPacketCommonFilter *filter, WheelPacketCommonInput *input) {
    uint8_t sample = filter->next_sample;
    for (uint8_t button = 0; button < WHEEL_PACKET_COMMON_BUTTON_COUNT; button++) {
        filter->button_samples[sample][button] = input->buttons[button];
        input->buttons[button] = filter->button_samples[0][button] &
                                 filter->button_samples[1][button] &
                                 filter->button_samples[2][button];
    }
    for (uint8_t axis = 0; axis < WHEEL_PACKET_COMMON_FILTERED_AXIS_COUNT; axis++) {
        filter->axis_samples[sample][axis] = input->controls[FILTERED_AXIS_FIRST_CONTROL + axis];
        uint16_t total = 0;
        for (uint8_t history = 0; history < WHEEL_PACKET_COMMON_HISTORY_DEPTH; history++) {
            total += filter->axis_samples[history][axis];
        }
        input->controls[FILTERED_AXIS_FIRST_CONTROL + axis] =
            (uint8_t)(total / WHEEL_PACKET_COMMON_HISTORY_DEPTH);
    }
    filter->next_sample++;
    if (filter->next_sample == WHEEL_PACKET_COMMON_HISTORY_DEPTH) {
        filter->next_sample = 0;
    }
}

/**
 * @brief Expands a common packet's packed output controls.
 *
 * Replaces the first two control bytes with the low and high nibbles of control byte seven while
 * preserving the packet's selector and filtered analog controls.
 *
 * @param[in,out] input Filtered common input updated in place.
 */
void wheel_packet_common_expand_packed_controls(WheelPacketCommonInput *input) {
    input->controls[0] = input->controls[PACKED_OUTPUT_CONTROL] & 0x0fu;
    input->controls[1] = input->controls[PACKED_OUTPUT_CONTROL] >> 4;
}

/**
 * @brief Applies persistent button latching to a common packet.
 *
 * Records buttons eight and eleven in the packet latch flags while they are pressed and restores
 * either button while its corresponding flag remains set. Profile transitions suspend updates.
 *
 * @param[in,out] input Filtered common input and packet latch flags.
 * @param[in] enabled True when alternative button latching is enabled.
 * @param[in] profile_transition_pending True while a profile transition suppresses latch updates.
 */
void wheel_packet_common_latch_buttons(WheelPacketCommonInput *input, bool enabled,
                                       bool profile_transition_pending) {
    if (!enabled || profile_transition_pending) {
        return;
    }

    if ((input->buttons[1] & FIRST_LATCH_BUTTON) != 0) {
        input->controls[3] |= FIRST_LATCH_FLAG;
    } else if ((input->controls[3] & FIRST_LATCH_FLAG) != 0) {
        input->buttons[1] |= FIRST_LATCH_BUTTON;
    }
    if ((input->buttons[1] & SECOND_LATCH_BUTTON) != 0) {
        input->controls[3] |= SECOND_LATCH_FLAG;
    } else if ((input->controls[3] & SECOND_LATCH_FLAG) != 0) {
        input->buttons[1] |= SECOND_LATCH_BUTTON;
    }
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
