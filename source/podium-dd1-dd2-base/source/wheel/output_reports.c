#include "wheel/output_reports.h"

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

enum {
    WHEEL_OUTPUT_REPORT_ACTION_TWO = 0,
    WHEEL_OUTPUT_REPORT_ACTION_ONE = 1,
    WHEEL_OUTPUT_REPORT_ACTION_FOUR = 2,
    WHEEL_OUTPUT_REPORT_ACTION_FIVE = 3,
    WHEEL_OUTPUT_REPORT_ONE_PENDING = 1u << 0,
    WHEEL_OUTPUT_REPORT_TWO_PENDING = 1u << 1,
    WHEEL_OUTPUT_REPORT_FOUR_PENDING = 1u << 2,
    WHEEL_OUTPUT_REPORT_FIVE_PENDING = 1u << 3,
    WHEEL_OUTPUT_REPORT_SEVENTEEN_PENDING = 1u << 4,
    WHEEL_OUTPUT_REMOTE_TELEMETRY_PENDING = 1u << 5,
    WHEEL_OUTPUT_REPORT_SEVENTEEN_COMMAND = 3,
    WHEEL_OUTPUT_REPORT_SEVENTEEN_CHUNK_SIZE = 30,
    WHEEL_OUTPUT_REPORT_SEVENTEEN_HEADER_STEP = 0x0f,
    WHEEL_OUTPUT_REPORT_SEVENTEEN_LAST_SEQUENCE = 1,
    WHEEL_OUTPUT_REMOTE_TELEMETRY_COMMAND = 3,
    WHEEL_OUTPUT_REMOTE_TELEMETRY_TRANSMISSIONS = 3,
    WHEEL_OUTPUT_BUTTON_ILLUMINATION_COMMAND = 0x16,
    WHEEL_MODE_REMOTE_TUNING_LEGACY = 0x0e,
    WHEEL_MODE_REMOTE_TUNING_EXTENDED = 0x1c,
    WHEEL_MODE_LEGACY_ALTERNATE = 0x0f,
    WHEEL_MODE_LEGACY_COMPATIBILITY = 0x17,
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
 * Retains report 1 or 2 unconditionally except while legacy display output suppresses report 2.
 * Reports 4 and 5 are retained only for the two legacy modes or adapter mode one. Replacing a
 * retained report keeps its corresponding output pending.
 *
 * @param[in,out] reports Retained report payloads and pending state.
 * @param[in] arguments Action byte followed by the report payload.
 * @param[in] wheel_mode Negotiated attached-wheel mode.
 * @param[in] adapter_mode Attached adapter mode.
 * @param[in] display_blink_active True while the local legacy display blink phase is active.
 */
void wheel_output_reports_apply(WheelOutputReports *reports, const uint8_t *arguments,
                                uint8_t wheel_mode, uint16_t adapter_mode,
                                bool display_blink_active) {
    bool legacy_mode =
        wheel_mode == WHEEL_MODE_LEGACY_ALTERNATE || wheel_mode == WHEEL_MODE_LEGACY_COMPATIBILITY;
    switch (arguments[0]) {
    case WHEEL_OUTPUT_REPORT_ACTION_TWO:
        if (!legacy_mode || display_blink_active) {
            memcpy(reports->report_two, arguments + 1, sizeof(reports->report_two));
            reports->pending |= WHEEL_OUTPUT_REPORT_TWO_PENDING;
        }
        break;
    case WHEEL_OUTPUT_REPORT_ACTION_ONE:
        memcpy(reports->report_one, arguments + 1, sizeof(reports->report_one));
        reports->pending |= WHEEL_OUTPUT_REPORT_ONE_PENDING;
        break;
    case WHEEL_OUTPUT_REPORT_ACTION_FOUR:
        if (legacy_mode || adapter_mode == 1) {
            memcpy(reports->report_four, arguments + 1, sizeof(reports->report_four));
            reports->pending |= WHEEL_OUTPUT_REPORT_FOUR_PENDING;
        }
        break;
    case WHEEL_OUTPUT_REPORT_ACTION_FIVE:
        if (legacy_mode || adapter_mode == 1) {
            memcpy(reports->report_five, arguments + 1, sizeof(reports->report_five));
            reports->pending |= WHEEL_OUTPUT_REPORT_FIVE_PENDING;
        }
        break;
    }
}

/**
 * @brief Encodes the next pending attached-wheel output report.
 *
 * Selects reports in the order 1, 2, 4, 5, 17, remote telemetry, and changed button illumination.
 * Single-frame reports write their report number and retained payload at frame offsets one and two,
 * then consume their pending state. Report 17 emits its next segmented transfer frame. Remote
 * telemetry writes command 3 and its 30-byte payload for three successive selections. Button
 * illumination uses command 0x16 only in remote-tuning wheel modes. The caller supplies the command
 * byte and checksum.
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
    } else if ((reports->pending & WHEEL_OUTPUT_REPORT_SEVENTEEN_PENDING) != 0) {
        encode_report_seventeen(reports, frame);
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
