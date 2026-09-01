#ifndef OPENTEC_BASE_USB_INPUT_REPORT_H
#define OPENTEC_BASE_USB_INPUT_REPORT_H

#include <stdint.h>

#include "usb/fanatec_input.h"
#include "usb/logitech_input.h"

/** @brief Storage size required for any supported primary USB input report. */
enum {
    USB_INPUT_REPORT_MAX_SIZE = FANATEC_INPUT_REPORT_SIZE, /**< Maximum supported report size. */
};

/** @brief Supported primary USB input report layouts. */
typedef enum {
    USB_INPUT_REPORT_MODE_FANATEC = 0,               /**< Native Fanatec report layout. */
    USB_INPUT_REPORT_MODE_FANATEC_COMPATIBILITY = 1, /**< Fanatec compatibility layout. */
    USB_INPUT_REPORT_MODE_DRIVING_FORCE_EX = 2,      /**< Logitech Driving Force EX layout. */
    USB_INPUT_REPORT_MODE_DRIVING_FORCE_PRO = 3,     /**< Logitech Driving Force Pro layout. */
    USB_INPUT_REPORT_MODE_G27 = 4,                   /**< Logitech G27 layout. */
} UsbInputReportMode;

/** @brief Input values for the supported native and compatibility layouts. */
typedef struct {
    fanatec_input_state fanatec; /**< Native Fanatec input values. */
    LogitechInputState logitech; /**< Logitech compatibility input values. */
} UsbInputReportState;

/**
 * @brief Encodes a primary USB input report.
 *
 * Selects the report encoder for the requested layout and writes its bytes into the caller-owned
 * buffer. The Fanatec compatibility layout uses the Fanatec input state; all Logitech layouts use
 * the Logitech input state.
 *
 * @param[in] mode Requested primary input report layout.
 * @param[out] report Buffer with room for USB_INPUT_REPORT_MAX_SIZE bytes.
 * @param[in] state Input values for the selected layout.
 * @return The encoded report length when the mode and pointers are valid and encoding succeeds;
 * otherwise zero.
 */
uint8_t usb_input_report_encode(UsbInputReportMode mode, uint8_t report[USB_INPUT_REPORT_MAX_SIZE],
                                const UsbInputReportState *state);

#endif
