#include "wheel/protocol.h"

#include <stdbool.h>
#include <stdint.h>

#include "wheel/authentication.h"
#include "wheel/packet_mode_one.h"

enum {
    WHEEL_INTERFACE_MODE_VENDOR_04 = 0x04,
    WHEEL_INTERFACE_MODE_VENDOR_06 = 0x06,
    WHEEL_INTERFACE_MODE_ANALOG_AXES = 0x15,
};

/**
 * @brief Calculates the attached-wheel message CRC-8.
 *
 * Starts at 0xFF and applies the reflected 0x8C polynomial to each input byte.
 *
 * @param[in] data First byte covered by the checksum.
 * @param[in] length Number of bytes to process.
 * @return Reflected CRC-8 value using polynomial 0x8C.
 */
static uint8_t crc8(const uint8_t *data, uint8_t length) {
    uint8_t crc = UINT8_MAX;
    while (length-- != 0) {
        crc ^= *data++;
        for (uint8_t bit = 0; bit < 8; bit++) {
            crc = (crc & 1u) != 0 ? (uint8_t)((crc >> 1) ^ 0x8cu) : (uint8_t)(crc >> 1);
        }
    }
    return crc;
}

static void clear(uint8_t *data, uint8_t length) {
    for (uint8_t index = 0; index < length; index++) {
        data[index] = 0;
    }
}

static void build_selection_response(WheelProtocol *protocol) {
    uint8_t flags = protocol->response[WHEEL_PROTOCOL_FLAGS_OFFSET];
    clear(protocol->response, WHEEL_PROTOCOL_PACKET_SIZE);
    protocol->response[0] = WHEEL_PROTOCOL_COMMAND_SELECT_MODE;
    protocol->response[WHEEL_PROTOCOL_CHECKSUM_OFFSET] =
        crc8(protocol->response, WHEEL_PROTOCOL_CONTENT_SIZE);
    protocol->response[WHEEL_PROTOCOL_FLAGS_OFFSET] = flags;
}

static void build_active_response(WheelProtocol *protocol) {
    if (!wheel_packet_mode_one_applies(protocol->mode)) {
        return;
    }
    uint8_t flags = protocol->response[WHEEL_PROTOCOL_FLAGS_OFFSET];
    clear(protocol->response, WHEEL_PROTOCOL_PACKET_SIZE);
    wheel_packet_mode_one_encode(&protocol->mode_one_output, protocol->response);
    protocol->response[WHEEL_PROTOCOL_CHECKSUM_OFFSET] =
        crc8(protocol->response, WHEEL_PROTOCOL_CONTENT_SIZE);
    protocol->response[WHEEL_PROTOCOL_FLAGS_OFFSET] = flags;
}

/**
 * @brief Tests whether an interface mode enables the gated overlay input.
 *
 * Enables the secondary auxiliary input for interface modes 4, 6, and 0x15.
 *
 * @param[in] interface_mode Current wheel input interface mode.
 * @return True when the gated input participates in overlay acknowledgement.
 */
static bool interface_mode_uses_gated_acknowledgement_input(uint8_t interface_mode) {
    return interface_mode == WHEEL_INTERFACE_MODE_VENDOR_04 ||
           interface_mode == WHEEL_INTERFACE_MODE_VENDOR_06 ||
           interface_mode == WHEEL_INTERFACE_MODE_ANALOG_AXES;
}

/**
 * @brief Detects input eligible to acknowledge a display overlay.
 *
 * Accepts any directional or button bit, the first auxiliary byte, and the second auxiliary byte
 * only in interface modes 4, 6, and 0x15.
 *
 * @param[in] protocol Protocol state with the active interface mode.
 * @param[in] input Decoded and button-filtered attached-wheel request.
 * @return True while an eligible input is active.
 */
static bool acknowledgement_input_active(const WheelProtocol *protocol,
                                         const WheelPacketModeOneInput *input) {
    bool button_active = input->buttons[0] != 0 || input->buttons[1] != 0 || input->buttons[2] != 0;
    bool gated_input_active =
        input->controls.x != 0 &&
        interface_mode_uses_gated_acknowledgement_input(protocol->interface_mode);
    return button_active || input->controls.latch_flags != 0 || gated_input_active;
}

