#include "wheel/output_reports.h"

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

enum {
    WHEEL_OUTPUT_REPORT_ONE_PENDING = 1u << 0,
    WHEEL_OUTPUT_REPORT_TWO_PENDING = 1u << 1,
    WHEEL_OUTPUT_REPORT_FOUR_PENDING = 1u << 2,
    WHEEL_OUTPUT_REPORT_FIVE_PENDING = 1u << 3,
    WHEEL_OUTPUT_REPORT_SEVENTEEN_PENDING = 1u << 4,
    WHEEL_OUTPUT_REMOTE_TELEMETRY_PENDING = 1u << 5,
    WHEEL_OUTPUT_REPORT_SIX_PENDING = 1u << 6,
    WHEEL_OUTPUT_DISPLAY_COMMAND_PENDING = 1u << 7,
    WHEEL_OUTPUT_REPORT_SEVENTEEN_COMMAND = 3,
    WHEEL_OUTPUT_REPORT_SEVENTEEN_CHUNK_SIZE = 30,
    WHEEL_OUTPUT_REPORT_SEVENTEEN_HEADER_STEP = 0x0f,
    WHEEL_OUTPUT_REPORT_SEVENTEEN_LAST_SEQUENCE = 1,
    WHEEL_OUTPUT_REMOTE_TELEMETRY_COMMAND = 3,
    WHEEL_OUTPUT_REMOTE_TELEMETRY_TRANSMISSIONS = 3,
    WHEEL_OUTPUT_DISPLAY_COMMAND_REPORT_ID = 0xa6,
    WHEEL_OUTPUT_DISPLAY_COMMAND_PACKET_TYPE = 0x82,
    WHEEL_OUTPUT_INTERFACE_PRESENTATION_FIRST_MODE = 1,
    WHEEL_OUTPUT_INTERFACE_PRESENTATION_LAST_MODE = 3,
    WHEEL_OUTPUT_INTERFACE_PRESENTATION_FIRST_COMMAND = 0x20,
    WHEEL_OUTPUT_INTERFACE_PRESENTATION_TRANSMISSIONS = 3,
    WHEEL_OUTPUT_INTERFACE_PRESENTATION_DISPLAY_MODE = 3,
    WHEEL_OUTPUT_INTERFACE_PRESENTATION_DISPLAY_COMMAND = 1,
    WHEEL_OUTPUT_BUTTON_ILLUMINATION_COMMAND = 0x16,
    WHEEL_DISPLAY_TORQUE_KEY_PROMPT = 0x1a,
    WHEEL_DISPLAY_TORQUE_KEY_CONFIRMED = 0x28,
    WHEEL_DISPLAY_ENABLE_TORQUE_PROMPT = 0x29,
    WHEEL_DISPLAY_ENABLE_TORQUE_CONFIRMED = 0x2a,
    WHEEL_DISPLAY_TORQUE_KEY_PROMPT_PENDING = 1u << 0,
    WHEEL_DISPLAY_TORQUE_KEY_CONFIRMED_PENDING = 1u << 1,
    WHEEL_DISPLAY_ENABLE_TORQUE_PROMPT_PENDING = 1u << 2,
    WHEEL_DISPLAY_ENABLE_TORQUE_CONFIRMED_PENDING = 1u << 3,
    WHEEL_MODE_REMOTE_TUNING_LEGACY = 0x0e,
    WHEEL_MODE_REMOTE_TUNING_EXTENDED = 0x1c,
    WHEEL_MODE_LEGACY_ALTERNATE = 0x0f,
    WHEEL_MODE_LEGACY_COMPATIBILITY = 0x17,
    WHEEL_INTERFACE_MODE_BUTTON_CHORD = 0x9000,
    WHEEL_INTERFACE_MODE_TOGGLE_DELAY_MS = 200,
};

