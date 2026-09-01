#ifndef OPENTEC_BASE_WHEEL_CENTER_CAPTURE_H
#define OPENTEC_BASE_WHEEL_CENTER_CAPTURE_H

#include <stdbool.h>
#include <stdint.h>

#include "usb/output_command.h"

/**
 * @brief Result of applying a host wheel-center capture command.
 *
 * The action distinguishes unrelated reports, recognized rate-limited commands, and commands
 * that may request a new center capture.
 */
typedef enum {
    WHEEL_CENTER_CAPTURE_UNHANDLED, /**< The output report is not a center-capture command. */
    WHEEL_CENTER_CAPTURE_HANDLED,   /**< The command is recognized but rate limited. */
    WHEEL_CENTER_CAPTURE_REQUESTED, /**< The command is recognized and capture should be attempted.
                                     */
} WheelCenterCaptureAction;

/**
 * @brief Host wheel-center capture command timing state.
 *
 * Deadlines rate-limit capture requests and result notices using the platform monotonic clock.
 */
typedef struct {
    uint32_t next_capture_ms;      /**< Earliest time at which another capture may be requested. */
    uint32_t next_notification_ms; /**< Deadline after which another result notice may be queued. */
} WheelCenterCaptureCommand;

/**
 * @brief Initializes host wheel-center capture command state.
 *
 * Clears both deadlines so the first matching command is accepted immediately; result notices use
 * a strict-after-deadline comparison.
 *
 * @param[out] command Host capture command state to initialize.
 */
void wheel_center_capture_command_init(WheelCenterCaptureCommand *command);

/**
 * @brief Applies a host wheel-center capture command.
 *
 * Recognizes seven-byte short reports beginning with F9 05, accepts the first one immediately,
 * and rate-limits later requests until the capture deadline.
 *
 * @param[in,out] command Host capture command timing state.
 * @param[in] output Classified USB output report.
 * @param[in] now_ms Current monotonic time in milliseconds.
 * @return WHEEL_CENTER_CAPTURE_UNHANDLED for an unrelated or malformed report,
 * WHEEL_CENTER_CAPTURE_HANDLED for a recognized rate-limited report, or
 * WHEEL_CENTER_CAPTURE_REQUESTED when capture may be started.
 */
WheelCenterCaptureAction wheel_center_capture_command_apply(WheelCenterCaptureCommand *command,
                                                            const UsbOutputCommand *output,
                                                            uint32_t now_ms);

/**
 * @brief Tests and schedules a wheel-center calibration result notice.
 *
 * Allows a notice only after the previous four-second presentation deadline and advances the
 * deadline when a notice is due.
 *
 * @param[in,out] command Host capture command timing state.
 * @param[in] now_ms Current monotonic time in milliseconds.
 * @return true when a new result notice is due and the deadline was advanced; false otherwise.
 */
bool wheel_center_capture_command_notification_due(WheelCenterCaptureCommand *command,
                                                   uint32_t now_ms);

#endif
