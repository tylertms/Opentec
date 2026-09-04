#include "wheel/protocol.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "wheel/authentication.h"
#include "wheel/capability.h"
#include "wheel/output_reports.h"
#include "wheel/packet_adapter.h"
#include "wheel/packet_alternate.h"
#include "wheel/packet_axis_mode.h"
#include "wheel/packet_common.h"
#include "wheel/packet_crc.h"
#include "wheel/packet_display.h"
#include "wheel/packet_extended.h"
#include "wheel/packet_metadata.h"
#include "wheel/packet_mode_four.h"
#include "wheel/packet_mode_one.h"
#include "wheel/packet_remapped.h"
#include "wheel/packet_remote_tuning.h"
#include "wheel/pulse_gate.h"

/** @brief Internal legacy-status response values and offsets. */
enum {
    WHEEL_STATUS_SELECT_PREFIX_MODE = 9,   /**< Mode prefix that includes status fields. */
    WHEEL_STATUS_RESPONSE_CODE = 0x82,     /**< Legacy status response code. */
    WHEEL_STATUS_IDLE = 0x1e,              /**< Idle status value. */
    WHEEL_STATUS_SETUP_PAGE_OFFSET = 0x1f, /**< Setup-page status offset. */
    WHEEL_SETUP_PAGE_MAXIMUM = 5,          /**< Highest setup page value. */
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
 * @brief Clears response fields before an active packet-family encoder runs.
 *
 * CRC-family responses retain display-value bytes five and six and reserved bytes eleven through
 * thirty-one because the official encoder does not write those fields.
 *
 * @param[in,out] protocol Active protocol state and response storage.
 */
static void clear_active_response(WheelProtocol *protocol) {
    if (!wheel_packet_crc_applies(protocol->mode)) {
        clear(protocol->response, WHEEL_PROTOCOL_PACKET_SIZE);
        return;
    }
    clear(protocol->response, 5);
    clear(protocol->response + 7, 4);
    clear(protocol->response + WHEEL_PACKET_CRC_CONTENT_SIZE,
          WHEEL_PROTOCOL_PACKET_SIZE - WHEEL_PACKET_CRC_CONTENT_SIZE);
}

static bool report_mode_marker(uint8_t report_mode) { return report_mode >= 2 && report_mode <= 4; }

static uint8_t active_report_mode(const WheelProtocol *protocol) {
    if (wheel_packet_mode_one_applies(protocol->mode)) {
        return protocol->mode_one_report_state.report_mode;
    }
    if (protocol->mode == 4) {
        return protocol->mode_four_input.report_mode;
    }
    if (wheel_packet_display_applies(protocol->mode)) {
        return protocol->display_input.report_mode;
    }
    if (wheel_packet_remapped_applies(protocol->mode)) {
        return protocol->remapped_input.report_mode;
    }
    if (wheel_packet_alternate_applies(protocol->mode)) {
        return protocol->alternate_input.report_mode;
    }
    if (wheel_packet_packed_applies(protocol->mode)) {
        return protocol->packed_input.report_mode;
    }
    if (wheel_packet_crc_applies(protocol->mode)) {
        return protocol->crc_input.report_mode;
    }
    return protocol->common_input.report_mode;
}

/**
 * @brief Reports whether the active wheel report mode marks the third display glyph.
 *
 * Modes two through four carry the marker in their third display glyph during both command-two
 * response encoding and command-three scan output.
 *
 * @param[in] protocol Protocol state whose active report mode is inspected.
 * @return True when the marker is required.
 */
bool wheel_protocol_report_mode_marker(const WheelProtocol *protocol) {
    return protocol != NULL && report_mode_marker(active_report_mode(protocol));
}

/**
 * @brief Adds the legacy wheel-status fields used for display rotation.
 *
 * Mode 0x0E receives the signed angle in bytes 9 and 10 when profile rotation is enabled, followed
 * by state code 6, a clear adapter flag, and the current two-byte legacy pedal status.
 *
 * @param[in] protocol Active attached-wheel protocol state.
 * @param[out] response Cleared response receiving the legacy status fields.
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
    response[14] = protocol->legacy_pedal_status[0];
    response[15] = protocol->legacy_pedal_status[1];
}

static bool mode_has_input_decoder(uint8_t mode) {
    return wheel_packet_mode_one_applies(mode) || mode == 4 || wheel_packet_adapter_applies(mode) ||
           wheel_packet_display_applies(mode) || wheel_packet_remapped_applies(mode) ||
           wheel_packet_alternate_applies(mode) || wheel_packet_packed_applies(mode) ||
           wheel_packet_axis_mode_applies(mode) || wheel_packet_extended_applies(mode) ||
           wheel_packet_metadata_applies(mode) || wheel_packet_crc_applies(mode) ||
           mode == WHEEL_MODE_REMOTE_TUNING_LEGACY || mode == WHEEL_MODE_REMOTE_TUNING_EXTENDED;
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
 * overlays the highest-priority host output report. A closed interface gate blanks idle mode-0x0F
 * and mode-0x17 content. The checksum is updated while transport acknowledgement flags are
 * preserved.
 *
 * @param[in,out] protocol Active protocol state and response storage.
 */
static void build_active_response(WheelProtocol *protocol) {
    if (protocol->mode_input_invalid || !mode_has_input_decoder(protocol->mode)) {
        return;
    }
    bool remote_tuning_mode = protocol->mode == WHEEL_MODE_REMOTE_TUNING_LEGACY ||
                              protocol->mode == WHEEL_MODE_REMOTE_TUNING_EXTENDED;
    if (!wheel_packet_mode_one_applies(protocol->mode) && protocol->mode != 4 &&
        !wheel_packet_adapter_applies(protocol->mode) &&
        !wheel_packet_display_applies(protocol->mode) &&
        !wheel_packet_remapped_applies(protocol->mode) &&
        !wheel_packet_alternate_applies(protocol->mode) &&
        !wheel_packet_packed_applies(protocol->mode) &&
        !wheel_packet_axis_mode_applies(protocol->mode) &&
        !wheel_packet_extended_applies(protocol->mode) &&
        !wheel_packet_metadata_applies(protocol->mode) &&
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
    bool third_glyph_marker = wheel_protocol_report_mode_marker(protocol);
    uint8_t flags = protocol->response[WHEEL_PROTOCOL_FLAGS_OFFSET];
    clear_active_response(protocol);
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
    } else if (wheel_packet_adapter_applies(protocol->mode)) {
        protocol->adapter_output.display.third_glyph_marker = third_glyph_marker;
        wheel_packet_adapter_encode(&protocol->adapter_output, &protocol->adapter, protocol->now_ms,
                                    protocol->response);
    } else if (wheel_packet_crc_applies(protocol->mode)) {
        bool marker = protocol->crc_output.display.third_glyph_marker;
        protocol->crc_output.display.third_glyph_marker = third_glyph_marker;
        wheel_packet_crc_encode(protocol->mode, &protocol->crc_output, protocol->response);
        protocol->crc_output.display.third_glyph_marker = marker;
    } else if (protocol->mode == 4) {
        WheelPacketModeFourOutput output = protocol->mode_four_output;
        output.display.third_glyph_marker = third_glyph_marker;
        wheel_packet_mode_four_encode(&output, protocol->adapter_output.display_report,
                                      protocol->response);
    } else if (wheel_packet_alternate_applies(protocol->mode)) {
        wheel_packet_alternate_encode(&protocol->alternate_output, protocol->response);
    } else if (wheel_packet_display_applies(protocol->mode) ||
               wheel_packet_remapped_applies(protocol->mode) ||
               wheel_packet_axis_mode_applies(protocol->mode) ||
               wheel_packet_extended_applies(protocol->mode) ||
               wheel_packet_metadata_applies(protocol->mode)) {
        WheelDisplayOutput display = protocol->mode_one_output.display;
        display.third_glyph_marker = third_glyph_marker;
        if (protocol->display_character_mode && protocol->mode == 0x09) {
            for (uint8_t index = 0; index < WHEEL_DISPLAY_GLYPH_COUNT; index++) {
                display.glyphs[index] = wheel_display_output_character(display.glyphs[index]);
            }
        }
        wheel_packet_common_response_encode(&display, protocol->mode_one_output.vibration,
                                            protocol->mode_one_output.legacy_axes,
                                            protocol->response);
        if (protocol->mode == 0x09) {
            protocol->response[0] = WHEEL_PROTOCOL_COMMAND_SELECT_MODE;
        }
    } else if (wheel_packet_packed_applies(protocol->mode)) {
        WheelDisplayOutput display = protocol->mode_one_output.display;
        display.third_glyph_marker = third_glyph_marker;
        wheel_packet_packed_encode(&display, protocol->mode_one_output.vibration,
                                   protocol->mode_one_output.legacy_axes, protocol->response);
    } else if (!remote_tuning_mode) {
        WheelPacketModeOneOutput output = protocol->mode_one_output;
        output.display.third_glyph_marker = third_glyph_marker;
        wheel_packet_mode_one_encode(protocol->mode, &output,
                                     protocol->adapter_output.display_report, protocol->response);
    } else {
        protocol->response[0] = WHEEL_PROTOCOL_COMMAND_AUTHENTICATE;
    }
    if (!system_control_response && !remote_tuning_response) {
        encode_legacy_status(protocol, protocol->response);
        if (!system_status_response) {
            bool generic_output_supported =
                protocol->mode == 0x09 || protocol->mode == 0x0a || protocol->mode == 0x0b ||
                protocol->mode == 0x0f || protocol->mode == 0x10 || protocol->mode == 0x11 ||
                protocol->mode == 0x16 || protocol->mode == 0x17 || protocol->mode == 0x1b ||
                protocol->mode == 0x1d ||
                wheel_output_reports_shifter_state_pending(&protocol->output_reports);
            bool report_encoded =
                generic_output_supported &&
                wheel_output_reports_encode_next(&protocol->output_reports, protocol->mode,
                                                 protocol->response);
            if (!report_encoded && (protocol->mode == WHEEL_MODE_LEGACY_ALTERNATE ||
                                    protocol->mode == WHEEL_MODE_LEGACY_COMPATIBILITY)) {
                bool interface_mode_gate =
                    wheel_output_reports_interface_mode_gate(&protocol->output_reports);
                if (!interface_mode_gate) {
                    clear(protocol->response + 1, WHEEL_PROTOCOL_CONTENT_SIZE - 1);
                }
                protocol->response[WHEEL_PROTOCOL_INTERFACE_MODE_GATE_OFFSET] =
                    (protocol->response[WHEEL_PROTOCOL_INTERFACE_MODE_GATE_OFFSET] & 0xfeu) |
                    (interface_mode_gate ? 1u : 0u);
            }
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
    if (!wheel_packet_crc_applies(protocol->mode) || system_status_response) {
        protocol->response[WHEEL_PROTOCOL_CHECKSUM_OFFSET] =
            crc8(protocol->response, WHEEL_PROTOCOL_CONTENT_SIZE);
    }
    protocol->response[WHEEL_PROTOCOL_FLAGS_OFFSET] = flags;
}

/**
 * @brief Selects the attached-wheel display rotation output.
 *
 * Retains whether display rotation is enabled and the current signed angle for the next legacy
 * remote-tuning response.
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
 * @brief Updates the legacy pedal status returned to the attached wheel.
 *
 * Replaces both retained status bytes used by legacy wheel responses.
 *
 * @param[in,out] protocol Attached-wheel protocol state.
 * @param[in] first First pedal status byte.
 * @param[in] second Second pedal status byte.
 */
void wheel_protocol_set_legacy_pedal_status(WheelProtocol *protocol, uint8_t first,
                                            uint8_t second) {
    protocol->legacy_pedal_status[0] = first;
    protocol->legacy_pedal_status[1] = second;
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
 * Accepts any filtered button bit or an active authenticated button-latch flag.
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
 * @brief Clears the motion deltas consumed by the wheel acknowledgement transition.
 *
 * The official reset covers the primary counter and the first two auxiliary counters while
 * preserving the later auxiliary channels.
 *
 * @param[in,out] protocol Protocol state containing queued wheel motion.
 */
static void reset_acknowledgement_motion(WheelProtocol *protocol) {
    protocol->motion.primary = 0;
    protocol->motion.axes[0] = 0;
    protocol->motion.axes[1] = 0;
}

/**
 * @brief Decodes one negative and positive pulse pair.
 *
 * Gives the negative flag priority when both flags are present, otherwise returns the positive or
 * idle direction.
 *
 * @param[in] flags Four positive/negative pulse pairs.
 * @param[in] negative_mask Negative-direction flag for the selected pair.
 * @param[in] positive_mask Positive-direction flag for the selected pair.
 * @return Minus one, plus one, or zero for the selected pulse pair.
 */
static int8_t pulse_delta(uint8_t flags, uint8_t negative_mask, uint8_t positive_mask) {
    if ((flags & negative_mask) != 0) {
        return -1;
    }
    return (flags & positive_mask) != 0 ? 1 : 0;
}

/**
 * @brief Accumulates mode-0x18 pulse input.
 *
 * Applies the interface timing gate and maps each lower/higher bit pair to positive/negative
 * motion: bits 4/5 drive primary motion, bits 2/3 drive auxiliary axis zero, and bits 0/1 drive
 * auxiliary axis one. PlayStation and direct reports also map bits 6/7 to auxiliary axis two. The
 * packet motion byte is replaced by its signed primary direction.
 *
 * @param[in,out] protocol Protocol state containing pulse timing and motion counters.
 */
static void accumulate_filtered_pulses(WheelProtocol *protocol) {
    uint8_t flags = (uint8_t)protocol->crc_input.motion;
    int8_t primary = pulse_delta(flags, 0x20, 0x10);
    if (!wheel_pulse_gate_ready(&protocol->pulse_gate, protocol->interface_mode, protocol->now_ms,
                                flags)) {
        protocol->crc_input.motion = 0;
        return;
    }
    protocol->crc_input.motion = primary;
    wheel_motion_accumulate_primary(&protocol->motion, primary);
    wheel_motion_accumulate_axis(&protocol->motion, 0, pulse_delta(flags, 0x08, 0x04));
    wheel_motion_accumulate_axis(&protocol->motion, 1, pulse_delta(flags, 0x02, 0x01));
    if (protocol->interface_mode != 6) {
        wheel_motion_accumulate_axis(&protocol->motion, 2, pulse_delta(flags, 0x80, 0x40));
    }
}

/**
 * @brief Accumulates axis-mode rotary and interface pulse input.
 *
 * Always queues the signed packet motion on the primary counter. Xbox reports additionally decode
 * three positive/negative bit pairs after the 90 ms gate, while PlayStation reports convert the
 * signed motion to one pulse pair after the 15 ms gate.
 *
 * @param[in,out] protocol Protocol state containing axis-mode input, pulse timing, and counters.
 */
static void accumulate_axis_mode_motion(WheelProtocol *protocol) {
    int8_t motion = protocol->common_input.motion;
    wheel_motion_accumulate_primary(&protocol->motion, motion);

    uint8_t flags;
    if (protocol->interface_mode == 7) {
        flags = motion < 0 ? 0x20 : motion > 0 ? 0x10 : 0;
    } else if (protocol->interface_mode == 6) {
        flags = (uint8_t)motion;
    } else {
        return;
    }
    if (!wheel_pulse_gate_ready(&protocol->pulse_gate, protocol->interface_mode, protocol->now_ms,
                                flags)) {
        return;
    }

    wheel_motion_accumulate_axis(&protocol->motion, 0, pulse_delta(flags, 0x20, 0x10));
    if (protocol->interface_mode == 6) {
        wheel_motion_accumulate_axis(&protocol->motion, 1, pulse_delta(flags, 0x08, 0x04));
        wheel_motion_accumulate_axis(&protocol->motion, 2, pulse_delta(flags, 0x02, 0x01));
    }
}

/**
 * @brief Accumulates adapter-oriented rotary and interface pulse input.
 *
 * Converts signed adapter motion into interface pulse flags for the Xbox or PlayStation pulse gate.
 * Other interfaces retain the signed motion without generating report pulses.
 *
 * @param[in,out] protocol Protocol state containing adapter input, pulse timing, and counters.
 */
static void accumulate_adapter_motion(WheelProtocol *protocol) {
    if (!protocol->adapter.connected) {
        return;
    }

    if (protocol->interface_mode != 6 && protocol->interface_mode != 7) {
        return;
    }
    const int8_t motion = protocol->common_input.motion;
    const uint8_t flags = motion > 0 ? 0x10u : motion < 0 ? 0x20u : 0;
    if (!wheel_pulse_gate_ready(&protocol->pulse_gate, protocol->interface_mode, protocol->now_ms,
                                flags)) {
        return;
    }

    wheel_motion_accumulate_axis(&protocol->motion, 0, motion);
}

/**
 * @brief Accumulates extended-family rotary and pulse input.
 *
 * Queues the packet's primary rotary step before applying interface pulse timing. Xbox and
 * PlayStation interfaces map the three lower directional pairs to auxiliary counters. Status mode
 * additionally maps bits six and seven to a fourth counter. Direct interfaces retain each pair
 * independently for 80 milliseconds and expose bits four and five as the normalized motion
 * direction.
 *
 * @param[in,out] protocol Protocol state containing extended input, pulse timing, and counters.
 */
static void accumulate_extended_motion(WheelProtocol *protocol) {
    uint8_t flags = (uint8_t)protocol->common_input.motion;
    if (protocol->mode == WHEEL_PACKET_EXTENDED_MODE_REMOTE) {
        int8_t primary = (flags & 0x10u) != 0 ? 1 : (flags & 0x20u) != 0 ? -1 : 0;
        if (primary == 0) {
            protocol->extended_primary_released = true;
        } else if (protocol->extended_primary_released) {
            protocol->extended_primary_released = false;
            wheel_motion_accumulate_primary(&protocol->motion, primary);
        }
        int8_t secondary = (flags & 0x40u) != 0 ? 1 : (flags & 0x80u) != 0 ? -1 : 0;
        if (secondary == 0) {
            protocol->extended_secondary_released = true;
        } else if (protocol->extended_secondary_released) {
            protocol->extended_secondary_released = false;
            wheel_motion_accumulate_axis(&protocol->motion, 1, secondary);
        }
    } else {
        wheel_motion_accumulate_primary(
            &protocol->motion, wheel_packet_extended_primary_delta(&protocol->common_input));
    }

    if (protocol->interface_mode != 6 && protocol->interface_mode != 7) {
        uint8_t active_flags = wheel_packet_extended_hold_direct_pulses(
            &protocol->extended_pulse_state, protocol->mode, protocol->now_ms, flags);
        protocol->common_input.motion = flags == 0                    ? 0
                                        : (active_flags & 0x10u) != 0 ? 1
                                        : (active_flags & 0x20u) != 0 ? -1
                                                                      : 0;
        return;
    }

    if (!wheel_pulse_gate_ready(&protocol->pulse_gate, protocol->interface_mode, protocol->now_ms,
                                flags)) {
        protocol->common_input.motion = 0;
        return;
    }
    protocol->common_input.motion = pulse_delta(flags, 0x20, 0x10);
    wheel_motion_accumulate_axis(&protocol->motion, 0, pulse_delta(flags, 0x20, 0x10));
    wheel_motion_accumulate_axis(&protocol->motion, 1, pulse_delta(flags, 0x08, 0x04));
    wheel_motion_accumulate_axis(&protocol->motion, 2, pulse_delta(flags, 0x02, 0x01));
    if (protocol->mode == WHEEL_PACKET_EXTENDED_MODE_STATUS) {
        wheel_motion_accumulate_axis(&protocol->motion, 3, pulse_delta(flags, 0x80, 0x40));
    }
}

static void accumulate_common_pulses(WheelProtocol *protocol, WheelPacketCommonInput *input,
                                     bool fourth_axis) {
    uint8_t flags = (uint8_t)input->motion;
    if (protocol->interface_mode != 6 && protocol->interface_mode != 7) {
        uint8_t active_flags = wheel_packet_extended_hold_direct_pulses(
            &protocol->extended_pulse_state, protocol->mode, protocol->now_ms, flags);
        input->motion = flags == 0                    ? 0
                        : (active_flags & 0x10u) != 0 ? 1
                        : (active_flags & 0x20u) != 0 ? -1
                                                      : 0;
        return;
    }
    bool packed_mode = wheel_packet_packed_applies(protocol->mode);
    bool pulse_ready = packed_mode
                           ? wheel_pulse_gate_ready_for_packed(
                                 &protocol->pulse_gate, protocol->interface_mode,
                                 protocol->now_ms, flags)
                           : wheel_pulse_gate_ready(&protocol->pulse_gate, protocol->interface_mode,
                                                    protocol->now_ms, flags);
    if (!pulse_ready) {
        input->motion = 0;
        return;
    }
    input->motion = pulse_delta(flags, 0x20, 0x10);
    wheel_motion_accumulate_axis(&protocol->motion, 0, pulse_delta(flags, 0x20, 0x10));
    wheel_motion_accumulate_axis(&protocol->motion, 1, pulse_delta(flags, 0x08, 0x04));
    wheel_motion_accumulate_axis(&protocol->motion, 2, pulse_delta(flags, 0x02, 0x01));
    if (fourth_axis) {
        wheel_motion_accumulate_axis(&protocol->motion, 3, pulse_delta(flags, 0x80, 0x40));
    }
}

/**
 * @brief Detects packed-family input eligible to acknowledge a display overlay.
 *
 * Accepts any filtered primary button bit or the fifth control byte.
 *
 * @param[in] input Filtered and normalized packed-family request.
 * @return True while an eligible input is active.
 */
static bool packed_acknowledgement_input_active(const WheelPacketPackedInput *input) {
    bool button_active = input->buttons[0] != 0 || input->buttons[1] != 0 || input->buttons[2] != 0;
    return button_active || input->controls[4] != 0;
}

/**
 * @brief Detects common-packet buttons eligible to acknowledge an overlay.
 *
 * Accepts any filtered primary button bit and excludes axis and report controls.
 *
 * @param[in] input Filtered common-packet request.
 * @return True while an eligible input is active.
 */
static bool common_buttons_acknowledgement_input_active(const WheelPacketCommonInput *input) {
    return input->buttons[0] != 0 || input->buttons[1] != 0 || input->buttons[2] != 0;
}

/**
 * @brief Captures an active attached-wheel request.
 *
 * Captures extended remote-tuning control packets separately. Other requests are decoded and
 * normalized for the selected mode, record display-acknowledgement input, latch attached-wheel
 * input capability, update the change snapshot, and preserve separately consumed report fields.
 *
 * @param[in,out] protocol Protocol state that owns the request snapshot and change latch.
 * @param[in] request Complete 57-byte attached-wheel request.
 */
static void capture_request(WheelProtocol *protocol,
                            const uint8_t request[WHEEL_PROTOCOL_PACKET_SIZE]) {
    if (protocol->mode_input_invalid || !mode_has_input_decoder(protocol->mode)) {
        protocol->request_ready = false;
        protocol->acknowledgement_input_active = false;
        return;
    }
    if (protocol->mode == WHEEL_MODE_REMOTE_TUNING_EXTENDED &&
        request[0] == WHEEL_PROTOCOL_COMMAND_AUTHENTICATE_REPLY) {
        for (uint8_t index = 0; index < sizeof(protocol->remote_tuning_controls); index++) {
            protocol->remote_tuning_controls[index] = request[index + 2];
        }
        protocol->remote_tuning_controls_pending = true;
    } else if (wheel_packet_mode_one_applies(protocol->mode)) {
        uint8_t snapshot[WHEEL_PACKET_MODE_ONE_SNAPSHOT_SIZE];
        bool authenticated_controls =
            protocol->mode == 0x13 || protocol->mode == 0x14 || protocol->mode == 0x16;
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
        if (authenticated_controls) {
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
        wheel_packet_mode_one_normalize(&protocol->mode_one_input, authenticated_controls,
                                        protocol->button_latch_enabled,
                                        protocol->profile_transition_pending, snapshot);
        bool changed = false;
        for (uint8_t index = 0; index < WHEEL_PROTOCOL_SNAPSHOT_SIZE; index++) {
            changed |= protocol->request[index] != snapshot[index];
            protocol->request[index] = snapshot[index];
        }
        protocol->request_changed |= changed;
    } else if (wheel_packet_adapter_applies(protocol->mode)) {
        uint8_t snapshot[WHEEL_PACKET_COMMON_SNAPSHOT_SIZE];
        wheel_packet_common_decode(request, &protocol->common_input);
        wheel_capability_update_report(&protocol->capabilities, protocol->common_input.report_mode,
                                       protocol->common_input.report_capabilities);
        wheel_packet_common_filter(&protocol->common_filter, &protocol->common_input);
        wheel_packet_adapter_merge(&protocol->common_input, &protocol->adapter);
        protocol->capabilities.input_available |= protocol->adapter.buttons_active;
        accumulate_adapter_motion(protocol);
        uint8_t adapter_controls[8] = {0};
        adapter_controls[5] = protocol->common_input.controls[4];
        adapter_controls[6] = protocol->common_input.controls[5];
        adapter_controls[7] = protocol->common_input.controls[2];
        wheel_axis_override_process_packet(
            &protocol->axis_override_processor, protocol->configured_axis_override_mode,
            protocol->mode, protocol->interface_mode, protocol->common_input.axis_limit,
            protocol->now_ms, &protocol->paddle_bite_point_percent,
            &protocol->common_input.buttons[0], &protocol->common_input.motion, adapter_controls,
            protocol->common_input.axis_outputs);
        protocol->common_input.controls[4] = adapter_controls[5];
        protocol->common_input.controls[5] = adapter_controls[6];
        protocol->common_input.controls[2] = adapter_controls[7];
        wheel_packet_common_snapshot(&protocol->common_input, snapshot);
        protocol->acknowledgement_input_active =
            common_buttons_acknowledgement_input_active(&protocol->common_input);
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
        uint8_t mode_four_controls[8];
        for (uint8_t index = 0; index < 4; index++) {
            mode_four_controls[index] = protocol->mode_four_input.controls[index];
            mode_four_controls[index + 4] = protocol->mode_four_input.control_data[index];
        }
        wheel_axis_override_process_packet(
            &protocol->axis_override_processor, mode_four_controls[6], protocol->mode,
            protocol->interface_mode, protocol->mode_four_input.axis_limit, protocol->now_ms,
            &protocol->paddle_bite_point_percent, &protocol->mode_four_input.buttons[0],
            &protocol->mode_four_input.motion, mode_four_controls,
            protocol->mode_four_input.axis_outputs);
        for (uint8_t index = 0; index < 4; index++) {
            protocol->mode_four_input.controls[index] = mode_four_controls[index];
            protocol->mode_four_input.control_data[index] = mode_four_controls[index + 4];
        }
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
    } else if (wheel_packet_display_applies(protocol->mode)) {
        uint8_t snapshot[WHEEL_PACKET_COMMON_SNAPSHOT_SIZE];
        wheel_packet_common_decode(request, &protocol->display_input);
        wheel_capability_update(&protocol->capabilities, protocol->mode,
                                protocol->display_input.report_mode,
                                protocol->display_input.report_capabilities);
        wheel_packet_display_filter(&protocol->display_filter, &protocol->display_input);
        accumulate_common_pulses(protocol, &protocol->display_input, false);
        uint8_t report_enabled = protocol->display_input.controls[7];
        protocol->display_input.controls[7] = 1;
        wheel_axis_override_process_packet(
            &protocol->axis_override_processor, protocol->configured_axis_override_mode,
            protocol->mode, protocol->interface_mode, protocol->display_input.axis_limit,
            protocol->now_ms, &protocol->paddle_bite_point_percent,
            &protocol->display_input.buttons[0], &protocol->display_input.motion,
            protocol->display_input.controls, protocol->display_input.axis_outputs);
        protocol->display_input.controls[7] = report_enabled;
        wheel_packet_common_snapshot(&protocol->display_input, snapshot);
        protocol->acknowledgement_input_active =
            common_buttons_acknowledgement_input_active(&protocol->display_input);
        bool changed = false;
        for (uint8_t index = 0; index < WHEEL_PROTOCOL_SNAPSHOT_SIZE; index++) {
            changed |= protocol->request[index] != snapshot[index];
            protocol->request[index] = snapshot[index];
        }
        protocol->request_changed |= changed;
    } else if (wheel_packet_remapped_applies(protocol->mode)) {
        uint8_t snapshot[WHEEL_PACKET_COMMON_SNAPSHOT_SIZE];
        wheel_packet_common_decode(request, &protocol->remapped_input);
        wheel_capability_update(&protocol->capabilities, protocol->mode,
                                protocol->remapped_input.report_mode,
                                protocol->remapped_input.report_capabilities);
        int8_t motion_delta = wheel_packet_remapped_primary_delta(&protocol->remapped_input);
        wheel_motion_accumulate_primary(&protocol->motion, motion_delta);
        wheel_packet_remapped_filter(&protocol->remapped_filter, &protocol->remapped_input,
                                     protocol->interface_mode);
        accumulate_common_pulses(protocol, &protocol->remapped_input, false);
        wheel_packet_common_snapshot(&protocol->remapped_input, snapshot);
        protocol->acknowledgement_input_active =
            common_buttons_acknowledgement_input_active(&protocol->remapped_input);
        bool changed = false;
        for (uint8_t index = 0; index < WHEEL_PROTOCOL_SNAPSHOT_SIZE; index++) {
            changed |= protocol->request[index] != snapshot[index];
            protocol->request[index] = snapshot[index];
        }
        protocol->request_changed |= changed;
    } else if (wheel_packet_alternate_applies(protocol->mode)) {
        uint8_t snapshot[WHEEL_PACKET_COMMON_SNAPSHOT_SIZE];
        wheel_packet_common_decode(request, &protocol->alternate_input);
        wheel_capability_update(&protocol->capabilities, protocol->mode,
                                protocol->alternate_input.report_mode,
                                protocol->alternate_input.report_capabilities);
        wheel_packet_alternate_filter(&protocol->alternate_filter, &protocol->alternate_input,
                                      protocol->interface_mode);
        wheel_packet_common_snapshot(&protocol->alternate_input, snapshot);
        protocol->acknowledgement_input_active =
            common_buttons_acknowledgement_input_active(&protocol->alternate_input);
        bool changed = false;
        for (uint8_t index = 0; index < WHEEL_PROTOCOL_SNAPSHOT_SIZE; index++) {
            changed |= protocol->request[index] != snapshot[index];
            protocol->request[index] = snapshot[index];
        }
        protocol->request_changed |= changed;
    } else if (wheel_packet_packed_applies(protocol->mode)) {
        uint8_t snapshot[WHEEL_PACKET_PACKED_SNAPSHOT_SIZE];
        wheel_packet_packed_decode(request, &protocol->packed_input);
        wheel_capability_update(&protocol->capabilities, protocol->mode,
                                protocol->packed_input.report_mode,
                                protocol->packed_input.report_capabilities);
        protocol->capabilities.input_available |=
            protocol->packed_input.controls[2] != 0 || protocol->packed_input.controls[3] != 0;
        wheel_packet_packed_filter_buttons(&protocol->packed_filter, &protocol->packed_input);
        wheel_packet_packed_normalize(&protocol->packed_input);
        accumulate_common_pulses(protocol, &protocol->packed_input, false);
        wheel_axis_override_process_packet(
            &protocol->axis_override_processor, protocol->configured_axis_override_mode,
            protocol->mode, protocol->interface_mode, protocol->packed_input.axis_limit,
            protocol->now_ms, &protocol->paddle_bite_point_percent,
            &protocol->packed_input.buttons[0], &protocol->packed_input.motion,
            protocol->packed_input.controls, protocol->packed_input.axis_outputs);
        wheel_packet_packed_snapshot(&protocol->packed_input, snapshot);
        protocol->acknowledgement_input_active =
            packed_acknowledgement_input_active(&protocol->packed_input);
        bool changed = false;
        for (uint8_t index = 0; index < WHEEL_PROTOCOL_SNAPSHOT_SIZE; index++) {
            changed |= protocol->request[index] != snapshot[index];
            protocol->request[index] = snapshot[index];
        }
        protocol->request_changed |= changed;
    } else if (wheel_packet_axis_mode_applies(protocol->mode)) {
        uint8_t snapshot[WHEEL_PACKET_COMMON_SNAPSHOT_SIZE];
        wheel_packet_common_decode(request, &protocol->common_input);
        wheel_capability_update(&protocol->capabilities, protocol->mode,
                                protocol->common_input.report_mode,
                                protocol->common_input.report_capabilities);
        accumulate_axis_mode_motion(protocol);
        wheel_packet_common_filter(&protocol->axis_mode_filter, &protocol->common_input);
        wheel_packet_common_expand_packed_controls(&protocol->common_input);
        wheel_axis_override_process_axis_mode(
            &protocol->axis_override_processor, protocol->common_input.controls[6],
            protocol->interface_mode, protocol->now_ms, &protocol->paddle_bite_point_percent,
            &protocol->common_input.buttons[0], &protocol->common_input.motion,
            protocol->common_input.controls, protocol->common_input.axis_outputs);
        wheel_packet_common_snapshot(&protocol->common_input, snapshot);
        protocol->acknowledgement_input_active =
            common_buttons_acknowledgement_input_active(&protocol->common_input);
        bool changed = false;
        for (uint8_t index = 0; index < WHEEL_PROTOCOL_SNAPSHOT_SIZE; index++) {
            changed |= protocol->request[index] != snapshot[index];
            protocol->request[index] = snapshot[index];
        }
        protocol->request_changed |= changed;
    } else if (wheel_packet_extended_applies(protocol->mode)) {
        uint8_t snapshot[WHEEL_PACKET_COMMON_SNAPSHOT_SIZE];
        wheel_packet_common_decode(request, &protocol->common_input);
        wheel_capability_update(&protocol->capabilities, protocol->mode,
                                protocol->common_input.report_mode,
                                protocol->common_input.report_capabilities);
        protocol->capabilities.input_available |=
            protocol->common_input.controls[2] != 0 || protocol->common_input.controls[3] != 0;
        accumulate_extended_motion(protocol);
        wheel_packet_common_filter(&protocol->extended_filter, &protocol->common_input);
        if (protocol->mode != WHEEL_PACKET_EXTENDED_MODE_REMOTE) {
            wheel_packet_extended_swap_buttons(&protocol->common_input);
        }
        wheel_packet_common_expand_packed_controls(&protocol->common_input);
        wheel_packet_common_latch_buttons(&protocol->common_input, protocol->button_latch_enabled,
                                          protocol->profile_transition_pending);
        wheel_axis_override_process(
            &protocol->axis_override_processor, protocol->common_input.controls[6], protocol->mode,
            protocol->interface_mode, protocol->common_input.controls[2] != 0, protocol->now_ms,
            &protocol->paddle_bite_point_percent, &protocol->common_input.buttons[0],
            &protocol->common_input.motion, protocol->common_input.controls[4],
            protocol->common_input.controls[5], protocol->common_input.axis_outputs);
        wheel_packet_common_snapshot(&protocol->common_input, snapshot);
        protocol->acknowledgement_input_active =
            common_buttons_acknowledgement_input_active(&protocol->common_input);
        bool changed = false;
        for (uint8_t index = 0; index < WHEEL_PROTOCOL_SNAPSHOT_SIZE; index++) {
            changed |= protocol->request[index] != snapshot[index];
            protocol->request[index] = snapshot[index];
        }
        protocol->request_changed |= changed;
    } else if (wheel_packet_metadata_applies(protocol->mode)) {
        wheel_packet_metadata_decode(request, &protocol->common_input);
        wheel_capability_update(&protocol->capabilities, protocol->mode,
                                protocol->common_input.report_mode,
                                protocol->common_input.report_capabilities);
        protocol->acknowledgement_input_active = false;
    } else if (wheel_packet_crc_applies(protocol->mode)) {
        uint8_t snapshot[WHEEL_PACKET_CRC_SNAPSHOT_SIZE];
        wheel_packet_crc_decode(request, &protocol->crc_input);
        wheel_capability_update(&protocol->capabilities, protocol->mode,
                                protocol->crc_input.report_mode,
                                protocol->crc_input.report_capabilities);
        if (protocol->mode == WHEEL_MODE_CRC_AUTHENTICATED &&
            (!protocol->adapter.connected || protocol->adapter.mode != 1)) {
            protocol->capabilities.input_available = false;
        }
        protocol->capabilities.input_available |=
            protocol->crc_input.controls[2] != 0 || protocol->crc_input.controls[3] != 0 ||
            (protocol->adapter.connected && protocol->adapter.mode == 1);
        wheel_packet_crc_prepare(&protocol->crc_input, protocol->mode, protocol->interface_mode);
        wheel_packet_crc_filter(&protocol->crc_filter, &protocol->crc_input, protocol->mode);
        if (protocol->mode == WHEEL_MODE_FILTERED_PULSE) {
            accumulate_filtered_pulses(protocol);
            wheel_packet_crc_smooth_axes(&protocol->crc_filter, &protocol->crc_input);
        }
        wheel_packet_crc_normalize(&protocol->crc_input, protocol->mode, protocol->interface_mode,
                                   &protocol->adapter);
        wheel_axis_override_process_packet(
            &protocol->axis_override_processor, protocol->configured_axis_override_mode,
            protocol->mode, protocol->interface_mode, protocol->crc_input.axis_limit,
            protocol->now_ms, &protocol->paddle_bite_point_percent, &protocol->crc_input.buttons[0],
            &protocol->crc_input.motion, protocol->crc_input.controls,
            protocol->crc_input.axis_outputs);
        if (protocol->mode != WHEEL_MODE_FILTERED_PULSE) {
            wheel_motion_accumulate_primary(&protocol->motion, protocol->crc_input.motion);
        }
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
    protocol->axis_report_enabled_latched |=
        protocol->mode_four_runtime.axis_report_enabled != 0 ||
        protocol->mode_one_input.axis_report_enabled != 0 ||
        protocol->display_input.axis_report_enabled != 0 ||
        protocol->remapped_input.axis_report_enabled != 0 ||
        protocol->alternate_input.axis_report_enabled != 0 ||
        protocol->packed_input.axis_report_enabled != 0 ||
        protocol->common_input.axis_report_enabled != 0 ||
        protocol->crc_input.axis_report_enabled != 0 ||
        protocol->axis_override_processor.packet_axis_report_enabled;
    protocol->request_ready = true;
}

/**
 * @brief Selects the attached-wheel packet mode.
 *
 * Recognizes the two scan commands and command A5. Every A5 mode through the official maximum
 * enters authentication or active traffic; modes without an input decoder use a blank active
 * response. Out-of-range A5 values enter the unsupported phase with an invalid-command latch.
 * Recognized selections produce an acknowledgement response.
 *
 * @param[in,out] protocol Wheel protocol state to update.
 * @param[in] request Complete attached-wheel selection request.
 */
static void select_mode(WheelProtocol *protocol,
                        const uint8_t request[WHEEL_PROTOCOL_PACKET_SIZE]) {
    switch (request[0]) {
    case WHEEL_PROTOCOL_COMMAND_SCAN_PRIMARY:
        protocol->mode = WHEEL_MODE_SCAN_PRIMARY;
        protocol->mode_input_invalid = false;
        protocol->command_invalid = false;
        protocol->selection_recovery_pending = false;
        protocol->phase = WHEEL_PROTOCOL_SCANNING_PRIMARY;
        break;
    case WHEEL_PROTOCOL_COMMAND_SCAN_SECONDARY:
        protocol->mode = WHEEL_MODE_SCAN_SECONDARY;
        protocol->mode_input_invalid = false;
        protocol->command_invalid = false;
        protocol->selection_recovery_pending = false;
        protocol->phase = WHEEL_PROTOCOL_SCANNING_SECONDARY;
        break;
    case WHEEL_PROTOCOL_COMMAND_SELECT_MODE:
        if (request[1] > WHEEL_MODE_MAXIMUM) {
            protocol->mode_input_invalid = true;
            protocol->command_invalid = true;
            protocol->selection_recovery_pending = false;
            protocol->phase = WHEEL_PROTOCOL_UNSUPPORTED;
            break;
        }
        protocol->mode = request[1];
        protocol->mode_input_invalid = !mode_has_input_decoder(protocol->mode);
        protocol->command_invalid = protocol->mode_input_invalid;
        protocol->selection_recovery_pending = false;
        if (wheel_authentication_required(protocol->mode)) {
            wheel_authentication_init(&protocol->authentication, protocol->mode);
            protocol->phase = WHEEL_PROTOCOL_AUTHENTICATING;
        } else {
            protocol->phase = WHEEL_PROTOCOL_ACTIVE;
        }
        break;
    default:
        protocol->selection_recovery_pending = true;
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
    const WheelPacketDisplayInput empty_display_input = {0};
    const WheelPacketRemappedInput empty_remapped_input = {0};
    const WheelPacketAlternateInput empty_alternate_input = {0};
    const WheelPacketAlternateOutput empty_alternate_output = {0};
    const WheelPacketPackedInput empty_packed_input = {0};
    const WheelPacketCommonInput empty_common_input = {0};
    const WheelPacketCrcInput empty_crc_input = {0};
    const WheelPacketCrcOutput empty_crc_output = {0};
    const WheelAdapterInput empty_adapter = {0};
    const WheelPacketAdapterOutput empty_adapter_output = {0};
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
    wheel_packet_display_filter_init(&protocol->display_filter);
    protocol->display_input = empty_display_input;
    wheel_packet_remapped_filter_init(&protocol->remapped_filter);
    protocol->remapped_input = empty_remapped_input;
    wheel_packet_alternate_filter_init(&protocol->alternate_filter);
    protocol->alternate_input = empty_alternate_input;
    protocol->alternate_output = empty_alternate_output;
    wheel_packet_packed_filter_init(&protocol->packed_filter);
    protocol->packed_input = empty_packed_input;
    wheel_packet_common_filter_init(&protocol->common_filter);
    wheel_packet_common_filter_init(&protocol->axis_mode_filter);
    wheel_packet_common_filter_init(&protocol->extended_filter);
    protocol->common_input = empty_common_input;
    wheel_packet_extended_pulse_init(&protocol->extended_pulse_state);
    wheel_packet_crc_filter_init(&protocol->crc_filter);
    protocol->crc_input = empty_crc_input;
    protocol->crc_output = empty_crc_output;
    protocol->adapter = empty_adapter;
    protocol->adapter_output = empty_adapter_output;
    wheel_packet_remote_tuning_init(&protocol->system_control_output);
    wheel_packet_remote_tuning_init(&protocol->remote_tuning_output);
    wheel_output_reports_init(&protocol->output_reports);
    wheel_capability_init(&protocol->capabilities);
    wheel_authentication_init(&protocol->authentication, WHEEL_MODE_UNKNOWN);
    protocol->phase = WHEEL_PROTOCOL_WAITING;
    protocol->now_ms = 0;
    wheel_pulse_gate_init(&protocol->pulse_gate);
    protocol->mode = WHEEL_MODE_UNKNOWN;
    protocol->interface_mode = 0;
    protocol->configured_axis_override_mode = WHEEL_AXIS_OVERRIDE_MODE_NONE;
    protocol->paddle_bite_point_percent = 100;
    protocol->system_status_code = 0;
    clear(protocol->legacy_pedal_status, sizeof(protocol->legacy_pedal_status));
    clear(protocol->remote_tuning_controls, sizeof(protocol->remote_tuning_controls));
    protocol->display_rotation_angle = 0;
    protocol->button_latch_enabled = false;
    protocol->display_character_mode = false;
    protocol->display_rotation_enabled = false;
    protocol->host_capability_enabled = false;
    protocol->profile_transition_pending = false;
    protocol->system_status_pending = false;
    protocol->request_ready = false;
    protocol->request_changed = false;
    protocol->command_invalid = false;
    protocol->mode_input_invalid = false;
    protocol->selection_recovery_pending = false;
    protocol->axis_report_enabled_latched = false;
    protocol->extended_primary_released = true;
    protocol->extended_secondary_released = true;
    protocol->acknowledgement_input_active = false;
    protocol->remote_tuning_controls_pending = false;
}

/**
 * @brief Updates standard packet-family wheel output.
 *
 * Replaces the display, vibration, and legacy-axis output encoded for mode-one packets.
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
 * Replaces the display, vibration, and legacy-axis output encoded for mode-four packets.
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
 * Replaces the display, vibration, legacy-axis, motor-link restart, and report-status output
 * encoded for CRC-family packets.
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
 * @brief Selects the protocol response acknowledgement flag.
 *
 * Updates flag bit zero while preserving every other response flag.
 *
 * @param[in,out] protocol Attached-wheel protocol response to update.
 * @param[in] acknowledged True to set the acknowledgement flag; false to clear it.
 */
void wheel_protocol_set_response_acknowledged(WheelProtocol *protocol, bool acknowledged) {
    if (acknowledged) {
        protocol->response[WHEEL_PROTOCOL_FLAGS_OFFSET] |= WHEEL_PROTOCOL_RESPONSE_ACKNOWLEDGED;
    } else {
        protocol->response[WHEEL_PROTOCOL_FLAGS_OFFSET] &=
            (uint8_t)~WHEEL_PROTOCOL_RESPONSE_ACKNOWLEDGED;
    }
}

/**
 * @brief Configures attached-wheel adapter input.
 *
 * Retains adapter buttons, axes, rotary positions, profile flags, mode, connection state, and
 * pending motion used by attached-wheel packet families.
 *
 * @param[in,out] protocol Wheel protocol state to configure.
 * @param[in] adapter Attached-wheel adapter input.
 */
void wheel_protocol_set_adapter(WheelProtocol *protocol, const WheelAdapterInput *adapter) {
    protocol->adapter = *adapter;
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
 * @brief Queues a remote telemetry report for the active attached wheel.
 *
 * Mode 0x12 retains the complete payload in its paced alternate transfer. Other modes use the
 * standard attached-wheel output-report queue.
 *
 * @param[in,out] protocol Attached-wheel protocol and retained output state.
 * @param[in] payload Complete 30-byte remote telemetry report.
 * @return True when the report was retained.
 */
bool wheel_protocol_queue_remote_telemetry(
    WheelProtocol *protocol, const uint8_t payload[WHEEL_OUTPUT_REMOTE_TELEMETRY_SIZE]) {
    if (protocol == NULL || payload == NULL) {
        return false;
    }
    if (wheel_packet_alternate_applies(protocol->mode)) {
        return wheel_packet_alternate_queue_payload(&protocol->alternate_output, payload);
    }
    return wheel_output_reports_queue_remote_telemetry(&protocol->output_reports, payload);
}

/**
 * @brief Reports whether remote telemetry awaits attached-wheel transfer.
 *
 * Selects the paced alternate transfer in mode 0x12 and the standard output-report queue for
 * every other mode.
 *
 * @param[in] protocol Attached-wheel protocol state.
 * @return True while a remote telemetry report remains queued.
 */
bool wheel_protocol_remote_telemetry_pending(const WheelProtocol *protocol) {
    if (protocol == NULL) {
        return false;
    }
    return wheel_packet_alternate_applies(protocol->mode)
               ? wheel_packet_alternate_payload_pending(&protocol->alternate_output)
               : wheel_output_reports_remote_telemetry_pending(&protocol->output_reports);
}

/**
 * @brief Configures attached-wheel axis processing.
 *
 * Retains the host interface mode, configured analog-paddle mode, and current time used by
 * incoming standard, common-payload, and CRC-family controls. Updates the bite-point percentage
 * unless an adjustment is in progress.
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
 * Retains the button-latch enable and profile-transition state used while normalizing standard and
 * extended input.
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
 * @brief Selects character or raw-segment output for mode-nine wheel displays.
 *
 * Character mode translates page glyphs before the next attached-wheel response. Raw mode keeps
 * the seven-segment bit patterns used by tuning displays and all other wheel modes.
 *
 * @param[in,out] protocol Attached-wheel protocol state.
 * @param[in] enabled True to translate mode-nine glyphs to characters.
 */
void wheel_protocol_set_display_character_mode(WheelProtocol *protocol, bool enabled) {
    if (protocol != NULL) {
        protocol->display_character_mode = enabled;
    }
}

/**
 * @brief Applies one attached-wheel protocol request.
 *
 * Advances the ready-and-acknowledge handshake, selects or authenticates the requested packet
 * family, captures valid active input, and builds the corresponding response. Every in-range A5
 * mode byte is accepted; a mode without a decoder remains active with the official A6 response. An
 * out-of-range A5 value enters the unsupported phase with the invalid-command latch, while an
 * unrecognized selection is retained for the service deadline recovery path. Once the selecting
 * phase is reached, the mode command is processed without rechecking the ready bit. Scan modes
 * remain under the separate scan service.
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
        reset_acknowledgement_motion(protocol);
        if (!ready) {
            protocol->response[WHEEL_PROTOCOL_FLAGS_OFFSET] &=
                (uint8_t)~WHEEL_PROTOCOL_RESPONSE_ACKNOWLEDGED;
            protocol->phase = WHEEL_PROTOCOL_WAITING;
        } else {
            protocol->phase = WHEEL_PROTOCOL_SELECTING;
        }
        return;
    case WHEEL_PROTOCOL_SELECTING:
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
        protocol->command_invalid =
            protocol->mode_input_invalid || !wheel_protocol_message_valid(request);
        if (!protocol->command_invalid) {
            if (active_command_valid(protocol, request[0])) {
                capture_request(protocol, request);
            } else if (wheel_authentication_required(protocol->mode)) {
                protocol->command_invalid = true;
                wheel_authentication_init(&protocol->authentication, protocol->mode);
                protocol->phase = WHEEL_PROTOCOL_AUTHENTICATING;
            } else {
                protocol->command_invalid = true;
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
 * @brief Returns the current standard display-packet input.
 *
 * Exposes decoded mode-0x10 input only after a supported request has been captured.
 *
 * @param[in] protocol Wheel protocol state.
 * @return Current display-packet input, or null when unavailable.
 */
const WheelPacketDisplayInput *wheel_protocol_display_input(const WheelProtocol *protocol) {
    return protocol->request_ready && wheel_packet_display_applies(protocol->mode)
               ? &protocol->display_input
               : 0;
}

/**
 * @brief Returns the current remapped packet input.
 *
 * Exposes decoded mode-0x11 input only after a supported request has been captured.
 *
 * @param[in] protocol Wheel protocol state.
 * @return Current remapped packet input, or null when unavailable.
 */
const WheelPacketRemappedInput *wheel_protocol_remapped_input(const WheelProtocol *protocol) {
    return protocol->request_ready && wheel_packet_remapped_applies(protocol->mode)
               ? &protocol->remapped_input
               : 0;
}

/**
 * @brief Returns the current alternate packet input.
 *
 * Exposes decoded mode-0x12 input only after a supported request has been captured.
 *
 * @param[in] protocol Wheel protocol state.
 * @return Current alternate input, or null when unavailable.
 */
const WheelPacketAlternateInput *wheel_protocol_alternate_input(const WheelProtocol *protocol) {
    return protocol->request_ready && wheel_packet_alternate_applies(protocol->mode)
               ? &protocol->alternate_input
               : 0;
}

/**
 * @brief Returns the current packed packet-family input.
 *
 * Exposes decoded packed-family input only after a supported request has been captured.
 *
 * @param[in] protocol Wheel protocol state.
 * @return Current packed-family input, or null when unavailable.
 */
const WheelPacketPackedInput *wheel_protocol_packed_input(const WheelProtocol *protocol) {
    return protocol->request_ready && wheel_packet_packed_applies(protocol->mode)
               ? &protocol->packed_input
               : 0;
}

/**
 * @brief Returns the current axis-mode packet input.
 *
 * Exposes decoded and normalized input only after a supported axis-mode request has been captured.
 *
 * @param[in] protocol Wheel protocol state.
 * @return Current axis-mode input, or null when unavailable.
 */
const WheelPacketAxisModeInput *wheel_protocol_axis_mode_input(const WheelProtocol *protocol) {
    return protocol->request_ready && wheel_packet_axis_mode_applies(protocol->mode)
               ? &protocol->common_input
               : 0;
}

/**
 * @brief Returns the current extended packet-family input.
 *
 * Exposes decoded and normalized input only after a supported extended request has been captured.
 *
 * @param[in] protocol Wheel protocol state.
 * @return Current extended-family input, or null when unavailable.
 */
const WheelPacketExtendedInput *wheel_protocol_extended_input(const WheelProtocol *protocol) {
    return protocol->request_ready && wheel_packet_extended_applies(protocol->mode)
               ? &protocol->common_input
               : 0;
}

/**
 * @brief Returns the current adapter-oriented packet input.
 *
 * Exposes decoded, filtered, merged, and normalized input only after a mode 0x0C request has been
 * captured.
 *
 * @param[in] protocol Wheel protocol state.
 * @return Current adapter-oriented input, or null when unavailable.
 */
const WheelPacketAdapterInput *wheel_protocol_adapter_input(const WheelProtocol *protocol) {
    return protocol->request_ready && wheel_packet_adapter_applies(protocol->mode)
               ? &protocol->common_input
               : 0;
}

/**
 * @brief Returns the current metadata-only packet input.
 *
 * Exposes the raw axis values and report metadata only after a mode 0x1E request has been captured.
 *
 * @param[in] protocol Wheel protocol state.
 * @return Current metadata-only input, or null when unavailable.
 */
const WheelPacketMetadataInput *wheel_protocol_metadata_input(const WheelProtocol *protocol) {
    return protocol->request_ready && wheel_packet_metadata_applies(protocol->mode)
               ? &protocol->common_input
               : 0;
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
 * Selects the value from the current mode-one, mode-four, display, remapped, alternate, packed,
 * axis-mode, extended, adapter-oriented, metadata-only, or CRC-family input report.
 * Returns zero until a supported input report is ready.
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
    const WheelPacketDisplayInput *display = wheel_protocol_display_input(protocol);
    if (display != 0) {
        return display->axis_limit;
    }
    const WheelPacketRemappedInput *remapped = wheel_protocol_remapped_input(protocol);
    if (remapped != 0) {
        return remapped->axis_limit;
    }
    const WheelPacketAlternateInput *alternate = wheel_protocol_alternate_input(protocol);
    if (alternate != 0) {
        return alternate->axis_limit;
    }
    const WheelPacketPackedInput *packed = wheel_protocol_packed_input(protocol);
    if (packed != 0) {
        return packed->axis_limit;
    }
    const WheelPacketAxisModeInput *axis_mode = wheel_protocol_axis_mode_input(protocol);
    if (axis_mode != 0) {
        return axis_mode->axis_limit;
    }
    const WheelPacketExtendedInput *extended = wheel_protocol_extended_input(protocol);
    if (extended != 0) {
        return extended->axis_limit;
    }
    const WheelPacketAdapterInput *adapter = wheel_protocol_adapter_input(protocol);
    if (adapter != 0) {
        return adapter->axis_limit;
    }
    const WheelPacketMetadataInput *metadata = wheel_protocol_metadata_input(protocol);
    if (metadata != 0) {
        return metadata->axis_limit;
    }
    const WheelPacketCrcInput *crc = wheel_protocol_crc_input(protocol);
    return crc != 0 ? crc->axis_limit : 0;
}

/**
 * @brief Returns the attached wheel's secondary button byte.
 *
 * Selects the mode-button field retained by the active mode-one, mode-four, display, remapped,
 * alternate, packed, axis-mode, extended, adapter-oriented, or CRC-family input packet.
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
    const WheelPacketDisplayInput *display = wheel_protocol_display_input(protocol);
    if (display != 0) {
        return display->mode_buttons;
    }
    const WheelPacketRemappedInput *remapped = wheel_protocol_remapped_input(protocol);
    if (remapped != 0) {
        return remapped->mode_buttons;
    }
    const WheelPacketAlternateInput *alternate = wheel_protocol_alternate_input(protocol);
    if (alternate != 0) {
        return alternate->mode_buttons;
    }
    const WheelPacketPackedInput *packed = wheel_protocol_packed_input(protocol);
    if (packed != 0) {
        return packed->mode_buttons;
    }
    const WheelPacketAxisModeInput *axis_mode = wheel_protocol_axis_mode_input(protocol);
    if (axis_mode != 0) {
        return axis_mode->mode_buttons;
    }
    const WheelPacketExtendedInput *extended = wheel_protocol_extended_input(protocol);
    if (extended != 0) {
        return extended->mode_buttons;
    }
    const WheelPacketAdapterInput *adapter = wheel_protocol_adapter_input(protocol);
    if (adapter != 0) {
        return adapter->mode_buttons;
    }
    const WheelPacketCrcInput *crc = wheel_protocol_crc_input(protocol);
    return crc != 0 ? crc->mode_buttons : 0;
}

/**
 * @brief Returns the attached wheel's two primary axis-output bytes.
 *
 * Selects the normalized values from the current mode-one, mode-four, display, remapped, alternate,
 * packed, axis-mode, extended, adapter-oriented, or CRC-family input report.
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
    const WheelPacketDisplayInput *display = wheel_protocol_display_input(protocol);
    if (display != 0) {
        return display->axis_outputs;
    }
    const WheelPacketRemappedInput *remapped = wheel_protocol_remapped_input(protocol);
    if (remapped != 0) {
        return remapped->axis_outputs;
    }
    const WheelPacketAlternateInput *alternate = wheel_protocol_alternate_input(protocol);
    if (alternate != 0) {
        return alternate->axis_outputs;
    }
    const WheelPacketPackedInput *packed = wheel_protocol_packed_input(protocol);
    if (packed != 0) {
        return packed->axis_outputs;
    }
    const WheelPacketAxisModeInput *axis_mode = wheel_protocol_axis_mode_input(protocol);
    if (axis_mode != 0) {
        return axis_mode->axis_outputs;
    }
    const WheelPacketExtendedInput *extended = wheel_protocol_extended_input(protocol);
    if (extended != 0) {
        return extended->axis_outputs;
    }
    const WheelPacketAdapterInput *adapter = wheel_protocol_adapter_input(protocol);
    if (adapter != 0) {
        return adapter->axis_outputs;
    }
    const WheelPacketCrcInput *crc = wheel_protocol_crc_input(protocol);
    return crc != 0 ? crc->axis_outputs : 0;
}

/**
 * @brief Tests the official axis-report mode gate.
 *
 * The firmware exposes the retained axis-report flag only for modes 0x04, 0x06, 0x0C, 0x0E,
 * 0x0F, and 0x13 through 0x17, plus 0x1C.
 *
 * @param[in] mode Negotiated attached-wheel mode.
 * @return True when the mode can expose the axis-report flag.
 */
static bool axis_report_mode_allowed(uint8_t mode) {
    return mode == 0x04 || mode == 0x06 || mode == 0x0c || mode == 0x0e || mode == 0x0f ||
           (mode >= 0x13 && mode <= 0x17) || mode == 0x1c;
}

/**
 * @brief Reports whether the attached wheel enabled its axis report.
 *
 * Applies the official mode gate before exposing the retained global axis-report flag. Unsupported
 * modes report disabled.
 *
 * @param[in] protocol Attached-wheel protocol state.
 * @return True while the retained global axis-report flag is enabled.
 */
bool wheel_protocol_axis_report_enabled(const WheelProtocol *protocol) {
    return protocol != NULL && axis_report_mode_allowed(protocol->mode) &&
           protocol->axis_report_enabled_latched;
}

/**
 * @brief Copies the attached wheel's two 16-bit axis values.
 *
 * Selects the separately retained standard-packet values or the mode-four, display, remapped,
 * alternate, packed, axis-mode, extended, adapter-oriented, metadata-only, and CRC-family values.
 * The destination is cleared when no supported input report is ready.
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
    const WheelPacketDisplayInput *display = wheel_protocol_display_input(protocol);
    if (display != 0) {
        values[0] = display->axis_values[0];
        values[1] = display->axis_values[1];
        return true;
    }
    const WheelPacketRemappedInput *remapped = wheel_protocol_remapped_input(protocol);
    if (remapped != 0) {
        values[0] = remapped->axis_values[0];
        values[1] = remapped->axis_values[1];
        return true;
    }
    const WheelPacketAlternateInput *alternate = wheel_protocol_alternate_input(protocol);
    if (alternate != 0) {
        values[0] = alternate->axis_values[0];
        values[1] = alternate->axis_values[1];
        return true;
    }
    const WheelPacketPackedInput *packed = wheel_protocol_packed_input(protocol);
    if (packed != 0) {
        values[0] = packed->axis_values[0];
        values[1] = packed->axis_values[1];
        return true;
    }
    const WheelPacketAxisModeInput *axis_mode = wheel_protocol_axis_mode_input(protocol);
    if (axis_mode != 0) {
        values[0] = axis_mode->axis_values[0];
        values[1] = axis_mode->axis_values[1];
        return true;
    }
    const WheelPacketExtendedInput *extended = wheel_protocol_extended_input(protocol);
    if (extended != 0) {
        values[0] = extended->axis_values[0];
        values[1] = extended->axis_values[1];
        return true;
    }
    const WheelPacketAdapterInput *adapter = wheel_protocol_adapter_input(protocol);
    if (adapter != 0) {
        values[0] = adapter->axis_values[0];
        values[1] = adapter->axis_values[1];
        return true;
    }
    const WheelPacketMetadataInput *metadata = wheel_protocol_metadata_input(protocol);
    if (metadata != 0) {
        values[0] = metadata->axis_values[0];
        values[1] = metadata->axis_values[1];
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
 * Selects normalized mode-one, mode-four, display, remapped, alternate, packed, axis-mode,
 * extended, adapter-oriented, or CRC-family controls.
 * Mode-four control and control-data groups are joined in their packet order. The destination is
 * cleared when no supported input report is ready.
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
    const WheelPacketDisplayInput *display = wheel_protocol_display_input(protocol);
    if (display != 0) {
        for (uint8_t index = 0; index < WHEEL_PACKET_COMMON_CONTROL_COUNT; index++) {
            controls[index] = display->controls[index];
        }
        return true;
    }
    const WheelPacketRemappedInput *remapped = wheel_protocol_remapped_input(protocol);
    if (remapped != 0) {
        for (uint8_t index = 0; index < WHEEL_PACKET_COMMON_CONTROL_COUNT; index++) {
            controls[index] = remapped->controls[index];
        }
        return true;
    }
    const WheelPacketAlternateInput *alternate = wheel_protocol_alternate_input(protocol);
    if (alternate != 0) {
        for (uint8_t index = 0; index < WHEEL_PACKET_COMMON_CONTROL_COUNT; index++) {
            controls[index] = alternate->controls[index];
        }
        return true;
    }
    const WheelPacketPackedInput *packed = wheel_protocol_packed_input(protocol);
    if (packed != 0) {
        for (uint8_t index = 0; index < WHEEL_PACKET_PACKED_CONTROL_COUNT; index++) {
            controls[index] = packed->controls[index];
        }
        return true;
    }
    const WheelPacketAxisModeInput *axis_mode = wheel_protocol_axis_mode_input(protocol);
    if (axis_mode != 0) {
        for (uint8_t index = 0; index < WHEEL_PACKET_COMMON_CONTROL_COUNT; index++) {
            controls[index] = axis_mode->controls[index];
        }
        return true;
    }
    const WheelPacketExtendedInput *extended = wheel_protocol_extended_input(protocol);
    if (extended != 0) {
        for (uint8_t index = 0; index < WHEEL_PACKET_COMMON_CONTROL_COUNT; index++) {
            controls[index] = extended->controls[index];
        }
        return true;
    }
    const WheelPacketAdapterInput *adapter = wheel_protocol_adapter_input(protocol);
    if (adapter != 0) {
        for (uint8_t index = 0; index < WHEEL_PACKET_COMMON_CONTROL_COUNT; index++) {
            controls[index] = adapter->controls[index];
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
 * @brief Returns one queued attached-wheel axis motion direction.
 *
 * Inspects the selected protocol motion counter without consuming it.
 *
 * @param[in] protocol Attached-wheel protocol state.
 * @param[in] axis Zero-based auxiliary motion axis.
 * @return Negative one, zero, or positive one; zero for an unsupported axis.
 */
int8_t wheel_protocol_axis_motion_direction(const WheelProtocol *protocol, uint8_t axis) {
    return wheel_motion_axis_direction(&protocol->motion, axis);
}

/**
 * @brief Takes one queued attached-wheel axis motion step.
 *
 * Moves the selected protocol motion counter one position toward zero.
 *
 * @param[in,out] protocol Attached-wheel protocol state.
 * @param[in] axis Zero-based auxiliary motion axis.
 * @return Negative one, zero, or positive one; zero for an unsupported axis.
 */
int8_t wheel_protocol_take_axis_motion(WheelProtocol *protocol, uint8_t axis) {
    return wheel_motion_take_axis(&protocol->motion, axis);
}

/**
 * @brief Takes the latest attached-wheel remote-tuning controls.
 *
 * Copies the retained 30-byte control payload and clears its one-shot pending latch.
 *
 * @param[in,out] protocol Attached-wheel protocol state.
 * @param[out] output Destination for the complete control payload.
 * @return True when a pending payload was copied.
 */
bool wheel_protocol_take_remote_tuning_controls(WheelProtocol *protocol, uint8_t output[30]) {
    if (protocol == NULL || output == NULL || !protocol->remote_tuning_controls_pending) {
        return false;
    }
    for (uint8_t index = 0; index < sizeof(protocol->remote_tuning_controls); index++) {
        output[index] = protocol->remote_tuning_controls[index];
    }
    protocol->remote_tuning_controls_pending = false;
    return true;
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
 * Returns the directional, button, and mode-specific auxiliary input state captured from supported
 * requests.
 *
 * @param[in] protocol Attached-wheel protocol state.
 * @return True while an eligible input is active.
 */
bool wheel_protocol_acknowledgement_input_active(const WheelProtocol *protocol) {
    return protocol->request_ready &&
           (wheel_packet_mode_one_applies(protocol->mode) || protocol->mode == 4 ||
            wheel_packet_display_applies(protocol->mode) ||
            wheel_packet_remapped_applies(protocol->mode) ||
            wheel_packet_alternate_applies(protocol->mode) ||
            wheel_packet_packed_applies(protocol->mode) ||
            wheel_packet_axis_mode_applies(protocol->mode) ||
            wheel_packet_extended_applies(protocol->mode) ||
            wheel_packet_adapter_applies(protocol->mode) ||
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