/**
 * @brief Initializes retained attached-wheel output reports.
 *
 * Clears all retained report payloads, transfer sequence, and pending state.
 *
 * @param[out] reports Report storage to initialize.
 */
void wheel_output_reports_init(WheelOutputReports *reports) {
    memset(reports, 0, sizeof(*reports));
    wheel_interface_catalog_init(&reports->interface_catalog);
}

/**
 * @brief Expands a compact host mask into an attached-wheel output report.
 *
 * Treats the four-byte input as a little-endian stream of three-bit groups. Each group expands
 * into a 16-bit value whose three bands use masks 0x1F00, 0xE007, and 0x00F8. Report one consumes
 * six groups and report two consumes nine. Legacy wheel modes suppress report two while the
 * interface gate is closed.
 *
 * @param[in,out] reports Retained report payloads and pending state.
 * @param[in] report Attached-wheel report number, either one or two.
 * @param[in] packed Four-byte compact group mask.
 * @param[in] wheel_mode Negotiated attached-wheel mode.
 * @return True when a supported report was expanded and queued.
 */
bool wheel_output_reports_queue_packed(WheelOutputReports *reports, uint8_t report,
                                       const uint8_t packed[4], uint8_t wheel_mode) {
    if (reports == NULL || packed == NULL || (report != 1 && report != 2)) {
        return false;
    }
    bool legacy_mode =
        wheel_mode == WHEEL_MODE_LEGACY_ALTERNATE || wheel_mode == WHEEL_MODE_LEGACY_COMPATIBILITY;
    if (report == 2 && legacy_mode && !reports->interface_mode_gate) {
        return false;
    }

    uint32_t groups = (uint32_t)packed[0] | (uint32_t)packed[1] << 8 | (uint32_t)packed[2] << 16 |
                      (uint32_t)packed[3] << 24;
    uint8_t *payload = report == 1 ? reports->report_one : reports->report_two;
    uint8_t group_count =
        report == 1 ? WHEEL_OUTPUT_REPORT_ONE_SIZE / 2 : WHEEL_OUTPUT_REPORT_TWO_SIZE / 2;
    for (uint8_t index = 0; index < group_count; index++) {
        uint8_t bits = (uint8_t)(groups >> (index * 3u)) & 0x07u;
        uint16_t expanded = 0;
        if ((bits & 0x01u) != 0) {
            expanded |= 0x1f00u;
        }
        if ((bits & 0x02u) != 0) {
            expanded |= 0xe007u;
        }
        if ((bits & 0x04u) != 0) {
            expanded |= 0x00f8u;
        }
        payload[index * 2u] = (uint8_t)expanded;
        payload[index * 2u + 1u] = (uint8_t)(expanded >> 8);
    }
    reports->pending |=
        report == 1 ? WHEEL_OUTPUT_REPORT_ONE_PENDING : WHEEL_OUTPUT_REPORT_TWO_PENDING;
    return true;
}

/**
 * @brief Queues a segmented attached-wheel report 17 transfer.
 *
 * Retains all 61 host-provided bytes, resets the transfer sequence, and makes the transfer the
 * lowest-priority pending attached-wheel output report.
 *
 * @param[in,out] reports Retained report payloads, sequence, and pending state.
 * @param[in] payload Complete 61-byte report payload.
 */
void wheel_output_reports_queue_seventeen(
    WheelOutputReports *reports, const uint8_t payload[WHEEL_OUTPUT_REPORT_SEVENTEEN_SIZE]) {
    memcpy(reports->report_seventeen, payload, sizeof(reports->report_seventeen));
    reports->report_seventeen_sequence = 0;
    reports->pending |= WHEEL_OUTPUT_REPORT_SEVENTEEN_PENDING;
}

/**
 * @brief Queues a native attached-wheel display command.
 *
 * Retains the newest one-byte command for a type-0x82 configuration packet. The command is
 * scheduled after direct reports and report 17, and before remote telemetry.
 *
 * @param[in,out] reports Retained report payloads and pending state.
 * @param[in] command Native wheel display command.
 */
