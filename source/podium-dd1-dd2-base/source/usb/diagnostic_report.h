#ifndef OPENTEC_BASE_USB_DIAGNOSTIC_REPORT_H
#define OPENTEC_BASE_USB_DIAGNOSTIC_REPORT_H

#include <stdbool.h>
#include <stdint.h>

#include "usb/device.h"
#include "usb/vendor_command.h"
#include "wheel/status_service.h"

/**
 * @brief Attached-wheel telemetry and base-motor torque carried in the diagnostic motor block.
 *
 * The identity, temperature, and runtime fields are populated from the attached-wheel auxiliary
 * bus target 0xf0. The torque field remains the base motor live-position sample.
 */
typedef struct {
    uint8_t version;             /**< Low six bits of the attached-wheel version value. */
    int8_t initial_status;       /**< Signed status byte from the attached-wheel identity probe. */
    uint16_t motor_temperature;  /**< Raw signed temperature from auxiliary register 0x12. */
    uint16_t driver_temperature; /**< Raw signed temperature from auxiliary register 0x13. */
    uint8_t reserved[2];         /**< Reserved diagnostic motor-block bytes, normally zero. */
    uint32_t runtime_seconds;    /**< Runtime counter from auxiliary register 0x11. */
    uint16_t motor_torque;       /**< Raw torque sample from the base motor live-position frame. */
} UsbDiagnosticMotorState;

/** @brief Cooling phase and threshold values carried by the diagnostic report. */
typedef struct {
    uint8_t phase;                  /**< Current cooling phase. */
    uint8_t output_duty_percent;    /**< Cooling output duty cycle percentage. */
    int8_t primary_delay_seconds;   /**< Primary cooling delay in seconds. */
    int8_t secondary_delay_seconds; /**< Secondary cooling delay in seconds. */
    int8_t low_threshold_offset;    /**< Low cooling threshold offset. */
    int8_t high_threshold_offset;   /**< High cooling threshold offset. */
} UsbDiagnosticCoolingState;

/** @brief Primary and secondary PWM duty values carried by the diagnostic report. */
typedef struct {
    uint8_t secondary_duty_percent; /**< Secondary PWM duty cycle percentage. */
    uint8_t primary_duty_percent;   /**< Primary PWM duty cycle percentage. */
} UsbDiagnosticPwmState;

/** @brief Primary and secondary fan-speed values carried by the diagnostic report. */
typedef struct {
    uint16_t primary;   /**< Primary fan speed in revolutions per minute. */
    uint16_t secondary; /**< Secondary fan speed in revolutions per minute. */
} UsbDiagnosticPulseState;

/** @brief Auxiliary direction and position values carried by the diagnostic report. */
typedef struct {
    uint8_t direction; /**< Auxiliary motion direction. */
    uint16_t position; /**< Auxiliary position value. */
} UsbDiagnosticAuxiliaryPosition;

/** @brief Logical values assembled into one diagnostic vendor report. */
typedef struct {
    uint8_t base_mode;                 /**< Active base operating mode. */
    uint16_t resistance_values[2];     /**< Native primary and secondary resistance values. */
    uint32_t system_seconds;           /**< System uptime in seconds. */
    uint32_t transport_error_count;    /**< Count of transport errors. */
    UsbDiagnosticMotorState motor;     /**< F0 attached-wheel telemetry and base motor torque. */
    WheelStatusSnapshot wheel_status;  /**< Attached-wheel status values. */
    UsbDiagnosticCoolingState cooling; /**< Cooling diagnostic values. */
    UsbDiagnosticPwmState pwm;         /**< PWM diagnostic values. */
    UsbDiagnosticPulseState pulse;     /**< Pulse diagnostic values. */
    UsbDiagnosticAuxiliaryPosition auxiliary_position; /**< Auxiliary position values. */
    int32_t wheel_position;                            /**< Wheel position value. */
    int32_t wheel_velocity;                            /**< Wheel velocity value. */
} UsbDiagnosticSnapshot;

/** @brief Enable, change-detection, and retained-image state for diagnostic reports. */
typedef struct {
    uint8_t previous[USB_DEVICE_REPORT_SIZE]; /**< Last committed diagnostic report. */
    bool enabled;                             /**< True when diagnostic publication is enabled. */
    bool dirty;          /**< True when the next enabled snapshot must be published. */
    bool previous_valid; /**< True when previous contains a committed report. */
} UsbDiagnosticReportService;

/**
 * @brief Initializes diagnostic report publication state.
 *
 * Disables publication and clears the forced-refresh flag and previous report image.
 *
 * @param[out] service Diagnostic report service to initialize.
 */
void usb_diagnostic_report_service_init(UsbDiagnosticReportService *service);

/**
 * @brief Applies a vendor diagnostic-report command.
 *
 * Updates enable or refresh state for commands addressed to the diagnostic snapshot route.
 *
 * @param[in,out] service Diagnostic report service receiving the request.
 * @param[in] command Decoded vendor command and arguments.
 * @return True when the command uses the diagnostic snapshot route; otherwise false.
 */
bool usb_diagnostic_report_apply_command(UsbDiagnosticReportService *service,
                                         const UsbVendorCommand *command);

/**
 * @brief Prepares a changed or explicitly refreshed diagnostic report.
 *
 * Encodes the current snapshot and compares it with the last committed report while publication is
 * enabled.
 *
 * @param[in] service Diagnostic report publication state.
 * @param[in] snapshot Logical diagnostic values to encode.
 * @param[out] output Encoded diagnostic report.
 * @return True when the caller should submit the encoded report; otherwise false.
 */
bool usb_diagnostic_report_prepare(const UsbDiagnosticReportService *service,
                                   const UsbDiagnosticSnapshot *snapshot,
                                   uint8_t output[USB_DEVICE_REPORT_SIZE]);

/**
 * @brief Commits a submitted diagnostic report.
 *
 * Retains the exact report for change detection and clears the forced-refresh flag.
 *
 * @param[in,out] service Diagnostic report service completing publication.
 * @param[in] report Submitted diagnostic report.
 */
void usb_diagnostic_report_commit(UsbDiagnosticReportService *service,
                                  const uint8_t report[USB_DEVICE_REPORT_SIZE]);

#endif
