#include "wheel/center_capture.h"

#include <stddef.h>
#include <stdint.h>

#include "platform/time.h"

enum {
    WHEEL_CENTER_CAPTURE_COMMAND_SIZE = 7,
    WHEEL_CENTER_CAPTURE_COMMAND_PREFIX = 0xf9,
    WHEEL_CENTER_CAPTURE_COMMAND_SUBCOMMAND = 5,
    WHEEL_CENTER_CAPTURE_INTERVAL_MS = 1000,
    WHEEL_CENTER_CAPTURE_NOTIFICATION_INTERVAL_MS = 4000,
};

/**
 * @brief Initializes host wheel-center capture command state.
 *
 * Allows the first matching command immediately and initializes result-notice timing.
 *
 * @param[out] command Host capture command state to initialize.
 */
void wheel_center_capture_command_init(WheelCenterCaptureCommand *command) {
    *command = (WheelCenterCaptureCommand){0};
}

/**
 * @brief Applies a host wheel-center capture command.
 *
 * Claims seven-byte short reports whose first two bytes are F9 05. The first matching report is
 * accepted immediately, then further matching reports are claimed without requesting another
 * capture until one second has elapsed.
 *
 * @param[in,out] command Host capture command timing state.
 * @param[in] output Classified USB output report.
 * @param[in] now_ms Current monotonic time in milliseconds.
 * @return Whether the report was unrelated, rate-limited, or accepted for capture.
 */
WheelCenterCaptureAction wheel_center_capture_command_apply(WheelCenterCaptureCommand *command,
                                                            const UsbOutputCommand *output,
                                                            uint32_t now_ms) {
    if (command == NULL || output == NULL || output->kind != USB_OUTPUT_COMMAND_SHORT ||
        output->payload == NULL || output->length != WHEEL_CENTER_CAPTURE_COMMAND_SIZE ||
        output->payload[0] != WHEEL_CENTER_CAPTURE_COMMAND_PREFIX ||
        output->payload[1] != WHEEL_CENTER_CAPTURE_COMMAND_SUBCOMMAND) {
        return WHEEL_CENTER_CAPTURE_UNHANDLED;
    }
    if (!platform_time_reached(now_ms, command->next_capture_ms)) {
        return WHEEL_CENTER_CAPTURE_HANDLED;
    }
    command->next_capture_ms = now_ms + WHEEL_CENTER_CAPTURE_INTERVAL_MS;
    return WHEEL_CENTER_CAPTURE_REQUESTED;
}

/**
 * @brief Schedules the wheel-center calibration result notice.
 *
 * Allows a result only after the previous four-second presentation deadline. The strict comparison
 * keeps the existing notice through its deadline and supports millisecond-counter wrap.
 *
 * @param[in,out] command Host capture command timing state.
 * @param[in] now_ms Current monotonic time in milliseconds.
 * @return True when a new result notice should be queued.
 */
bool wheel_center_capture_command_notification_due(WheelCenterCaptureCommand *command,
                                                   uint32_t now_ms) {
    if ((int32_t)(now_ms - command->next_notification_ms) <= 0) {
        return false;
    }
    command->next_notification_ms = now_ms + WHEEL_CENTER_CAPTURE_NOTIFICATION_INTERVAL_MS;
    return true;
}