void wheel_output_reports_queue_display_command(WheelOutputReports *reports, uint8_t command) {
    if (reports == NULL) {
        return;
    }
    reports->display_command = command;
    reports->pending |= WHEEL_OUTPUT_DISPLAY_COMMAND_PENDING;
}

/**
 * @brief Queues a native attached-wheel display notification.
 *
 * Latches recognized Torque Key and force-output prompt or confirmation commands independently.
 * Prompts repeat in remote-tuning wheel modes until the matching confirmation is sent;
 * confirmations consume both themselves and their corresponding prompt.
 *
 * @param[in,out] reports Retained display-notification state.
 * @param[in] command Native prompt or confirmation command.
 */
void wheel_output_reports_queue_display_notification(WheelOutputReports *reports, uint8_t command) {
    if (reports == NULL) {
        return;
    }
    switch (command) {
    case WHEEL_DISPLAY_TORQUE_KEY_PROMPT:
        reports->display_notifications_pending |= WHEEL_DISPLAY_TORQUE_KEY_PROMPT_PENDING;
        break;
    case WHEEL_DISPLAY_TORQUE_KEY_CONFIRMED:
        reports->display_notifications_pending |= WHEEL_DISPLAY_TORQUE_KEY_CONFIRMED_PENDING;
        break;
    case WHEEL_DISPLAY_ENABLE_TORQUE_PROMPT:
        reports->display_notifications_pending |= WHEEL_DISPLAY_ENABLE_TORQUE_PROMPT_PENDING;
        break;
    case WHEEL_DISPLAY_ENABLE_TORQUE_CONFIRMED:
        reports->display_notifications_pending |= WHEEL_DISPLAY_ENABLE_TORQUE_CONFIRMED_PENDING;
        break;
    default:
        break;
    }
}

/**
 * @brief Activates one legacy host-interface presentation on the attached wheel.
 *
 * Replaces any earlier direct presentation cycle. Modes one through three emit the empty command
 * records 0x20 through 0x22 three times, with mode three also queuing display command one. Modes
 * four and five restart the remote-tuning record and indexed-help catalog streams.
 *
 * @param[in,out] reports Retained attached-wheel output state.
 * @param[in] mode Requested legacy host-interface presentation mode.
 */
void wheel_output_reports_activate_interface_presentation(WheelOutputReports *reports,
                                                          uint8_t mode) {
    if (reports == NULL) {
        return;
    }
    reports->interface_presentation_command = 0;
    reports->interface_presentation_transmissions = 0;
    if (wheel_interface_catalog_activate(&reports->interface_catalog, mode)) {
        return;
    }
    if (mode < WHEEL_OUTPUT_INTERFACE_PRESENTATION_FIRST_MODE ||
        mode > WHEEL_OUTPUT_INTERFACE_PRESENTATION_LAST_MODE) {
        return;
    }
    reports->interface_presentation_command =
        (uint8_t)(WHEEL_OUTPUT_INTERFACE_PRESENTATION_FIRST_COMMAND + mode - 1u);
    reports->interface_presentation_transmissions =
        WHEEL_OUTPUT_INTERFACE_PRESENTATION_TRANSMISSIONS;
    if (mode == WHEEL_OUTPUT_INTERFACE_PRESENTATION_DISPLAY_MODE) {
        wheel_output_reports_queue_display_command(
            reports, WHEEL_OUTPUT_INTERFACE_PRESENTATION_DISPLAY_COMMAND);
    }
}

/**
 * @brief Queues one remote telemetry report for the attached wheel.
 *
 * Retains the complete 30-byte report only when no earlier telemetry report is pending. An
 * accepted report is scheduled after direct reports and report 17 for three transmissions.
 *
 * @param[in,out] reports Retained report payloads and pending state.
 * @param[in] payload Complete remote telemetry report.
 * @return True when the report was retained.
 */