/**
 * @brief Captures an active attached-wheel request.
 *
 * Decodes and normalizes the standard request, records display-acknowledgement input, updates the
 * change snapshot, and preserves the report fields consumed outside the normalized input view.
 *
 * @param[in,out] protocol Protocol state that owns the request snapshot and change latch.
 * @param[in] request Complete 57-byte attached-wheel request.
 */
static void capture_request(WheelProtocol *protocol,
                            const uint8_t request[WHEEL_PROTOCOL_PACKET_SIZE]) {
    if (wheel_packet_mode_one_applies(protocol->mode)) {
        uint8_t snapshot[WHEEL_PACKET_MODE_ONE_SNAPSHOT_SIZE];
        wheel_packet_mode_one_decode(request, &protocol->mode_one_input);
        protocol->mode_one_report_state.axis_values[0] = protocol->mode_one_input.axis_values[0];
        protocol->mode_one_report_state.axis_values[1] = protocol->mode_one_input.axis_values[1];
        protocol->mode_one_report_state.report_mode = protocol->mode_one_input.report_mode;
        protocol->mode_one_report_state.report_capabilities =
            protocol->mode_one_input.report_capabilities;
        protocol->mode_one_report_state.axis_limit = protocol->mode_one_input.axis_limit;
        wheel_packet_mode_one_filter_buttons(&protocol->mode_one_button_filter,
                                             &protocol->mode_one_input);
        protocol->acknowledgement_input_active =
            acknowledgement_input_active(protocol, &protocol->mode_one_input);
        if (protocol->mode_one_output.operating_mode == 0x13 ||
            protocol->mode_one_output.operating_mode == 0x14) {
            wheel_packet_mode_one_filter_control_axes(&protocol->mode_one_control_axis_filter,
                                                      &protocol->mode_one_input);
            wheel_axis_override_process(
                &protocol->axis_override_processor, protocol->configured_axis_override_mode,
                protocol->mode_one_output.operating_mode, protocol->interface_mode,
                protocol->mode_one_input.controls.enabled != 0, protocol->axis_calibration_value,
                protocol->mode_one_input.controls.x, protocol->mode_one_input.controls.y,
                protocol->mode_one_input.axis_outputs);
        }
        wheel_packet_mode_one_normalize(&protocol->mode_one_input,
                                        protocol->mode_one_output.operating_mode == 0x13 ||
                                            protocol->mode_one_output.operating_mode == 0x14,
                                        protocol->button_latch_enabled,
                                        protocol->profile_transition_pending, snapshot);
        bool changed = false;
        for (uint8_t index = 0; index < WHEEL_PROTOCOL_SNAPSHOT_SIZE; index++) {
            changed |= protocol->request[index] != snapshot[index];
            protocol->request[index] = snapshot[index];
        }
        protocol->request_changed |= changed;
    }
    protocol->request_ready = true;
}

static void select_mode(WheelProtocol *protocol,
                        const uint8_t request[WHEEL_PROTOCOL_PACKET_SIZE]) {
    switch (request[0]) {
    case WHEEL_PROTOCOL_COMMAND_SCAN_PRIMARY:
        protocol->mode = WHEEL_MODE_SCAN_PRIMARY;
        protocol->phase = WHEEL_PROTOCOL_SCANNING_PRIMARY;
        break;
    case WHEEL_PROTOCOL_COMMAND_SCAN_SECONDARY:
        protocol->mode = WHEEL_MODE_SCAN_SECONDARY;
        protocol->phase = WHEEL_PROTOCOL_SCANNING_SECONDARY;
        break;
    case WHEEL_PROTOCOL_COMMAND_SELECT_MODE:
        if (request[1] > WHEEL_MODE_MAXIMUM) {
            protocol->phase = WHEEL_PROTOCOL_UNSUPPORTED;
            break;
        }
        protocol->mode = request[1];
        if (wheel_authentication_required(protocol->mode_one_output.operating_mode)) {
            wheel_authentication_init(&protocol->authentication,
                                      protocol->mode_one_output.operating_mode);
            protocol->phase = WHEEL_PROTOCOL_AUTHENTICATING;
        } else {
            protocol->phase = WHEEL_PROTOCOL_ACTIVE;
        }
        break;
    default:
        return;
    }
    build_selection_response(protocol);
}

