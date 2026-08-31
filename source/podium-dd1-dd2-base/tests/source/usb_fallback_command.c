#include <assert.h>
#include <stdbool.h>
#include <stdint.h>

#include "usb/fallback_command.h"

static bool decode(const uint8_t payload[7], UsbFallbackCommand *command) {
    UsbOutputCommand output = {
        .kind = USB_OUTPUT_COMMAND_SHORT,
        .payload = payload,
        .length = 7,
    };
    return usb_fallback_command_decode(&output, command);
}

static void test_direct_f8_commands(void) {
    static const struct {
        uint8_t opcode;
        UsbFallbackCommandKind kind;
    } cases[] = {
        {0x02, USB_FALLBACK_STEERING_RANGE_LOW},      {0x03, USB_FALLBACK_STEERING_RANGE_HIGH},
        {0x12, USB_FALLBACK_DISPLAY_FLAGS},           {0x15, USB_FALLBACK_SENSITIVITY},
        {0x16, USB_FALLBACK_FORCE_FEEDBACK_STRENGTH}, {0x18, USB_FALLBACK_NATURAL_DAMPER},
        {0x19, USB_FALLBACK_NATURAL_FRICTION},        {0x1a, USB_FALLBACK_NATURAL_INERTIA},
        {0x1b, USB_FALLBACK_INTERPOLATION},           {0x1c, USB_FALLBACK_FORCE_EFFECT_INTENSITY},
        {0x1d, USB_FALLBACK_FORCE_EFFECT_STRENGTH},   {0x1e, USB_FALLBACK_SPRING_EFFECT_STRENGTH},
        {0x1f, USB_FALLBACK_DAMPER_EFFECT_STRENGTH},  {0x20, USB_FALLBACK_VIBRATION_STRENGTH},
        {0x81, USB_FALLBACK_STEERING_LIMIT},
    };
    for (uint8_t index = 0; index < sizeof(cases) / sizeof(cases[0]); index++) {
        uint8_t payload[7] = {0xf8, cases[index].opcode, 0x34, 0x12};
        UsbFallbackCommand command;
        assert(decode(payload, &command));
        assert(command.kind == cases[index].kind);
        assert(command.value == 0x1234);
    }

    uint8_t linear[7] = {0xf8, 0x17, 1};
    UsbFallbackCommand command;
    assert(decode(linear, &command));
    assert(command.kind == USB_FALLBACK_FORCE_SCALE);
    linear[2] = 2;
    assert(decode(linear, &command));
    linear[2] = 0;
    assert(!decode(linear, &command));
    linear[2] = 3;
    assert(!decode(linear, &command));
}

static void test_f9_commands(void) {
    UsbFallbackCommand command;
    uint8_t cooling[7] = {0xf9, 0x02, 0xff, 25, 75, 100};
    assert(decode(cooling, &command));
    assert(command.kind == USB_FALLBACK_COOLING_OVERRIDE);
    assert(command.parameters[0] == 0xff);
    assert(command.parameters[3] == 100);

    uint8_t security[7] = {0xf9, 0xa0, 1};
    assert(decode(security, &command));
    assert(command.kind == USB_FALLBACK_SECURITY_DISABLE);
    security[2] = 0;
    assert(!decode(security, &command));
}

static void test_rejects_other_reports(void) {
    UsbFallbackCommand command;
    uint8_t payload[7] = {0xf8, 0x09};
    assert(!decode(payload, &command));
    payload[0] = 0xfa;
    assert(!decode(payload, &command));

    UsbOutputCommand output = {
        .kind = USB_OUTPUT_COMMAND_VENDOR_TRANSFER,
        .payload = payload,
        .length = 7,
    };
    assert(!usb_fallback_command_decode(&output, &command));
    assert(!usb_fallback_command_decode(NULL, &command));
    assert(!usb_fallback_command_decode(&output, NULL));
}

int main(void) {
    test_direct_f8_commands();
    test_f9_commands();
    test_rejects_other_reports();
    return 0;
}