bool wheel_output_reports_queue_remote_telemetry(
    WheelOutputReports *reports, const uint8_t payload[WHEEL_OUTPUT_REMOTE_TELEMETRY_SIZE]) {
    if (reports == NULL || payload == NULL ||
        (reports->pending & WHEEL_OUTPUT_REMOTE_TELEMETRY_PENDING) != 0) {
        return false;
    }

    memcpy(reports->remote_telemetry, payload, sizeof(reports->remote_telemetry));
    reports->remote_telemetry_transmissions = WHEEL_OUTPUT_REMOTE_TELEMETRY_TRANSMISSIONS;
    reports->pending |= WHEEL_OUTPUT_REMOTE_TELEMETRY_PENDING;
    return true;
}

/**
 * @brief Reports whether remote telemetry awaits attached-wheel transfer.
 *
 * Tests the remote telemetry pending state without consuming a transmission.
 *
 * @param[in] reports Retained report payloads and pending state.
 * @return True while a remote telemetry report remains queued.
 */
bool wheel_output_reports_remote_telemetry_pending(const WheelOutputReports *reports) {
    return reports != NULL && (reports->pending & WHEEL_OUTPUT_REMOTE_TELEMETRY_PENDING) != 0;
}

/**
 * @brief Selects the attached-wheel button illumination state.
 *
 * Normalizes the active profile flag and retains it until a compatible wheel receives the changed
 * value through its profile-mode command.
 *
 * @param[in,out] reports Retained report payloads and profile-mode state.
 * @param[in] enabled True to enable attached-wheel button illumination.
 */
void wheel_output_reports_set_button_illumination(WheelOutputReports *reports, bool enabled) {
    reports->button_illumination = enabled;
}

/**
 * @brief Selects the legacy wheel interface-mode gate.
 *
 * Normalizes the requested state used by legacy report-two forwarding and the idle attached-wheel
 * response flag.
 *
 * @param[in,out] reports Retained report and interface state.
 * @param[in] enabled Requested interface-mode gate state.
 */
void wheel_output_reports_set_interface_mode_gate(WheelOutputReports *reports, bool enabled) {
    if (reports != NULL) {
        reports->interface_mode_gate = enabled;
    }
}

/**
 * @brief Updates the legacy wheel interface-mode gate from its local button chord.
 *
 * Toggles the gate once when secondary-button mask 0x9000 is newly held after the strict
 * 200-millisecond deadline. Releasing either button rearms the chord without changing the gate.
 *
 * @param[in,out] reports Retained report, gate, latch, and deadline state.
 * @param[in] secondary_buttons Current attached-wheel secondary buttons.
 * @param[in] now_ms Current monotonic time in milliseconds.
 */
void wheel_output_reports_update_interface_mode_gate(WheelOutputReports *reports,
                                                     uint16_t secondary_buttons, uint32_t now_ms) {
    if (reports == NULL) {
        return;
    }
    if ((secondary_buttons & WHEEL_INTERFACE_MODE_BUTTON_CHORD) !=
        WHEEL_INTERFACE_MODE_BUTTON_CHORD) {
        reports->interface_mode_button_latched = false;
        return;
    }
    if (!reports->interface_mode_button_latched &&
        (int32_t)(now_ms - reports->interface_mode_toggle_deadline_ms) > 0) {
        reports->interface_mode_gate = !reports->interface_mode_gate;
        reports->interface_mode_toggle_deadline_ms = now_ms + WHEEL_INTERFACE_MODE_TOGGLE_DELAY_MS;
        reports->interface_mode_button_latched = true;
    }
}

/**
 * @brief Returns the legacy wheel interface-mode gate.
 *
 * Exposes the normalized state without modifying its button latch or deadline.
 *
 * @param[in] reports Retained report and interface state.
 * @return True while the interface-mode gate is open.
 */
