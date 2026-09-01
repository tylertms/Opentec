#include <assert.h>
#include <stddef.h>
#include <stdint.h>

#include "usb/tuning_menu_service.h"

static UsbVendorCommand command(uint8_t arguments[2], uint8_t length) {
    return (UsbVendorCommand){
        .kind = USB_VENDOR_COMMAND_TUNING_MENU,
        .opcode = 2,
        .arguments = arguments,
        .length = length,
    };
}

static void test_selects_pages_and_encodes_status(void) {
    UsbTuningMenuService service;
    usb_tuning_menu_service_init(&service);
    uint8_t arguments[2] = {2, USB_TUNING_MENU_PAGE_MOTOR_DATA_ANALYSIS};
    UsbVendorCommand select = command(arguments, sizeof(arguments));
    uint8_t output[USB_DEVICE_REPORT_SIZE];

    assert(!usb_tuning_menu_service_response_pending(&service));
    assert(usb_tuning_menu_service_apply(&service, &select));
    assert(usb_tuning_menu_service_response_pending(&service));
    usb_tuning_menu_service_encode_response(&service, output);
    assert(output[0] == UINT8_MAX);
    assert(output[1] == 2);
    assert(output[2] == USB_TUNING_MENU_PAGE_MOTOR_DATA_ANALYSIS);
    for (uint8_t index = 3; index < USB_DEVICE_REPORT_SIZE; index++) {
        assert(output[index] == 0);
    }
    usb_tuning_menu_service_response_sent(&service);
    assert(!usb_tuning_menu_service_response_pending(&service));
}

static void test_refreshes_without_changing_page(void) {
    UsbTuningMenuService service;
    usb_tuning_menu_service_init(&service);
    uint8_t arguments[2] = {3, 0};
    UsbVendorCommand refresh = command(arguments, 1);
    uint8_t output[USB_DEVICE_REPORT_SIZE];

    assert(usb_tuning_menu_service_apply(&service, &refresh));
    usb_tuning_menu_service_encode_response(&service, output);
    assert(output[2] == 0);
}

static void test_preserves_page_for_unsupported_selectors(void) {
    UsbTuningMenuService service;
    usb_tuning_menu_service_init(&service);
    uint8_t arguments[2] = {2, 7};
    UsbVendorCommand select = command(arguments, sizeof(arguments));
    assert(usb_tuning_menu_service_apply(&service, &select));
    assert(!usb_tuning_menu_service_response_pending(&service));
    arguments[1] = 0;
    assert(usb_tuning_menu_service_apply(&service, &select));
    assert(!usb_tuning_menu_service_response_pending(&service));
}

static void test_rejects_incomplete_or_unrelated_commands(void) {
    UsbTuningMenuService service;
    usb_tuning_menu_service_init(&service);
    uint8_t arguments[2] = {2, 1};
    UsbVendorCommand select = command(arguments, 1);

    assert(!usb_tuning_menu_service_apply(&service, &select));
    arguments[0] = 1;
    select.length = sizeof(arguments);
    assert(!usb_tuning_menu_service_apply(&service, &select));
    select.kind = USB_VENDOR_COMMAND_DEVICE_CONTROL_UPDATE;
    assert(!usb_tuning_menu_service_apply(&service, &select));
    assert(!usb_tuning_menu_service_apply(NULL, &select));
    assert(!usb_tuning_menu_service_apply(&service, NULL));
    assert(!usb_tuning_menu_service_response_pending(NULL));
}

int main(void) {
    test_selects_pages_and_encodes_status();
    test_refreshes_without_changing_page();
    test_preserves_page_for_unsupported_selectors();
    test_rejects_incomplete_or_unrelated_commands();
    return 0;
}
