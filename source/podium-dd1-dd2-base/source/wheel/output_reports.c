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
    WHEEL_MODE_LEGACY_ALTERNATE = 0x0f,
    WHEEL_MODE_LEGACY_COMPATIBILITY = 0x17,
};

/**
 * @brief Initializes retained attached-wheel output reports.
 *
 * Clears all four report payloads and their pending state.
 *
 * @param[out] reports Report storage to initialize.
 */
void wheel_output_reports_init(WheelOutputReports *reports) {
    memset(reports, 0, sizeof(*reports));
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
 * Selects reports in the order 1, 2, 4, and 5, writes the report number and retained payload at
 * frame offsets one and two, and consumes the selected pending state. The caller supplies the
 * command byte and checksum.
 *
 * @param[in,out] reports Retained report payloads and pending state.
 * @param[in,out] frame Attached-wheel frame receiving the report number and payload.
 * @return True when a pending report was encoded.
 */
bool wheel_output_reports_encode_next(WheelOutputReports *reports, uint8_t *frame) {
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
    } else {
        return false;
    }

    frame[1] = report;
    memcpy(frame + 2, payload, size);
    reports->pending &= (uint8_t)~pending;
    return true;
}
