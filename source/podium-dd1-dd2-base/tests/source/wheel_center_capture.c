#include <assert.h>
#include <stddef.h>
#include <stdint.h>

#include "wheel/center_capture.h"

static UsbOutputCommand make_command(const uint8_t payload[7]) {
    return (UsbOutputCommand){
        .kind = USB_OUTPUT_COMMAND_SHORT,
        .payload = payload,
        .length = 7,
    };
}

static void test_accepts_capture_command_once_per_second(void) {
    const uint8_t payload[7] = {0xf9, 5, 0, 0, 0, 0, 0};
    UsbOutputCommand output = make_command(payload);
    WheelCenterCaptureCommand command;
    wheel_center_capture_command_init(&command);

    assert(wheel_center_capture_command_apply(&command, &output, 0) ==
           WHEEL_CENTER_CAPTURE_REQUESTED);
    assert(wheel_center_capture_command_apply(&command, &output, 999) ==
           WHEEL_CENTER_CAPTURE_HANDLED);
    assert(wheel_center_capture_command_apply(&command, &output, 1000) ==
           WHEEL_CENTER_CAPTURE_REQUESTED);
}

static void test_deadline_wraps(void) {
    const uint8_t payload[7] = {0xf9, 5, 0, 0, 0, 0, 0};
    UsbOutputCommand output = make_command(payload);
    WheelCenterCaptureCommand command;
    wheel_center_capture_command_init(&command);
    command.next_capture_ms = UINT32_MAX - 500;

    assert(wheel_center_capture_command_apply(&command, &output, UINT32_MAX - 500) ==
           WHEEL_CENTER_CAPTURE_REQUESTED);
    assert(wheel_center_capture_command_apply(&command, &output, 498) ==
           WHEEL_CENTER_CAPTURE_HANDLED);
    assert(wheel_center_capture_command_apply(&command, &output, 499) ==
           WHEEL_CENTER_CAPTURE_REQUESTED);
}

static void test_throttles_result_notification_for_four_seconds(void) {
    WheelCenterCaptureCommand command;
    wheel_center_capture_command_init(&command);

    assert(!wheel_center_capture_command_notification_due(&command, 0));
    assert(wheel_center_capture_command_notification_due(&command, 1));
    assert(command.next_notification_ms == 4001);
    assert(!wheel_center_capture_command_notification_due(&command, 4001));
    assert(wheel_center_capture_command_notification_due(&command, 4002));

    command.next_notification_ms = UINT32_MAX - 1;
    assert(!wheel_center_capture_command_notification_due(&command, UINT32_MAX - 1));
    assert(wheel_center_capture_command_notification_due(&command, UINT32_MAX));
    assert(command.next_notification_ms == 3999);
    assert(!wheel_center_capture_command_notification_due(&command, 3999));
    assert(wheel_center_capture_command_notification_due(&command, 4000));
}

static void test_rejects_other_reports(void) {
    uint8_t payload[7] = {0xf8, 5, 0, 0, 0, 0, 0};
    UsbOutputCommand output = make_command(payload);
    WheelCenterCaptureCommand command;
    wheel_center_capture_command_init(&command);

    assert(wheel_center_capture_command_apply(&command, &output, 0) ==
           WHEEL_CENTER_CAPTURE_UNHANDLED);
    payload[0] = 0xf9;
    payload[1] = 4;
    assert(wheel_center_capture_command_apply(&command, &output, 0) ==
           WHEEL_CENTER_CAPTURE_UNHANDLED);
    payload[1] = 5;
    output.length = 6;
    assert(wheel_center_capture_command_apply(&command, &output, 0) ==
           WHEEL_CENTER_CAPTURE_UNHANDLED);
    output.length = 7;
    output.kind = USB_OUTPUT_COMMAND_VENDOR_TRANSFER;
    assert(wheel_center_capture_command_apply(&command, &output, 0) ==
           WHEEL_CENTER_CAPTURE_UNHANDLED);
    assert(wheel_center_capture_command_apply(NULL, &output, 0) == WHEEL_CENTER_CAPTURE_UNHANDLED);
    assert(wheel_center_capture_command_apply(&command, NULL, 0) == WHEEL_CENTER_CAPTURE_UNHANDLED);
}

int main(void) {
    test_accepts_capture_command_once_per_second();
    test_deadline_wraps();
    test_throttles_result_notification_for_four_seconds();
    test_rejects_other_reports();
    return 0;
}