/**
 * @brief Checks a command byte for the active attached-wheel exchange.
 *
 * Selects the authenticated or standard command set from the current operating mode.
 *
 * @param[in] protocol Protocol state with the active operating mode.
 * @param[in] command Received command byte.
 * @return True for A6 or A7 in authenticated modes, or A5 in other modes.
 */
static bool active_command_valid(const WheelProtocol *protocol, uint8_t command) {
    if (wheel_authentication_required(protocol->mode_one_output.operating_mode)) {
        return command == WHEEL_PROTOCOL_COMMAND_AUTHENTICATE ||
               command == WHEEL_PROTOCOL_COMMAND_AUTHENTICATE_REPLY;
    }
    return command == WHEEL_PROTOCOL_COMMAND_SELECT_MODE;
}

void wheel_protocol_init(WheelProtocol *protocol) {
    const WheelPacketModeOneInput empty_input = {0};
    const WheelPacketModeOneReportState empty_report_state = {0};
    const WheelPacketModeOneOutput empty_output = {0};
    clear(protocol->response, WHEEL_PROTOCOL_PACKET_SIZE);
    clear(protocol->request, WHEEL_PROTOCOL_SNAPSHOT_SIZE);
    wheel_axis_override_processor_init(&protocol->axis_override_processor);
    wheel_packet_mode_one_button_filter_init(&protocol->mode_one_button_filter);
    wheel_packet_mode_one_control_axis_filter_init(&protocol->mode_one_control_axis_filter);
    protocol->mode_one_input = empty_input;
    protocol->mode_one_report_state = empty_report_state;
    protocol->mode_one_output = empty_output;
    wheel_authentication_init(&protocol->authentication, WHEEL_MODE_UNKNOWN);
    protocol->phase = WHEEL_PROTOCOL_WAITING;
    protocol->mode = WHEEL_MODE_UNKNOWN;
    protocol->interface_mode = 0;
    protocol->configured_axis_override_mode = WHEEL_AXIS_OVERRIDE_MODE_NONE;
    protocol->axis_calibration_value = 0;
    protocol->button_latch_enabled = false;
    protocol->profile_transition_pending = false;
    protocol->request_ready = false;
    protocol->request_changed = false;
    protocol->acknowledgement_input_active = false;
}

void wheel_protocol_set_mode_one_output(WheelProtocol *protocol,
                                        const WheelPacketModeOneOutput *output) {
    protocol->mode_one_output = *output;
}

void wheel_protocol_set_axis_processing(WheelProtocol *protocol, uint8_t interface_mode,
                                        uint8_t override_mode, uint8_t calibration_value) {
    protocol->interface_mode = interface_mode;
    protocol->configured_axis_override_mode = override_mode;
    protocol->axis_calibration_value = calibration_value;
}

void wheel_protocol_set_button_latch(WheelProtocol *protocol, bool enabled,
                                     bool profile_transition_pending) {
    protocol->button_latch_enabled = enabled;
    protocol->profile_transition_pending = profile_transition_pending;
}

