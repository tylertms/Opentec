#ifndef OPENTEC_BASE_USB_TUNING_STATUS_REPORT_H
#define OPENTEC_BASE_USB_TUNING_STATUS_REPORT_H

#include <stdbool.h>
#include <stdint.h>

#include "usb/device.h"
#include "usb/vendor_command.h"

/**
 * @brief Logical values encoded in a tuning-status report.
 *
 * Groups the base, auxiliary, wheel, adapter, pedal, tuning, output, and interface values needed
 * to compose the fixed vendor report.
 */
typedef struct {
    uint16_t base_status;      /**< Base status bit field. */
    uint8_t hardware_mode;     /**< Base hardware mode value. */
    uint8_t auxiliary_mode;    /**< Auxiliary input mode value. */
    uint8_t auxiliary_flags;   /**< Auxiliary input flags. */
    uint32_t auxiliary_status; /**< Auxiliary input status value. */
    uint8_t wheel_status_low;  /**< Low wheel status byte. */
    uint8_t wheel_status_high; /**< High wheel status bits; only six bits are encoded. */
    uint8_t wheel_mode;        /**< Attached-wheel mode value. */
    uint8_t button_mode;       /**< Attached-wheel button mode value. */
    uint32_t input;            /**< Current combined input value. */
    uint8_t adapter_mode;      /**< Attached adapter mode value. */
    uint8_t adapter[5];        /**< Five adapter status bytes. */
    uint8_t pedal_status;      /**< Pedal status byte. */
    uint8_t pedal_auxiliary;   /**< Pedal auxiliary status byte. */
    uint8_t pedal_axis_low;    /**< Low pedal-axis byte. */
    uint8_t pedal_axis_high;   /**< High pedal-axis byte. */
    bool tuning_available;     /**< True when tuning controls are available. */
    bool system_active;        /**< True when the system is active. */
    uint8_t force_effect;      /**< Force-effect value; encoded with the report's active bit. */
    uint8_t system_flags;      /**< System flags byte. */
    uint8_t output_status;     /**< Output status byte. */
    bool interface_gate;       /**< True when the interface gate is open. */
} UsbTuningStatusSnapshot;

/**
 * @brief Tuning-status publication and comparison state.
 *
 * Retains the last committed report and the latches controlling publication and explicit refresh.
 */
typedef struct {
    uint8_t previous[USB_DEVICE_REPORT_SIZE]; /**< Last committed report image. */
    bool enabled;                             /**< True when tuning-status reports are enabled. */
    bool dirty;          /**< True when the next enabled report must be refreshed. */
    bool previous_valid; /**< True when @ref previous contains a committed report. */
} UsbTuningStatusReportService;

/**
 * @brief Initializes tuning-status publication state.
 *
 * Disables publication and clears the previous-report image and refresh latches.
 *
 * @param[out] service Report service to initialize.
 */
void usb_tuning_status_report_service_init(UsbTuningStatusReportService *service);

/**
 * @brief Applies one tuning-status control command.
 *
 * Handles enable and refresh actions and claims commands on the tuning-status route when the
 * required arguments are present.
 *
 * @param[in,out] service Publication state to update.
 * @param[in] command Decoded tuning-status vendor command.
 * @return True when the command is valid for the tuning-status route; otherwise false.
 */
bool usb_tuning_status_report_apply_command(UsbTuningStatusReportService *service,
                                            const UsbVendorCommand *command);

/**
 * @brief Prepares a changed or refreshed tuning-status report.
 *
 * Encodes the snapshot when publication is enabled and compares it with the last committed image.
 *
 * @param[in] service Publication and comparison state.
 * @param[in] snapshot Current logical tuning-status values.
 * @param[out] output Destination for the candidate report.
 * @return True when a report should be published; otherwise false.
 */
bool usb_tuning_status_report_prepare(const UsbTuningStatusReportService *service,
                                      const UsbTuningStatusSnapshot *snapshot,
                                      uint8_t output[USB_DEVICE_REPORT_SIZE]);

/**
 * @brief Commits a published tuning-status report.
 *
 * Stores the report as the comparison baseline and clears its explicit refresh latch.
 *
 * @param[in,out] service Publication and comparison state to update.
 * @param[in] report Successfully published report.
 */
void usb_tuning_status_report_commit(UsbTuningStatusReportService *service,
                                     const uint8_t report[USB_DEVICE_REPORT_SIZE]);

#endif