bool wheel_output_reports_interface_mode_gate(const WheelOutputReports *reports) {
    return reports != NULL && reports->interface_mode_gate;
}

/**
 * @brief Encodes the next report 17 transfer segment.
 *
 * Writes command 3 and one 30-byte payload half. Sequence zero advances the retained first byte by
 * 0x0F before sending bytes 0 through 29, and sequence one sends bytes 30 through 59. A subsequent
 * call clears the pending state, restarts at sequence zero, and emits the restarted first half.
 *
 * @param[in,out] reports Retained report payload, sequence, and pending state.
 * @param[in,out] frame Attached-wheel frame receiving the command and payload segment.
 */
static void encode_report_seventeen(WheelOutputReports *reports, uint8_t *frame) {
    if (reports->report_seventeen_sequence > WHEEL_OUTPUT_REPORT_SEVENTEEN_LAST_SEQUENCE) {
        reports->report_seventeen_sequence = 0;
        reports->pending &= (uint8_t)~WHEEL_OUTPUT_REPORT_SEVENTEEN_PENDING;
    }

    const uint8_t *payload = reports->report_seventeen;
    frame[1] = WHEEL_OUTPUT_REPORT_SEVENTEEN_COMMAND;
    if (reports->report_seventeen_sequence == 0) {
        reports->report_seventeen[0] += WHEEL_OUTPUT_REPORT_SEVENTEEN_HEADER_STEP;
    } else {
        payload += WHEEL_OUTPUT_REPORT_SEVENTEEN_CHUNK_SIZE;
    }
    memcpy(frame + 2, payload, WHEEL_OUTPUT_REPORT_SEVENTEEN_CHUNK_SIZE);
    reports->report_seventeen_sequence++;
}

/**
 * @brief Applies an opcode-one attached-wheel output report.
 *
 * Retains report 1 or 2 unconditionally except while the legacy interface gate suppresses report 2.
 * Reports 4 and 5 are retained only for the two legacy modes or adapter mode one. Replacing a
 * retained report keeps its corresponding output pending.
 *
 * @param[in,out] reports Retained report payloads and pending state.
 * @param[in] arguments Action byte followed by the report payload.
 * @param[in] wheel_mode Negotiated attached-wheel mode.
 * @param[in] adapter_mode Attached adapter mode.
 * @return True when the selected report was retained.
 */
bool wheel_output_reports_apply(WheelOutputReports *reports, const uint8_t *arguments,
                                uint8_t wheel_mode, uint16_t adapter_mode) {
    if (reports == NULL || arguments == NULL) {
        return false;
    }
    bool legacy_mode =
        wheel_mode == WHEEL_MODE_LEGACY_ALTERNATE || wheel_mode == WHEEL_MODE_LEGACY_COMPATIBILITY;
    switch (arguments[0]) {
    case WHEEL_OUTPUT_REPORT_ACTION_TWO:
        if (!legacy_mode || reports->interface_mode_gate) {
            memcpy(reports->report_two, arguments + 1, sizeof(reports->report_two));
            reports->pending |= WHEEL_OUTPUT_REPORT_TWO_PENDING;
            return true;
        }
        return false;
    case WHEEL_OUTPUT_REPORT_ACTION_ONE:
        memcpy(reports->report_one, arguments + 1, sizeof(reports->report_one));
        reports->pending |= WHEEL_OUTPUT_REPORT_ONE_PENDING;
        return true;
    case WHEEL_OUTPUT_REPORT_ACTION_FOUR:
        if (legacy_mode || adapter_mode == 1) {
            memcpy(reports->report_four, arguments + 1, sizeof(reports->report_four));
            reports->pending |= WHEEL_OUTPUT_REPORT_FOUR_PENDING;
            return true;
        }
        return false;
    case WHEEL_OUTPUT_REPORT_ACTION_FIVE:
        if (legacy_mode || adapter_mode == 1) {
            memcpy(reports->report_five, arguments + 1, sizeof(reports->report_five));
            reports->pending |= WHEEL_OUTPUT_REPORT_FIVE_PENDING;
            return true;
        }
        return false;
    default:
        return false;
    }
}

