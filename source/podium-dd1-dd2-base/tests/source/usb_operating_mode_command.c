#include <assert.h>
#include <stddef.h>
#include <stdint.h>

#include "usb/operating_mode_command.h"

static void test_decodes_command_envelope(void) {
    const uint8_t payload[] = {0xf8, 9, 5, 0x11, 0x22, 0x33, 0x44};
    UsbOutputCommand output = {
        .kind = USB_OUTPUT_COMMAND_SHORT,
        .payload = payload,
        .length = sizeof(payload),
    };
    UsbOperatingModeCommand command;

    assert(usb_operating_mode_command_decode(&output, &command));
    assert(command.opcode == 5);
    assert(command.parameters[0] == 0x11);
    assert(command.parameters[1] == 0x22);
    assert(command.parameters[2] == 0x33);
    assert(command.parameters[3] == 0x44);
}

static void test_rejects_other_envelopes(void) {
    uint8_t payload[] = {0xf8, 9, 5, 0, 0, 0, 0};
    UsbOutputCommand output = {
        .kind = USB_OUTPUT_COMMAND_SHORT,
        .payload = payload,
        .length = sizeof(payload),
    };
    UsbOperatingModeCommand command;

    payload[0] = 0xf7;
    assert(!usb_operating_mode_command_decode(&output, &command));
    payload[0] = 0xf8;
    payload[1] = 8;
    assert(!usb_operating_mode_command_decode(&output, &command));
    payload[1] = 9;
    output.length = 6;
    assert(!usb_operating_mode_command_decode(&output, &command));
    output.length = 7;
    output.kind = USB_OUTPUT_COMMAND_VENDOR_TRANSFER;
    assert(!usb_operating_mode_command_decode(&output, &command));
    output.kind = USB_OUTPUT_COMMAND_SHORT;
    output.payload = NULL;
    assert(!usb_operating_mode_command_decode(&output, &command));
    assert(!usb_operating_mode_command_decode(NULL, &command));
    assert(!usb_operating_mode_command_decode(&output, NULL));
}

static void test_identifies_native_reset(void) {
    UsbOperatingModeCommand command = {
        .opcode = 1,
        .parameters = {1, 0, 0, 0},
    };

    assert(usb_operating_mode_command_requests_native_reset(&command));
    command.parameters[0] = 0;
    assert(!usb_operating_mode_command_requests_native_reset(&command));
    command.parameters[0] = 1;
    command.opcode = 2;
    assert(!usb_operating_mode_command_requests_native_reset(&command));
    assert(!usb_operating_mode_command_requests_native_reset(NULL));
}

static void test_decodes_operating_status(void) {
    UsbOperatingModeCommand command = {.opcode = 2, .parameters = {0}};
    bool enabled = true;

    assert(usb_operating_mode_command_decode_status(&command, &enabled));
    assert(!enabled);

    command.parameters[0] = 0xa5;
    assert(usb_operating_mode_command_decode_status(&command, &enabled));
    assert(enabled);

    command.opcode = 3;
    assert(!usb_operating_mode_command_decode_status(&command, &enabled));
    assert(!usb_operating_mode_command_decode_status(NULL, &enabled));
    assert(!usb_operating_mode_command_decode_status(&command, NULL));
}

static void test_identifies_led_pattern(void) {
    UsbOperatingModeCommand command = {
        .opcode = 0x10,
        .parameters = {0, 0xa5, 0, 0},
    };
    assert(usb_operating_mode_command_requests_led_pattern(&command));

    command.parameters[0] = 1;
    assert(!usb_operating_mode_command_requests_led_pattern(&command));

    command.opcode = 0x11;
    command.parameters[1] = 0x5a;
    assert(usb_operating_mode_command_requests_led_pattern(&command));

    command.opcode = 0x0f;
    assert(!usb_operating_mode_command_requests_led_pattern(&command));
    assert(!usb_operating_mode_command_requests_led_pattern(NULL));
}

static UsbOperatingModeCommand runtime_command(uint8_t subcommand, uint8_t request) {
    return (UsbOperatingModeCommand){
        .opcode = 1,
        .parameters = {subcommand, request, 0, 0},
    };
}