void wheel_protocol_accept(WheelProtocol *protocol,
                           const uint8_t request[WHEEL_PROTOCOL_PACKET_SIZE]) {
    bool ready = (request[WHEEL_PROTOCOL_FLAGS_OFFSET] & WHEEL_PROTOCOL_REQUEST_READY) != 0;

    switch (protocol->phase) {
    case WHEEL_PROTOCOL_WAITING:
        if (ready) {
            protocol->phase = WHEEL_PROTOCOL_SYNCHRONIZING;
        }
        return;
    case WHEEL_PROTOCOL_SYNCHRONIZING:
        if (!ready) {
            protocol->phase = WHEEL_PROTOCOL_WAITING;
            return;
        }
        protocol->response[WHEEL_PROTOCOL_FLAGS_OFFSET] |= WHEEL_PROTOCOL_RESPONSE_ACKNOWLEDGED;
        protocol->phase = WHEEL_PROTOCOL_ACKNOWLEDGING;
        return;
    case WHEEL_PROTOCOL_ACKNOWLEDGING:
        protocol->phase = ready ? WHEEL_PROTOCOL_SELECTING : WHEEL_PROTOCOL_WAITING;
        return;
    case WHEEL_PROTOCOL_SELECTING:
        if (!ready) {
            protocol->phase = WHEEL_PROTOCOL_WAITING;
            return;
        }
        select_mode(protocol, request);
        return;
    case WHEEL_PROTOCOL_AUTHENTICATING:
        if (ready && wheel_authentication_accept(&protocol->authentication, request,
                                                 wheel_protocol_message_valid(request),
                                                 protocol->response)) {
            protocol->phase = WHEEL_PROTOCOL_ACTIVE;
        }
        if (ready) {
            protocol->response[WHEEL_PROTOCOL_CHECKSUM_OFFSET] =
                crc8(protocol->response, WHEEL_PROTOCOL_CONTENT_SIZE);
        }
        return;
    case WHEEL_PROTOCOL_ACTIVE:
        if (!ready) {
            return;
        }
        if (wheel_protocol_message_valid(request)) {
            if (active_command_valid(protocol, request[0])) {
                capture_request(protocol, request);
            } else if (wheel_authentication_required(protocol->mode_one_output.operating_mode)) {
                wheel_authentication_init(&protocol->authentication,
                                          protocol->mode_one_output.operating_mode);
                protocol->phase = WHEEL_PROTOCOL_AUTHENTICATING;
            }
        }
        build_active_response(protocol);
        return;
    case WHEEL_PROTOCOL_UNSUPPORTED:
    case WHEEL_PROTOCOL_SCANNING_PRIMARY:
    case WHEEL_PROTOCOL_SCANNING_SECONDARY:
        return;
    }
}

const uint8_t *wheel_protocol_response(const WheelProtocol *protocol) { return protocol->response; }

const uint8_t *wheel_protocol_request(const WheelProtocol *protocol) {
    return protocol->request_ready ? protocol->request : 0;
}

const WheelPacketModeOneInput *wheel_protocol_mode_one_input(const WheelProtocol *protocol) {
    return protocol->request_ready && wheel_packet_mode_one_applies(protocol->mode)
               ? &protocol->mode_one_input
               : 0;
}

const WheelPacketModeOneReportState *
wheel_protocol_mode_one_report_state(const WheelProtocol *protocol) {
    return protocol->request_ready && wheel_packet_mode_one_applies(protocol->mode)
               ? &protocol->mode_one_report_state
               : 0;
}

const WheelAxisOverrideProcessor *wheel_protocol_axis_overrides(const WheelProtocol *protocol) {
    return &protocol->axis_override_processor;
}

bool wheel_protocol_request_changed(WheelProtocol *protocol) {
    bool changed = protocol->request_changed;
    protocol->request_changed = false;
    return changed;
}

/**
 * @brief Reports display-acknowledgement input from the current wheel packet.
 *
 * Returns the directional, button, auxiliary, and interface-gated input state captured before
 * request normalization clears transient fields.
 *
 * @param[in] protocol Attached-wheel protocol state.
 * @return True while an eligible input is active.
 */
bool wheel_protocol_acknowledgement_input_active(const WheelProtocol *protocol) {
    return protocol->request_ready && protocol->acknowledgement_input_active;
}

uint8_t wheel_protocol_message_checksum(const uint8_t packet[WHEEL_PROTOCOL_PACKET_SIZE]) {
    return crc8(packet, WHEEL_PROTOCOL_CONTENT_SIZE);
}

bool wheel_protocol_message_valid(const uint8_t packet[WHEEL_PROTOCOL_PACKET_SIZE]) {
    return packet[WHEEL_PROTOCOL_CHECKSUM_OFFSET] == wheel_protocol_message_checksum(packet);
}