/**
 * @brief Queues attached-wheel report six.
 *
 * Replaces the first two bytes of the retained report-four payload and queues that shared 25-byte
 * payload under report number six. A pending report four observes the replacement before it is
 * sent.
 *
 * @param[in,out] reports Retained report payloads and pending state.
 * @param[in] first First shared report byte.
 * @param[in] second Second shared report byte.
 */
void wheel_output_reports_queue_six(WheelOutputReports *reports, uint8_t first, uint8_t second) {
    if (reports == NULL) {
        return;
    }
    reports->report_four[0] = first;
    reports->report_four[1] = second;
    reports->pending |= WHEEL_OUTPUT_REPORT_SIX_PENDING;
}

/**
 * @brief Encodes the next pending attached-wheel output report.
 *
 * In remote-tuning wheel modes, selects prompt and confirmation notifications before all other
 * work. It then selects reports in the order 1, 2, 4, 5, 6, 17, host-interface catalogs, native
 * display command, remote telemetry, and changed button illumination. Single-frame reports write
 * their report number and retained payload at frame offsets one and two, then consume their pending
 * state. Reports four and six use the same 25-byte payload. Report 17 emits its next segmented
 * transfer frame. Catalogs emit one definition or indexed-help chunk. Remote telemetry writes
 * command 3 and its 30-byte payload for three successive selections. Button illumination uses
 * command 0x16 only in remote-tuning wheel modes. The caller supplies the checksum.
 *
 * @param[in,out] reports Retained report payloads and pending state.
 * @param[in] wheel_mode Negotiated attached-wheel mode.
 * @param[in,out] frame Attached-wheel frame receiving the report number and payload.
 * @return True when a pending report was encoded.
 */
