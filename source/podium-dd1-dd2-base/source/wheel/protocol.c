#include "wheel/protocol.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "wheel/authentication.h"
#include "wheel/capability.h"
#include "wheel/output_reports.h"
#include "wheel/packet_crc.h"
#include "wheel/packet_mode_four.h"
#include "wheel/packet_mode_one.h"
#include "wheel/packet_remote_tuning.h"

enum {
    WHEEL_STATUS_SELECT_PREFIX_MODE = 9,
    WHEEL_STATUS_RESPONSE_CODE = 0x82,
    WHEEL_STATUS_IDLE = 0x1e,
    WHEEL_STATUS_SETUP_PAGE_OFFSET = 0x1f,
    WHEEL_SETUP_PAGE_MAXIMUM = 5,
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

/**
 * @brief Clears a wheel protocol byte range.
 *
 * Writes zero to each byte in the selected range.
 *
 * @param[out] data First byte to clear.
 * @param[in] length Number of bytes to clear.
 */
static void clear(uint8_t *data, uint8_t length) {
    for (uint8_t index = 0; index < length; index++) {
        data[index] = 0;
    }
}

/**
 * @brief Adds the legacy wheel-status fields used for display rotation.
 *
 * Mode 0x0E receives the signed angle in bytes 9 and 10 when profile rotation is enabled, followed
 * by state code 6, a clear adapter flag, and the idle pedal-status pair.
 *
 * @param[in] protocol Active attached-wheel protocol state.
 * @param[in,out] response Cleared response receiving the legacy status fields.
 */
static void encode_legacy_status(const WheelProtocol *protocol, uint8_t *response) {
    if (protocol->mode != WHEEL_MODE_REMOTE_TUNING_LEGACY) {
        return;
    }
    if (protocol->display_rotation_enabled) {
        response[9] = (uint8_t)protocol->display_rotation_angle;
        response[10] = (uint8_t)((uint16_t)protocol->display_rotation_angle >> 8);
    }
    response[12] = 6;
    response[13] = 0;
    response[14] = 0;
    response[15] = 0;
}

/**
 * @brief Builds the wheel-mode selection acknowledgement.
 *
 * Clears the response, writes command A5 and its checksum, and preserves the transport flag byte.
 *
 * @param[in,out] protocol Wheel protocol state and response storage.
 */
static void build_selection_response(WheelProtocol *protocol) {
    uint8_t flags = protocol->response[WHEEL_PROTOCOL_FLAGS_OFFSET];
    clear(protocol->response, WHEEL_PROTOCOL_PACKET_SIZE);
    protocol->response[0] = WHEEL_PROTOCOL_COMMAND_SELECT_MODE;
    protocol->response[WHEEL_PROTOCOL_CHECKSUM_OFFSET] =
        crc8(protocol->response, WHEEL_PROTOCOL_CONTENT_SIZE);
    protocol->response[WHEEL_PROTOCOL_FLAGS_OFFSET] = flags;
}

/**
 * @brief Builds the next active attached-wheel response.
 *
 * Consumes a pending system status before a system-owned control response, remote-tuning work, and
 * host output reports. A setup-page response schedules its corresponding status for the following
 * exchange. Otherwise, encodes the selected packet family or a blank A6 remote-tuning frame and
 * overlays the highest-priority host output report. The checksum is updated while transport
 * acknowledgement flags are preserved.
 *
 * @param[in,out] protocol Active protocol state and response storage.
 */
static void build_active_response(WheelProtocol *protocol) {
    bool remote_tuning_mode = protocol->mode == WHEEL_MODE_REMOTE_TUNING_LEGACY ||
                              protocol->mode == WHEEL_MODE_REMOTE_TUNING_EXTENDED;
    if (!wheel_packet_mode_one_applies(protocol->mode) && protocol->mode != 4 &&
        !wheel_packet_crc_applies(protocol->mode) && !remote_tuning_mode) {
        return;
    }
    bool system_status_response = protocol->system_status_pending;
    bool system_control_response =
        !system_status_response && remote_tuning_mode &&
        wheel_packet_remote_tuning_pending(&protocol->system_control_output) &&
        ((protocol->mode == WHEEL_MODE_REMOTE_TUNING_LEGACY &&
          protocol->system_control_output.response.link == REMOTE_TUNING_LINK_LEGACY) ||
         (protocol->mode == WHEEL_MODE_REMOTE_TUNING_EXTENDED &&
          protocol->system_control_output.response.link == REMOTE_TUNING_LINK_EXTENDED));
    bool remote_tuning_response =
        !system_status_response && !system_control_response && remote_tuning_mode &&
        wheel_packet_remote_tuning_pending(&protocol->remote_tuning_output);
    uint8_t flags = protocol->response[WHEEL_PROTOCOL_FLAGS_OFFSET];
    clear(protocol->response, WHEEL_PROTOCOL_PACKET_SIZE);
    if (system_control_response) {
        RemoteTuningResponse response = protocol->system_control_output.response;
        (void)wheel_packet_remote_tuning_encode(&protocol->system_control_output,
                                                protocol->response);
        if (response.code == REMOTE_TUNING_RESPONSE_SETUP) {
            protocol->system_status_code =
                response.value == 0 ? WHEEL_STATUS_IDLE
                                    : (uint8_t)(response.value + WHEEL_STATUS_SETUP_PAGE_OFFSET);
            protocol->system_status_pending = true;
        }
    } else if (remote_tuning_response) {
        (void)wheel_packet_remote_tuning_encode(&protocol->remote_tuning_output,
                                                protocol->response);
    } else if (wheel_packet_crc_applies(protocol->mode)) {
        wheel_packet_crc_encode(protocol->mode, protocol->host_capability_enabled,
                                &protocol->crc_output, protocol->response);
    } else if (protocol->mode == 4) {
        wheel_packet_mode_four_encode(&protocol->mode_four_output, protocol->response);
    } else if (!remote_tuning_mode) {
        wheel_packet_mode_one_encode(protocol->mode, &protocol->mode_one_output,
                                     protocol->response);
    } else {
        protocol->response[0] = WHEEL_PROTOCOL_COMMAND_AUTHENTICATE;
    }
    if (!system_control_response && !remote_tuning_response) {
        encode_legacy_status(protocol, protocol->response);
        if (!system_status_response) {
            (void)wheel_output_reports_encode_next(&protocol->output_reports, protocol->mode,
                                                   protocol->response);
        }
    }
    if (system_status_response) {
        protocol->response[0] = protocol->mode == WHEEL_STATUS_SELECT_PREFIX_MODE
                                    ? WHEEL_PROTOCOL_COMMAND_SELECT_MODE
                                    : WHEEL_PROTOCOL_COMMAND_AUTHENTICATE;
        protocol->response[1] = WHEEL_STATUS_RESPONSE_CODE;
        protocol->response[2] = protocol->system_status_code;
        protocol->system_status_pending = false;
    }
    protocol->response[WHEEL_PROTOCOL_CHECKSUM_OFFSET] =
        crc8(protocol->response, WHEEL_PROTOCOL_CONTENT_SIZE);
    protocol->response[WHEEL_PROTOCOL_FLAGS_OFFSET] = flags;
}

/**
 * @brief Selects the attached-wheel display rotation output.
 *
 * Retains the active profile flag and current signed angle for the next legacy remote-tuning
 * response.
 *
 * @param[in,out] protocol Attached-wheel protocol state.
 * @param[in] enabled True to include the display angle.
 * @param[in] angle Signed angle in hundredths of a degree.
 */
void wheel_protocol_set_display_rotation(WheelProtocol *protocol, bool enabled, int16_t angle) {
    protocol->display_rotation_enabled = enabled;
    protocol->display_rotation_angle = angle;
}

/**
 * @brief Queues an attached-wheel status response.
 *
 * Retains the low status byte for the next active wheel exchange. A newer code replaces an older
 * pending code, matching the shared system-status owner.
 *
 * @param[in,out] protocol Attached-wheel protocol state to update.
 * @param[in] code System status code to publish.
 */
void wheel_protocol_queue_system_status(WheelProtocol *protocol, uint16_t code) {
    protocol->system_status_code = (uint8_t)code;
    protocol->system_status_pending = true;
}

/**
 * @brief Queues a system-owned remote-tuning response.
 *
 * Accepts active, inactive, and setup-page responses for a legacy or extended link. The separate
 * system slot takes priority without consuming a host-owned response that is already pending.
 *
 * @param[in,out] protocol Attached-wheel protocol state to update.
 * @param[in] response Semantic response requested by system-control policy.
 * @return True when the response was retained; otherwise false.
 */
bool wheel_protocol_queue_system_control_response(WheelProtocol *protocol,
                                                  const RemoteTuningResponse *response) {
    if (protocol == NULL || response == NULL ||
        (response->link != REMOTE_TUNING_LINK_LEGACY &&
         response->link != REMOTE_TUNING_LINK_EXTENDED) ||
        (response->code != REMOTE_TUNING_RESPONSE_ACTIVE &&
         response->code != REMOTE_TUNING_RESPONSE_INACTIVE &&
         response->code != REMOTE_TUNING_RESPONSE_SETUP) ||
        (response->code == REMOTE_TUNING_RESPONSE_SETUP &&
         response->value > WHEEL_SETUP_PAGE_MAXIMUM)) {
        return false;
    }
    return wheel_packet_remote_tuning_queue(&protocol->system_control_output, response);
}

/**
 * @brief Detects input eligible to acknowledge a display overlay.
 *
 * Accepts any directional or button bit and the first auxiliary byte from the standard packet
 * codec.
 *
 * @param[in] input Decoded and button-filtered attached-wheel request.
 * @return True while an eligible input is active.
 */
static bool mode_one_acknowledgement_input_active(const WheelPacketModeOneInput *input) {
    bool button_active = input->buttons[0] != 0 || input->buttons[1] != 0 || input->buttons[2] != 0;
    return button_active || input->controls.latch_flags != 0;
}

/**
 * @brief Detects mode-4 input eligible to acknowledge a display overlay.
 *
 * Accepts any filtered directional or button bit and payload byte 10, which is enabled for mode 4
 * by the overlay input gate.
 *
 * @param[in] input Filtered and normalized mode-4 request.
 * @return True while an eligible input is active.
 */
static bool mode_four_acknowledgement_input_active(const WheelPacketModeFourInput *input) {
    bool button_active = input->buttons[0] != 0 || input->buttons[1] != 0 || input->buttons[2] != 0;
    return button_active || input->control_data[0] != 0;
}

/**
 * @brief Detects CRC-family input eligible to acknowledge a display overlay.
 *
 * Accepts any filtered directional or button bit and payload byte 10, which is enabled for modes
 * 6 and 0x15 by the overlay input gate.
 *
 * @param[in] input Filtered and normalized CRC-family request.
 * @return True while an eligible input is active.
 */
static bool crc_acknowledgement_input_active(const WheelPacketCrcInput *input) {
    bool button_active = input->buttons[0] != 0 || input->buttons[1] != 0 || input->buttons[2] != 0;
    return button_active || input->controls[4] != 0;
}

/**
 * @brief Captures an active attached-wheel request.
 *
 * Decodes and normalizes the selected mode's request, records display-acknowledgement input,
 * latches attached-wheel input capability, updates the change snapshot, and preserves separately
 * consumed report fields.
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
        wheel_capability_update(&protocol->capabilities, protocol->mode,
                                protocol->mode_one_input.report_mode,
                                protocol->mode_one_input.report_capabilities);
        protocol->capabilities.input_available |=
            protocol->mode_one_input.controls.enabled != 0 ||
            protocol->mode_one_input.controls.latch_flags != 0;
        wheel_packet_mode_one_filter_buttons(&protocol->mode_one_button_filter,
                                             &protocol->mode_one_input);
        protocol->acknowledgement_input_active =
            mode_one_acknowledgement_input_active(&protocol->mode_one_input);
        if (protocol->mode == 0x13 || protocol->mode == 0x14) {
            wheel_packet_mode_one_filter_control_axes(&protocol->mode_one_control_axis_filter,
                                                      &protocol->mode_one_input);
            wheel_axis_override_process(
                &protocol->axis_override_processor, protocol->configured_axis_override_mode,
                protocol->mode, protocol->interface_mode,
                protocol->mode_one_input.controls.enabled != 0, protocol->now_ms,
                &protocol->paddle_bite_point_percent, &protocol->mode_one_input.buttons[0],
                &protocol->mode_one_input.motion, protocol->mode_one_input.controls.x,
                protocol->mode_one_input.controls.y, protocol->mode_one_input.axis_outputs);
        }
        wheel_motion_accumulate_primary(&protocol->motion, protocol->mode_one_input.motion);
        wheel_packet_mode_one_normalize(
            &protocol->mode_one_input, protocol->mode == 0x13 || protocol->mode == 0x14,
            protocol->button_latch_enabled, protocol->profile_transition_pending, snapshot);
        bool changed = false;
        for (uint8_t index = 0; index < WHEEL_PROTOCOL_SNAPSHOT_SIZE; index++) {
            changed |= protocol->request[index] != snapshot[index];
            protocol->request[index] = snapshot[index];
        }
        protocol->request_changed |= changed;
    } else if (protocol->mode == 4) {
        uint8_t snapshot[WHEEL_PACKET_MODE_FOUR_SNAPSHOT_SIZE];
        wheel_packet_mode_four_decode(request, &protocol->mode_four_input);
        wheel_motion_accumulate_primary(&protocol->motion, protocol->mode_four_input.motion);
        wheel_capability_update(&protocol->capabilities, protocol->mode,
                                protocol->mode_four_input.report_mode,
                                protocol->mode_four_input.report_capabilities);
        protocol->capabilities.input_available |=
            protocol->mode_four_input.controls[2] != 0 ||
            protocol->mode_four_input.controls[3] != 0 ||
            protocol->mode_four_input.mode_buttons != 0 ||
            protocol->mode_four_input.axis_report_enabled != 0;
        wheel_packet_mode_four_filter(&protocol->mode_four_filter, &protocol->mode_four_input);
        wheel_packet_mode_four_normalize(&protocol->mode_four_input, protocol->interface_mode,
                                         &protocol->mode_four_runtime, snapshot);
        protocol->acknowledgement_input_active =
            mode_four_acknowledgement_input_active(&protocol->mode_four_input);
        bool changed = false;
        for (uint8_t index = 0; index < WHEEL_PROTOCOL_SNAPSHOT_SIZE; index++) {
            changed |= protocol->request[index] != snapshot[index];
            protocol->request[index] = snapshot[index];
        }
        protocol->request_changed |= changed;
    } else if (wheel_packet_crc_applies(protocol->mode)) {
        uint8_t snapshot[WHEEL_PACKET_CRC_SNAPSHOT_SIZE];
        wheel_packet_crc_decode(request, &protocol->crc_input);
        wheel_capability_update(&protocol->capabilities, protocol->mode,
                                protocol->crc_input.report_mode,
                                protocol->crc_input.report_capabilities);
        if (protocol->mode == WHEEL_MODE_CRC_AUTHENTICATED &&
            (!protocol->crc_adapter.connected || protocol->crc_adapter.mode != 1)) {
            protocol->capabilities.input_available = false;
        }
        protocol->capabilities.input_available |=
            protocol->crc_input.controls[2] != 0 || protocol->crc_input.controls[3] != 0 ||
            (protocol->crc_adapter.connected && protocol->crc_adapter.mode == 1);
        wheel_packet_crc_prepare(&protocol->crc_input, protocol->mode, protocol->interface_mode);
        wheel_packet_crc_filter(&protocol->crc_filter, &protocol->crc_input);
        wheel_packet_crc_normalize(&protocol->crc_input, protocol->mode, protocol->interface_mode,
                                   &protocol->crc_adapter);
        wheel_axis_override_process_packet(
            &protocol->axis_override_processor, protocol->configured_axis_override_mode,
            protocol->mode, protocol->interface_mode, protocol->crc_input.axis_limit,
            protocol->now_ms, &protocol->paddle_bite_point_percent, &protocol->crc_input.buttons[0],
            &protocol->crc_input.motion, protocol->crc_input.controls,
            protocol->crc_input.axis_outputs);
        wheel_motion_accumulate_primary(&protocol->motion, protocol->crc_input.motion);
        wheel_packet_crc_smooth_axes(&protocol->crc_filter, &protocol->crc_input);
        wheel_packet_crc_snapshot(&protocol->crc_input, snapshot);
        protocol->acknowledgement_input_active =
            crc_acknowledgement_input_active(&protocol->crc_input);
        bool changed = false;
        for (uint8_t index = 0; index < WHEEL_PROTOCOL_SNAPSHOT_SIZE; index++) {
            changed |= protocol->request[index] != snapshot[index];
            protocol->request[index] = snapshot[index];
        }
        protocol->request_changed |= changed;
    }
    protocol->request_ready = true;
}

/**
 * @brief Selects the attached-wheel packet mode.
 *
 * Recognizes the two scan commands and command A5. Supported A5 modes enter authentication or
 * active traffic, unsupported mode values enter the unsupported phase, and recognized selections
 * produce an acknowledgement response.
 *
 * @param[in,out] protocol Wheel protocol state to update.
 * @param[in] request Complete attached-wheel selection request.
 */
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
        if (wheel_authentication_required(protocol->mode)) {
            wheel_authentication_init(&protocol->authentication, protocol->mode);
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
    if (wheel_authentication_required(protocol->mode)) {
        return command == WHEEL_PROTOCOL_COMMAND_AUTHENTICATE ||
               command == WHEEL_PROTOCOL_COMMAND_AUTHENTICATE_REPLY;
    }
    return command == WHEEL_PROTOCOL_COMMAND_SELECT_MODE;
}

/**
 * @brief Initializes the attached-wheel protocol.
 *
 * Clears packet state, initializes every packet-family processor, and starts the handshake in the
 * waiting phase with an unknown mode.
 *
 * @param[out] protocol Wheel protocol state to initialize.
 */
void wheel_protocol_init(WheelProtocol *protocol) {
    const WheelPacketModeOneInput empty_input = {0};
    const WheelPacketModeOneReportState empty_report_state = {0};
    const WheelPacketModeOneOutput empty_output = {0};
    const WheelPacketModeFourInput empty_mode_four_input = {0};
    const WheelPacketModeFourRuntime empty_mode_four_runtime = {0};
    const WheelPacketModeFourOutput empty_mode_four_output = {0};
    const WheelPacketCrcInput empty_crc_input = {0};
    const WheelPacketCrcOutput empty_crc_output = {0};
    const WheelPacketCrcAdapter empty_crc_adapter = {0};
    clear(protocol->response, WHEEL_PROTOCOL_PACKET_SIZE);
    clear(protocol->request, WHEEL_PROTOCOL_SNAPSHOT_SIZE);
    wheel_axis_override_processor_init(&protocol->axis_override_processor);
    wheel_motion_init(&protocol->motion);
    wheel_packet_mode_one_button_filter_init(&protocol->mode_one_button_filter);
    wheel_packet_mode_one_control_axis_filter_init(&protocol->mode_one_control_axis_filter);
    protocol->mode_one_input = empty_input;
    protocol->mode_one_report_state = empty_report_state;
    protocol->mode_one_output = empty_output;
    wheel_packet_mode_four_filter_init(&protocol->mode_four_filter);
    protocol->mode_four_input = empty_mode_four_input;
    protocol->mode_four_runtime = empty_mode_four_runtime;
    protocol->mode_four_output = empty_mode_four_output;
    wheel_packet_crc_filter_init(&protocol->crc_filter);
    protocol->crc_input = empty_crc_input;
    protocol->crc_output = empty_crc_output;
    protocol->crc_adapter = empty_crc_adapter;
    wheel_packet_remote_tuning_init(&protocol->system_control_output);
    wheel_packet_remote_tuning_init(&protocol->remote_tuning_output);
    wheel_output_reports_init(&protocol->output_reports);
    wheel_capability_init(&protocol->capabilities);
    wheel_authentication_init(&protocol->authentication, WHEEL_MODE_UNKNOWN);
    protocol->phase = WHEEL_PROTOCOL_WAITING;
    protocol->now_ms = 0;
    protocol->mode = WHEEL_MODE_UNKNOWN;
    protocol->interface_mode = 0;
    protocol->configured_axis_override_mode = WHEEL_AXIS_OVERRIDE_MODE_NONE;
    protocol->paddle_bite_point_percent = 100;
    protocol->system_status_code = 0;
    protocol->button_latch_enabled = false;
    protocol->host_capability_enabled = false;
    protocol->profile_transition_pending = false;
    protocol->system_status_pending = false;
    protocol->request_ready = false;
    protocol->request_changed = false;
    protocol->acknowledgement_input_active = false;
}

/**
 * @brief Updates standard packet-family wheel output.
 *
 * Replaces the display, display-state, and link-status output encoded for mode-one packets.
 *
 * @param[in,out] protocol Wheel protocol state to update.
 * @param[in] output Standard packet-family output state.
 */
void wheel_protocol_set_mode_one_output(WheelProtocol *protocol,
                                        const WheelPacketModeOneOutput *output) {
    protocol->mode_one_output = *output;
}

/**
 * @brief Updates mode-four wheel output.
 *
 * Replaces the display, display-state, and legacy-axis output encoded for mode-four packets.
 *
 * @param[in,out] protocol Wheel protocol state to update.
 * @param[in] output Mode-four output state.
 */
void wheel_protocol_set_mode_four_output(WheelProtocol *protocol,
                                         const WheelPacketModeFourOutput *output) {
    protocol->mode_four_output = *output;
}

/**
 * @brief Updates CRC-family wheel output.
 *
 * Replaces the display, motor-link restart, and report-status output encoded for CRC-family
 * packets.
 *
 * @param[in,out] protocol Wheel protocol state to update.
 * @param[in] output CRC-family output state.
 */
void wheel_protocol_set_crc_output(WheelProtocol *protocol, const WheelPacketCrcOutput *output) {
    protocol->crc_output = *output;
}

/**
 * @brief Selects the host-controlled attached-wheel capability.
 *
 * Retains the capability for CRC-family response byte seven and mirrors it in transport flag
 * 0x40 for every subsequent attached-wheel exchange.
 *
 * @param[in,out] protocol Attached-wheel protocol state and response storage.
 * @param[in] enabled True to advertise the host-controlled capability.
 */
void wheel_protocol_set_host_capability(WheelProtocol *protocol, bool enabled) {
    protocol->host_capability_enabled = enabled;
    if (enabled) {
        protocol->response[WHEEL_PROTOCOL_FLAGS_OFFSET] |= WHEEL_PROTOCOL_HOST_CAPABILITY;
    } else {
        protocol->response[WHEEL_PROTOCOL_FLAGS_OFFSET] &= (uint8_t)~WHEEL_PROTOCOL_HOST_CAPABILITY;
    }
}

/**
 * @brief Configures CRC-family adapter packet handling.
 *
 * Retains adapter buttons, axes, rotary positions, mode, connection state, and pending motion used
 * by attached-wheel input processing.
 *
 * @param[in,out] protocol Wheel protocol state to configure.
 * @param[in] adapter CRC-family adapter configuration.
 */
void wheel_protocol_set_crc_adapter(WheelProtocol *protocol, const WheelPacketCrcAdapter *adapter) {
    protocol->crc_adapter = *adapter;
}

/**
 * @brief Queues a remote-tuning response for the negotiated attached-wheel link.
 *
 * Accepts a supported response only when its legacy or extended link matches wheel mode 0x0E or
 * 0x1C respectively.
 *
 * @param[in,out] protocol Wheel protocol that owns the remote-tuning output queue.
 * @param[in] response Remote-tuning link, response code, and value.
 * @return True when the response was queued for the active wheel mode.
 */
bool wheel_protocol_queue_remote_tuning_response(WheelProtocol *protocol,
                                                 const RemoteTuningResponse *response) {
    if (protocol == NULL || response == NULL) {
        return false;
    }
    bool matching_link = (protocol->mode == WHEEL_MODE_REMOTE_TUNING_LEGACY &&
                          response->link == REMOTE_TUNING_LINK_LEGACY) ||
                         (protocol->mode == WHEEL_MODE_REMOTE_TUNING_EXTENDED &&
                          response->link == REMOTE_TUNING_LINK_EXTENDED);
    return matching_link &&
           wheel_packet_remote_tuning_queue(&protocol->remote_tuning_output, response);
}

/**
 * @brief Reports whether a remote-tuning response awaits attached-wheel transfer.
 *
 * Tests the shared remote-tuning output without consuming its response.
 *
 * @param[in] protocol Attached-wheel protocol state.
 * @return True when a supported response is pending.
 */
bool wheel_protocol_remote_tuning_response_pending(const WheelProtocol *protocol) {
    return protocol != NULL && wheel_packet_remote_tuning_pending(&protocol->remote_tuning_output);
}

/**
 * @brief Configures attached-wheel axis processing.
 *
 * Retains the host interface mode, analog-paddle mode, and bite-point percentage applied to
 * incoming mode-one and CRC-family controls.
 *
 * @param[in,out] protocol Wheel protocol state to configure.
 * @param[in] interface_mode Active host interface mode.
 * @param[in] override_mode Configured analog-paddle mode.
 * @param[in] bite_point_percent Active profile bite-point percentage.
 * @param[in] now_ms Current monotonic time in milliseconds.
 */
void wheel_protocol_set_axis_processing(WheelProtocol *protocol, uint8_t interface_mode,
                                        uint8_t override_mode, uint8_t bite_point_percent,
                                        uint32_t now_ms) {
    protocol->now_ms = now_ms;
    protocol->interface_mode = interface_mode;
    protocol->configured_axis_override_mode = override_mode;
    if (protocol->axis_override_processor.paddle_clutch_phase != WHEEL_PADDLE_CLUTCH_ADJUSTING) {
        protocol->paddle_bite_point_percent = bite_point_percent;
    }
}

/**
 * @brief Takes a completed attached-wheel bite-point adjustment.
 *
 * Forwards the adjusted percentage once after the wheel protocol exits its adjustment phase.
 *
 * @param[in,out] protocol Wheel protocol and analog-paddle state.
 * @param[out] updated_percent Completed percentage to persist.
 * @return True when a completed adjustment was available.
 */
bool wheel_protocol_take_bite_point(WheelProtocol *protocol, uint8_t *updated_percent) {
    return wheel_axis_override_take_bite_point(
        &protocol->axis_override_processor, protocol->paddle_bite_point_percent, updated_percent);
}

/**
 * @brief Takes an attached-wheel bite-point report update.
 *
 * Forwards each accepted percentage change once for presentation in the primary input report.
 *
 * @param[in,out] protocol Wheel protocol and analog-paddle state.
 * @param[out] updated_percent Percentage to publish in the next input report.
 * @return True when a new percentage was available.
 */
bool wheel_protocol_take_bite_point_report(WheelProtocol *protocol, uint8_t *updated_percent) {
    return wheel_axis_override_take_bite_point_report(
        &protocol->axis_override_processor, protocol->paddle_bite_point_percent, updated_percent);
}

/**
 * @brief Configures standard-packet button latching.
 *
 * Retains the button-latch enable and profile-transition state used while normalizing mode-one
 * input.
 *
 * @param[in,out] protocol Wheel protocol state to configure.
 * @param[in] enabled Enables standard-packet button latching.
 * @param[in] profile_transition_pending Suppresses latching during a profile transition.
 */
void wheel_protocol_set_button_latch(WheelProtocol *protocol, bool enabled,
                                     bool profile_transition_pending) {
    protocol->button_latch_enabled = enabled;
    protocol->profile_transition_pending = profile_transition_pending;
}

/**
 * @brief Applies one attached-wheel protocol request.
 *
 * Advances the ready-and-acknowledge handshake, selects or authenticates the requested packet
 * family, captures valid active input, and builds the corresponding response. Scan modes remain
 * under the separate scan service.
 *
 * @param[in,out] protocol Wheel protocol state and response storage.
 * @param[in] request Complete 57-byte attached-wheel request.
 */
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
            } else if (wheel_authentication_required(protocol->mode)) {
                wheel_authentication_init(&protocol->authentication, protocol->mode);
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

/**
 * @brief Returns the current attached-wheel response.
 *
 * Exposes the complete response packet prepared by the handshake or active packet encoder.
 *
 * @param[in] protocol Wheel protocol state.
 * @return Current 57-byte attached-wheel response.
 */
const uint8_t *wheel_protocol_response(const WheelProtocol *protocol) { return protocol->response; }

/**
 * @brief Returns the normalized attached-wheel request snapshot.
 *
 * Exposes the current 30-byte input snapshot after a supported active request has been captured.
 *
 * @param[in] protocol Wheel protocol state.
 * @return Current request snapshot, or null before supported input is ready.
 */
const uint8_t *wheel_protocol_request(const WheelProtocol *protocol) {
    return protocol->request_ready ? protocol->request : 0;
}

/**
 * @brief Returns the current standard packet-family input.
 *
 * Exposes decoded mode-one input only after a supported standard request has been captured.
 *
 * @param[in] protocol Wheel protocol state.
 * @return Current standard input, or null when unavailable.
 */
const WheelPacketModeOneInput *wheel_protocol_mode_one_input(const WheelProtocol *protocol) {
    return protocol->request_ready && wheel_packet_mode_one_applies(protocol->mode)
               ? &protocol->mode_one_input
               : 0;
}

/**
 * @brief Returns the current mode-four input.
 *
 * Exposes decoded mode-four input only after a mode-four request has been captured.
 *
 * @param[in] protocol Wheel protocol state.
 * @return Current mode-four input, or null when unavailable.
 */
const WheelPacketModeFourInput *wheel_protocol_mode_four_input(const WheelProtocol *protocol) {
    return protocol->request_ready && protocol->mode == 4 ? &protocol->mode_four_input : 0;
}

/**
 * @brief Returns the current CRC-family input.
 *
 * Exposes decoded CRC-family input only after a supported CRC request has been captured.
 *
 * @param[in] protocol Wheel protocol state.
 * @return Current CRC-family input, or null when unavailable.
 */
const WheelPacketCrcInput *wheel_protocol_crc_input(const WheelProtocol *protocol) {
    return protocol->request_ready && wheel_packet_crc_applies(protocol->mode)
               ? &protocol->crc_input
               : 0;
}

/**
 * @brief Returns separately retained standard report fields.
 *
 * Exposes axis, report-mode, capability, and limit fields retained before standard input
 * normalization.
 *
 * @param[in] protocol Wheel protocol state.
 * @return Current standard report state, or null when unavailable.
 */
const WheelPacketModeOneReportState *
wheel_protocol_mode_one_report_state(const WheelProtocol *protocol) {
    return protocol->request_ready && wheel_packet_mode_one_applies(protocol->mode)
               ? &protocol->mode_one_report_state
               : 0;
}

/**
 * @brief Returns the attached-wheel axis override processor.
 *
 * Exposes the current override state maintained while normalizing supported wheel packets.
 *
 * @param[in] protocol Wheel protocol state.
 * @return Current wheel-axis override processor.
 */
const WheelAxisOverrideProcessor *wheel_protocol_axis_overrides(const WheelProtocol *protocol) {
    return &protocol->axis_override_processor;
}

/**
 * @brief Returns the attached-wheel capability state.
 *
 * Exposes capabilities retained from supported active wheel reports.
 *
 * @param[in] protocol Wheel protocol state.
 * @return Current attached-wheel capability state.
 */
const WheelCapabilityState *wheel_protocol_capabilities(const WheelProtocol *protocol) {
    return &protocol->capabilities;
}

/**
 * @brief Returns the attached wheel's axis-limit value.
 *
 * Selects the value from the current mode-one, mode-four, or CRC-family input report. Returns zero
 * until a supported input report is ready.
 *
 * @param[in] protocol Attached-wheel protocol state.
 * @return Current axis-limit value, or zero when unavailable.
 */
uint8_t wheel_protocol_axis_limit(const WheelProtocol *protocol) {
    const WheelPacketModeOneInput *mode_one = wheel_protocol_mode_one_input(protocol);
    if (mode_one != 0) {
        return mode_one->axis_limit;
    }
    const WheelPacketModeFourInput *mode_four = wheel_protocol_mode_four_input(protocol);
    if (mode_four != 0) {
        return mode_four->axis_limit;
    }
    const WheelPacketCrcInput *crc = wheel_protocol_crc_input(protocol);
    return crc != 0 ? crc->axis_limit : 0;
}

/**
 * @brief Returns the attached wheel's secondary button byte.
 *
 * Selects the mode-button field retained by the active mode-one, mode-four, or CRC-family input
 * packet.
 *
 * @param[in] protocol Attached-wheel protocol state.
 * @return Current secondary button byte, or zero when unavailable.
 */
uint8_t wheel_protocol_mode_buttons(const WheelProtocol *protocol) {
    const WheelPacketModeOneInput *mode_one = wheel_protocol_mode_one_input(protocol);
    if (mode_one != 0) {
        return mode_one->mode_buttons;
    }
    const WheelPacketModeFourInput *mode_four = wheel_protocol_mode_four_input(protocol);
    if (mode_four != 0) {
        return mode_four->mode_buttons;
    }
    const WheelPacketCrcInput *crc = wheel_protocol_crc_input(protocol);
    return crc != 0 ? crc->mode_buttons : 0;
}

/**
 * @brief Returns the attached wheel's two primary axis-output bytes.
 *
 * Selects the normalized values from the current mode-one, mode-four, or CRC-family input report.
 *
 * @param[in] protocol Attached-wheel protocol state.
 * @return Two axis-output bytes, or null when no supported input report is ready.
 */
const uint8_t *wheel_protocol_axis_outputs(const WheelProtocol *protocol) {
    const WheelPacketModeOneInput *mode_one = wheel_protocol_mode_one_input(protocol);
    if (mode_one != 0) {
        return mode_one->axis_outputs;
    }
    const WheelPacketModeFourInput *mode_four = wheel_protocol_mode_four_input(protocol);
    if (mode_four != 0) {
        return mode_four->axis_outputs;
    }
    const WheelPacketCrcInput *crc = wheel_protocol_crc_input(protocol);
    return crc != 0 ? crc->axis_outputs : 0;
}

/**
 * @brief Reports whether the attached wheel enabled its axis report.
 *
 * Selects the capability flag retained by the current mode-one, mode-four, or CRC-family input
 * packet. Unsupported and inactive modes report disabled.
 *
 * @param[in] protocol Attached-wheel protocol state.
 * @return True while the current input packet enables its axis report.
 */
bool wheel_protocol_axis_report_enabled(const WheelProtocol *protocol) {
    const WheelPacketModeOneInput *mode_one = wheel_protocol_mode_one_input(protocol);
    if (mode_one != 0) {
        return mode_one->axis_report_enabled != 0;
    }
    const WheelPacketModeFourInput *mode_four = wheel_protocol_mode_four_input(protocol);
    if (mode_four != 0) {
        return mode_four->axis_report_enabled != 0;
    }
    const WheelPacketCrcInput *crc = wheel_protocol_crc_input(protocol);
    return crc != 0 && crc->axis_report_enabled != 0;
}

/**
 * @brief Copies the attached wheel's two 16-bit axis values.
 *
 * Selects the separately retained standard-packet values or the normalized mode-four and
 * CRC-family values. The destination is cleared when no supported input report is ready.
 *
 * @param[in] protocol Attached-wheel protocol state.
 * @param[out] values Two 16-bit axis values.
 * @return True when axis values were available.
 */
bool wheel_protocol_axis_values(const WheelProtocol *protocol, uint16_t values[2]) {
    values[0] = 0;
    values[1] = 0;
    const WheelPacketModeOneReportState *mode_one = wheel_protocol_mode_one_report_state(protocol);
    if (mode_one != 0) {
        values[0] = mode_one->axis_values[0];
        values[1] = mode_one->axis_values[1];
        return true;
    }
    const WheelPacketModeFourInput *mode_four = wheel_protocol_mode_four_input(protocol);
    if (mode_four != 0) {
        values[0] = mode_four->axis_values[0];
        values[1] = mode_four->axis_values[1];
        return true;
    }
    const WheelPacketCrcInput *crc = wheel_protocol_crc_input(protocol);
    if (crc == 0) {
        return false;
    }
    values[0] = crc->axis_values[0];
    values[1] = crc->axis_values[1];
    return true;
}

/**
 * @brief Copies the attached wheel's eight control bytes.
 *
 * Selects normalized mode-one, mode-four, or CRC-family controls. Mode-four control and
 * control-data groups are joined in their packet order. The destination is cleared when no
 * supported input report is ready.
 *
 * @param[in] protocol Attached-wheel protocol state.
 * @param[out] controls Eight control bytes.
 * @return True when controls were available.
 */
bool wheel_protocol_controls(const WheelProtocol *protocol, uint8_t controls[8]) {
    clear(controls, 8);
    const WheelPacketModeOneInput *mode_one = wheel_protocol_mode_one_input(protocol);
    if (mode_one != 0) {
        controls[0] = mode_one->controls.values[0];
        controls[1] = mode_one->controls.values[1];
        controls[2] = mode_one->controls.enabled;
        controls[3] = mode_one->controls.latch_flags;
        controls[4] = mode_one->controls.x;
        controls[5] = mode_one->controls.y;
        controls[6] = mode_one->controls.mode;
        controls[7] = mode_one->controls.packed_values;
        return true;
    }
    const WheelPacketModeFourInput *mode_four = wheel_protocol_mode_four_input(protocol);
    if (mode_four != 0) {
        for (uint8_t index = 0; index < WHEEL_PACKET_MODE_FOUR_CONTROL_COUNT; index++) {
            controls[index] = mode_four->controls[index];
            controls[index + WHEEL_PACKET_MODE_FOUR_CONTROL_COUNT] = mode_four->control_data[index];
        }
        return true;
    }
    const WheelPacketCrcInput *crc = wheel_protocol_crc_input(protocol);
    if (crc == 0) {
        return false;
    }
    for (uint8_t index = 0; index < WHEEL_PACKET_CRC_CONTROL_COUNT; index++) {
        controls[index] = crc->controls[index];
    }
    return true;
}

/**
 * @brief Returns the queued attached-wheel motion direction.
 *
 * Inspects the protocol's primary wrapping motion counter without consuming it.
 *
 * @param[in] protocol Attached-wheel protocol state.
 * @return Negative one, zero, or positive one.
 */
int8_t wheel_protocol_motion_direction(const WheelProtocol *protocol) {
    return wheel_motion_primary_direction(&protocol->motion);
}

/**
 * @brief Takes one queued attached-wheel motion step.
 *
 * Moves the protocol's primary wrapping motion counter one position toward zero.
 *
 * @param[in,out] protocol Attached-wheel protocol state.
 * @return Negative one, zero, or positive one.
 */
int8_t wheel_protocol_take_motion(WheelProtocol *protocol) {
    return wheel_motion_take_primary(&protocol->motion);
}

/**
 * @brief Takes the attached-wheel request-change latch.
 *
 * Returns whether the normalized request changed since the previous take and clears the latch.
 *
 * @param[in,out] protocol Wheel protocol state.
 * @return True when a normalized request change was pending.
 */
bool wheel_protocol_request_changed(WheelProtocol *protocol) {
    bool changed = protocol->request_changed;
    protocol->request_changed = false;
    return changed;
}

/**
 * @brief Reports display-acknowledgement input from the current wheel packet.
 *
 * Returns the directional, button, and mode-specific auxiliary input state captured from mode-one
 * or mode-four requests.
 *
 * @param[in] protocol Attached-wheel protocol state.
 * @return True while an eligible input is active.
 */
bool wheel_protocol_acknowledgement_input_active(const WheelProtocol *protocol) {
    return protocol->request_ready &&
           (wheel_packet_mode_one_applies(protocol->mode) || protocol->mode == 4 ||
            wheel_packet_crc_applies(protocol->mode)) &&
           protocol->acknowledgement_input_active;
}

/**
 * @brief Calculates an attached-wheel packet checksum.
 *
 * Applies the wheel CRC-8 to the first 32 bytes of a complete protocol packet.
 *
 * @param[in] packet Complete attached-wheel protocol packet.
 * @return CRC-8 for the packet content region.
 */
uint8_t wheel_protocol_message_checksum(const uint8_t packet[WHEEL_PROTOCOL_PACKET_SIZE]) {
    return crc8(packet, WHEEL_PROTOCOL_CONTENT_SIZE);
}

/**
 * @brief Validates an attached-wheel packet checksum.
 *
 * Compares the packet checksum byte with the CRC-8 calculated over its 32-byte content region.
 *
 * @param[in] packet Complete attached-wheel protocol packet.
 * @return True when the packet checksum matches.
 */
bool wheel_protocol_message_valid(const uint8_t packet[WHEEL_PROTOCOL_PACKET_SIZE]) {
    return packet[WHEEL_PROTOCOL_CHECKSUM_OFFSET] == wheel_protocol_message_checksum(packet);
}
