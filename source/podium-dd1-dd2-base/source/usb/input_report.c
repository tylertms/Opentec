#include "usb/input_report.h"

#include <stddef.h>
#include <stdint.h>

/**
 * @brief Encodes the primary USB input report for an operating mode.
 *
 * Selects the native Fanatec report or one of the three Logitech-compatible layouts. Native and
 * Fanatec compatibility modes share the same report layout.
 *
 * @param[in] mode Active operating-mode report selector.
 * @param[out] report Buffer that receives the selected input report.
 * @param[in] state Native and compatibility input values.
 * @return Encoded report length, or zero when the mode or arguments are invalid.
 */
uint8_t usb_input_report_encode(UsbInputReportMode mode, uint8_t report[USB_INPUT_REPORT_MAX_SIZE],
                                const UsbInputReportState *state) {
    if (report == NULL || state == NULL) {
        return 0;
    }

    switch (mode) {
    case USB_INPUT_REPORT_MODE_FANATEC:
        return fanatec_input_encode(report, &state->fanatec) ? FANATEC_INPUT_REPORT_SIZE : 0;

    case USB_INPUT_REPORT_MODE_FANATEC_COMPATIBILITY:
        return fanatec_input_compatibility_encode(report, &state->fanatec)
                   ? FANATEC_INPUT_COMPATIBILITY_REPORT_SIZE
                   : 0;

    case USB_INPUT_REPORT_MODE_DRIVING_FORCE_EX:
        return logitech_driving_force_ex_encode(report, &state->logitech)
                   ? LOGITECH_DRIVING_FORCE_EX_REPORT_SIZE
                   : 0;

    case USB_INPUT_REPORT_MODE_DRIVING_FORCE_PRO:
        return logitech_driving_force_pro_encode(report, &state->logitech)
                   ? LOGITECH_DRIVING_FORCE_PRO_REPORT_SIZE
                   : 0;

    case USB_INPUT_REPORT_MODE_G27:
        return logitech_g27_encode(report, &state->logitech) ? LOGITECH_G27_REPORT_SIZE : 0;
    }

    return 0;
}