bool wheel_output_reports_encode_next(WheelOutputReports *reports, uint8_t wheel_mode,
                                      uint8_t *frame) {
    uint8_t report;
    const uint8_t *payload;
    uint8_t size;
    uint8_t pending;

    if ((wheel_mode == WHEEL_MODE_REMOTE_TUNING_LEGACY ||
         wheel_mode == WHEEL_MODE_REMOTE_TUNING_EXTENDED) &&
        reports->display_notifications_pending != 0) {
        uint8_t command;
        if ((reports->display_notifications_pending &
             WHEEL_DISPLAY_ENABLE_TORQUE_CONFIRMED_PENDING) != 0) {
            command = WHEEL_DISPLAY_ENABLE_TORQUE_CONFIRMED;
            reports->display_notifications_pending &=
                (uint8_t)~(WHEEL_DISPLAY_ENABLE_TORQUE_CONFIRMED_PENDING |
                           WHEEL_DISPLAY_ENABLE_TORQUE_PROMPT_PENDING);
        } else if ((reports->display_notifications_pending &
                    WHEEL_DISPLAY_TORQUE_KEY_CONFIRMED_PENDING) != 0) {
            command = WHEEL_DISPLAY_TORQUE_KEY_CONFIRMED;
            reports->display_notifications_pending &=
                (uint8_t)~(WHEEL_DISPLAY_TORQUE_KEY_CONFIRMED_PENDING |
                           WHEEL_DISPLAY_TORQUE_KEY_PROMPT_PENDING);
        } else if ((reports->display_notifications_pending &
                    WHEEL_DISPLAY_ENABLE_TORQUE_PROMPT_PENDING) != 0) {
            command = WHEEL_DISPLAY_ENABLE_TORQUE_PROMPT;
        } else {
            command = WHEEL_DISPLAY_TORQUE_KEY_PROMPT;
        }
        memset(frame + 1, 0, 31);
        frame[0] = WHEEL_OUTPUT_DISPLAY_COMMAND_REPORT_ID;
        frame[1] = WHEEL_OUTPUT_DISPLAY_COMMAND_PACKET_TYPE;
        frame[2] = command;
        return true;
    }
    if ((reports->pending & WHEEL_OUTPUT_REPORT_ONE_PENDING) != 0) {
        report = 1;
        payload = reports->report_one;
        size = sizeof(reports->report_one);
        pending = WHEEL_OUTPUT_REPORT_ONE_PENDING;
    } else if ((reports->pending & WHEEL_OUTPUT_REPORT_TWO_PENDING) != 0) {
        report = 2;
        payload = reports->report_two;
        size = sizeof(reports->report_two);
        pending = WHEEL_OUTPUT_REPORT_TWO_PENDING;
    } else if ((reports->pending & WHEEL_OUTPUT_REPORT_FOUR_PENDING) != 0) {
        report = 4;
        payload = reports->report_four;
        size = sizeof(reports->report_four);
        pending = WHEEL_OUTPUT_REPORT_FOUR_PENDING;
    } else if ((reports->pending & WHEEL_OUTPUT_REPORT_FIVE_PENDING) != 0) {
        report = 5;
        payload = reports->report_five;
        size = sizeof(reports->report_five);
        pending = WHEEL_OUTPUT_REPORT_FIVE_PENDING;
    } else if ((reports->pending & WHEEL_OUTPUT_REPORT_SIX_PENDING) != 0) {
        report = 6;
        payload = reports->report_four;
        size = sizeof(reports->report_four);
        pending = WHEEL_OUTPUT_REPORT_SIX_PENDING;
    } else if ((reports->pending & WHEEL_OUTPUT_REPORT_SEVENTEEN_PENDING) != 0) {
        encode_report_seventeen(reports, frame);
        return true;
    } else if (wheel_interface_catalog_encode_next(&reports->interface_catalog, wheel_mode,
                                                   frame)) {
        return true;
    } else if (reports->interface_presentation_transmissions != 0) {
        memset(frame + 1, 0, 31);
        frame[1] = reports->interface_presentation_command;
        reports->interface_presentation_transmissions--;
        return true;
    } else if ((reports->pending & WHEEL_OUTPUT_DISPLAY_COMMAND_PENDING) != 0) {
        memset(frame + 1, 0, 31);
        frame[0] = WHEEL_OUTPUT_DISPLAY_COMMAND_REPORT_ID;
        frame[1] = WHEEL_OUTPUT_DISPLAY_COMMAND_PACKET_TYPE;
        frame[2] = reports->display_command;
        reports->pending &= (uint8_t)~WHEEL_OUTPUT_DISPLAY_COMMAND_PENDING;
        return true;
    } else if ((reports->pending & WHEEL_OUTPUT_REMOTE_TELEMETRY_PENDING) != 0) {
        frame[1] = WHEEL_OUTPUT_REMOTE_TELEMETRY_COMMAND;
        memcpy(frame + 2, reports->remote_telemetry, sizeof(reports->remote_telemetry));
        reports->remote_telemetry_transmissions--;
        if (reports->remote_telemetry_transmissions == 0) {
            reports->pending &= (uint8_t)~WHEEL_OUTPUT_REMOTE_TELEMETRY_PENDING;
        }
        return true;
    } else if ((wheel_mode == WHEEL_MODE_REMOTE_TUNING_LEGACY ||
                wheel_mode == WHEEL_MODE_REMOTE_TUNING_EXTENDED) &&
               reports->button_illumination != reports->sent_button_illumination) {
        frame[1] = WHEEL_OUTPUT_BUTTON_ILLUMINATION_COMMAND;
        frame[2] = reports->button_illumination ? 1 : 0;
        reports->sent_button_illumination = reports->button_illumination;
        return true;
    } else {
        return false;
    }

    frame[1] = report;
    memcpy(frame + 2, payload, size);
    reports->pending &= (uint8_t)~pending;
    return true;
}