static void test_decodes_auxiliary_runtime_modes(void) {
    UsbOperatingModeCommand command = runtime_command(0xfe, 0);
    UsbRuntimeModeTransition transition;

    assert(usb_operating_mode_command_decode_runtime(&command, 0, 0, 2, &transition));
    assert(transition.mode == USB_RUNTIME_MODE_AUXILIARY);
    assert(transition.save_settings);

    assert(usb_operating_mode_command_decode_runtime(&command, 1, 0, 3, &transition));
    assert(transition.mode == USB_RUNTIME_MODE_AUXILIARY_RECOVERY);
    assert(transition.save_settings);

    assert(!usb_operating_mode_command_decode_runtime(&command, 0, 0, 1, &transition));
    assert(!usb_operating_mode_command_decode_runtime(&command, 2, 0, 2, &transition));
}

static void test_decodes_bridge_runtime_modes(void) {
    UsbRuntimeModeTransition transition;
    UsbOperatingModeCommand command = runtime_command(0xfe, 1);

    assert(usb_operating_mode_command_decode_runtime(&command, 0, 0xff, 0, &transition));
    assert(transition.mode == USB_RUNTIME_MODE_STATUS_BRIDGE);
    assert(!transition.save_settings);

    command.parameters[1] = 3;
    assert(usb_operating_mode_command_decode_runtime(&command, 1, 0xff, 0, &transition));
    assert(transition.mode == USB_RUNTIME_MODE_PROTOCOL_BRIDGE);
    assert(!transition.save_settings);
}

static bool accepts_usb_bridge(uint8_t wheel_mode) {
    UsbOperatingModeCommand command = runtime_command(0xfe, 2);
    UsbRuntimeModeTransition transition;
    bool accepted =
        usb_operating_mode_command_decode_runtime(&command, 0, wheel_mode, 0, &transition);
    if (accepted) {
        assert(transition.mode == USB_RUNTIME_MODE_USB_BRIDGE);
        assert(!transition.save_settings);
    }
    return accepted;
}

static void test_gates_usb_bridge_by_wheel_mode(void) {
    for (uint16_t wheel_mode = 0; wheel_mode <= UINT8_MAX; wheel_mode++) {
        bool expected = wheel_mode == 0 || (wheel_mode >= 9 && wheel_mode <= 12) ||
                        (wheel_mode >= 16 && wheel_mode <= 22) ||
                        (wheel_mode >= 27 && wheel_mode <= 30);
        assert(accepts_usb_bridge((uint8_t)wheel_mode) == expected);
    }
}

static void test_decodes_reset_and_rejects_other_runtime_commands(void) {
    UsbRuntimeModeTransition transition = {
        .mode = USB_RUNTIME_MODE_PROTOCOL_RECOVERY,
        .save_settings = true,
    };
    UsbOperatingModeCommand command = runtime_command(0xff, 0xa5);

    assert(usb_operating_mode_command_decode_runtime(&command, UINT8_MAX, UINT8_MAX, UINT8_MAX,
                                                     &transition));
    assert(transition.mode == USB_RUNTIME_MODE_RESET);
    assert(!transition.save_settings);

    command.parameters[0] = 0xfd;
    assert(!usb_operating_mode_command_decode_runtime(&command, 0, 0, 0, &transition));
    command.parameters[0] = 0xfe;
    command.parameters[1] = 4;
    assert(!usb_operating_mode_command_decode_runtime(&command, 0, 0, 0, &transition));
    command.opcode = 2;
    assert(!usb_operating_mode_command_decode_runtime(&command, 0, 0, 0, &transition));
    assert(!usb_operating_mode_command_decode_runtime(NULL, 0, 0, 0, &transition));
    assert(!usb_operating_mode_command_decode_runtime(&command, 0, 0, 0, NULL));
}

int main(void) {
    test_decodes_command_envelope();
    test_rejects_other_envelopes();
    test_identifies_native_reset();
    test_decodes_operating_status();
    test_identifies_led_pattern();
    test_decodes_auxiliary_runtime_modes();
    test_decodes_bridge_runtime_modes();
    test_gates_usb_bridge_by_wheel_mode();
    test_decodes_reset_and_rejects_other_runtime_commands();
    return 0;
}
