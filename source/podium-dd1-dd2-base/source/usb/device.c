#include "usb/device.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "platform/usb.h"
#include "usb/compatibility_descriptor.h"
#include "usb/compatibility_report_descriptor.h"
#include "usb/control_pipe.h"
#include "usb/control_request.h"
#include "usb/descriptor.h"
#include "usb/device_control.h"
#include "usb/podium_report_descriptor.h"

enum {
    USB_CONTROL_ENDPOINT = 0,
    USB_HID_ENDPOINT = 1,
    USB_HID_DESCRIPTOR_OFFSET = 18,
    USB_HID_DESCRIPTOR_SIZE = 9,
    USB_STRING_COUNT = 9,
    USB_MANUFACTURER_DESCRIPTOR_SIZE = 16,
    USB_PRODUCT_DESCRIPTOR_SIZE = 60,
};

typedef enum {
    USB_CONTROL_STAGE_IDLE,
    USB_CONTROL_STAGE_DATA_IN,
    USB_CONTROL_STAGE_DATA_OUT,
    USB_CONTROL_STAGE_STATUS_IN,
    USB_CONTROL_STAGE_STATUS_OUT,
} UsbControlStage;

static uint8_t device_descriptor[USB_DEVICE_DESCRIPTOR_SIZE];
static uint8_t configuration_descriptor[USB_HID_CONFIGURATION_DESCRIPTOR_SIZE];
static uint8_t report_descriptor[USB_PODIUM_REPORT_DESCRIPTOR_SIZE];
static uint8_t language_descriptor[4];
static uint8_t manufacturer_descriptor[USB_MANUFACTURER_DESCRIPTOR_SIZE];
static uint8_t product_descriptor[USB_PRODUCT_DESCRIPTOR_SIZE];
static UsbDescriptorView strings[USB_STRING_COUNT];
static UsbDescriptorCatalog descriptor_catalog;
static UsbDeviceIdentity descriptor_identity;
static UsbHidConfiguration hid_configuration;
static UsbDeviceControl device_control;
static UsbControlPipe control_pipe;
static UsbControlPacket control_packet;
static UsbSetupPacket setup_packet;
static UsbControlRequest control_request;
static UsbControlTransfer control_transfer;
static PlatformUsbEvent usb_event;
static UsbDeviceOutputReport output_report;
static uint8_t value_data[2];
static uint8_t input_report[USB_DEVICE_REPORT_SIZE];
static uint8_t input_report_length;
static uint8_t control_report_type;
static uint8_t control_report_id;
static UsbControlStage control_stage;
static bool output_ready;
static bool input_data_one;
static bool output_data_one;
static BoardVariant board_variant;
static UsbInputReportMode input_mode;

static bool input_report_matches(const uint8_t *report, uint8_t length) {
    if (input_report_length != length) {
        return false;
    }
    for (uint8_t index = 0; index < length; index++) {
        if (input_report[index] != report[index]) {
            return false;
        }
    }
    return true;
}

static bool build_descriptors(BoardVariant variant, UsbInputReportMode mode) {
    const char *product;
    uint8_t product_index;
    size_t report_length;

    if (mode == USB_INPUT_REPORT_MODE_FANATEC) {
        descriptor_identity = (UsbDeviceIdentity){
            .usb_version = 0x0200,
            .vendor_id = 0x0eb7,
            .product_id = 0x0004,
            .device_version = 0x0523,
            .control_packet_size = PLATFORM_USB_PACKET_SIZE,
            .manufacturer_string = 1,
            .product_string = 3,
        };
        hid_configuration = (UsbHidConfiguration){
            .hid_version = 0x0111,
            .report_descriptor_size = USB_PODIUM_REPORT_DESCRIPTOR_SIZE,
            .endpoint_packet_size = PLATFORM_USB_PACKET_SIZE,
            .maximum_power_ma = 80,
            .country_code = 0x21,
            .input_endpoint = 0x81,
            .output_endpoint = 0x01,
            .poll_interval_ms = 1,
            .self_powered = true,
        };
        product = variant == BOARD_VARIANT_DD1 ? "FANATEC Podium Wheel Base DD1"
                                               : "FANATEC Podium Wheel Base DD2";
        product_index = 3;
        usb_podium_report_descriptor_encode(report_descriptor);
        report_length = USB_PODIUM_REPORT_DESCRIPTOR_SIZE;
    } else {
        if (!usb_compatibility_descriptor_profile(mode, &descriptor_identity, &hid_configuration)) {
            return false;
        }
        report_length = usb_compatibility_report_descriptor_encode(mode, report_descriptor,
                                                                   sizeof(report_descriptor));
        if (report_length == 0) {
            return false;
        }
        if (mode == USB_INPUT_REPORT_MODE_FANATEC_COMPATIBILITY) {
            product = "FANATEC CSL Elite Wheel Base";
            product_index = 8;
        } else {
            product = "G27 Racing Wheel";
            product_index = 2;
        }
    }

    usb_device_descriptor_encode(&descriptor_identity, device_descriptor);
    usb_hid_configuration_descriptor_encode(&hid_configuration, configuration_descriptor);
    size_t language_length =
        usb_language_descriptor_encode(0x0409, language_descriptor, sizeof(language_descriptor));
    size_t manufacturer_length = usb_string_descriptor_encode("Fanatec", manufacturer_descriptor,
                                                              sizeof(manufacturer_descriptor));
    size_t product_length =
        usb_string_descriptor_encode(product, product_descriptor, sizeof(product_descriptor));

    for (uint8_t index = 0; index < USB_STRING_COUNT; index++) {
        strings[index] = (UsbDescriptorView){0};
    }
    strings[0] =
        (UsbDescriptorView){.data = language_descriptor, .length = (uint16_t)language_length};
    if (descriptor_identity.manufacturer_string != 0) {
        strings[descriptor_identity.manufacturer_string] = (UsbDescriptorView){
            .data = manufacturer_descriptor, .length = (uint16_t)manufacturer_length};
    }
    strings[product_index] =
        (UsbDescriptorView){.data = product_descriptor, .length = (uint16_t)product_length};
    descriptor_catalog = (UsbDescriptorCatalog){
        .device = {.data = device_descriptor, .length = sizeof(device_descriptor)},
        .configuration =
            {
                .data = configuration_descriptor,
                .length = sizeof(configuration_descriptor),
            },
        .hid =
            {
                .data = &configuration_descriptor[USB_HID_DESCRIPTOR_OFFSET],
                .length = USB_HID_DESCRIPTOR_SIZE,
            },
        .report = {.data = report_descriptor, .length = (uint16_t)report_length},
        .strings = strings,
        .string_count = USB_STRING_COUNT,
    };
    input_mode = mode;
    return true;
}

static void reset_state(void) {
    usb_device_control_init(&device_control, true);
    control_stage = USB_CONTROL_STAGE_IDLE;
    input_report_length = 0;
    output_ready = false;
    input_data_one = false;
    output_data_one = false;
}

void usb_device_init(BoardVariant variant) {
    board_variant = variant;
    (void)build_descriptors(variant, USB_INPUT_REPORT_MODE_FANATEC);
    reset_state();
    platform_usb_init();
    platform_usb_control_ready();
    platform_usb_attach();
}

/**
 * @brief Selects the primary USB input-report operating mode.
 *
 * Rebuilds the device, configuration, string, and report descriptors for modes 0 through 4,
 * clears the control and HID transfer state, and restarts the USB controller.
 *
 * @param[in] mode Primary USB operating-mode selector.
 * @return True when the mode has a complete descriptor profile; otherwise false.
 */
bool usb_device_set_input_mode(UsbInputReportMode mode) {
    if (mode > USB_INPUT_REPORT_MODE_G27) {
        return false;
    }
    platform_usb_detach();
    if (!build_descriptors(board_variant, mode)) {
        platform_usb_attach();
        return false;
    }
    reset_state();
    platform_usb_control_ready();
    platform_usb_restart();
    return true;
}

/**
 * @brief Returns the active primary USB input-report operating mode.
 *
 * Reports the selector used for descriptor enumeration and primary input-report encoding.
 *
 * @return Active primary USB operating-mode selector.
 */
UsbInputReportMode usb_device_input_mode(void) { return input_mode; }

static void stall_control(void) {
    control_stage = USB_CONTROL_STAGE_IDLE;
    platform_usb_stall(USB_CONTROL_ENDPOINT);
}

static bool send_next_control_packet(void) {
    if (!usb_control_pipe_next(&control_pipe, &control_packet)) {
        return false;
    }
    return platform_usb_send(USB_CONTROL_ENDPOINT, control_packet.data.data,
                             (uint8_t)control_packet.data.length, control_packet.data_one);
}

static void begin_control_input(UsbDescriptorView data, uint16_t requested_length) {
    usb_control_pipe_begin(&control_pipe, data, requested_length);
    if (!send_next_control_packet()) {
        stall_control();
        return;
    }
    control_stage = USB_CONTROL_STAGE_DATA_IN;
}

static void begin_value_input(void) {
    value_data[0] = (uint8_t)control_transfer.value;
    value_data[1] = (uint8_t)(control_transfer.value >> 8);
    begin_control_input((UsbDescriptorView){.data = value_data, .length = control_transfer.length},
                        control_request.length);
}

static bool report_matches_request(void) {
    return control_transfer.report_type == USB_DEVICE_HID_REPORT_INPUT &&
           input_report_length != 0 &&
           (input_mode == USB_INPUT_REPORT_MODE_FANATEC
                ? input_report[0] == control_transfer.report_id
                : control_transfer.report_id == 0);
}

static void handle_control_transfer(void) {
    switch (control_transfer.kind) {
    case USB_CONTROL_TRANSFER_ACKNOWLEDGE:
        if (platform_usb_send(USB_CONTROL_ENDPOINT, 0, 0, true)) {
            control_stage = USB_CONTROL_STAGE_STATUS_IN;
        } else {
            stall_control();
        }
        break;
    case USB_CONTROL_TRANSFER_DATA:
        begin_control_input(control_transfer.data, control_request.length);
        break;
    case USB_CONTROL_TRANSFER_VALUE:
        begin_value_input();
        break;
    case USB_CONTROL_TRANSFER_REPORT_IN:
        if (report_matches_request()) {
            begin_control_input(
                (UsbDescriptorView){.data = input_report, .length = input_report_length},
                control_request.length);
        } else {
            stall_control();
        }
        break;
    case USB_CONTROL_TRANSFER_REPORT_OUT:
        if (control_transfer.length <= USB_DEVICE_REPORT_SIZE &&
            platform_usb_receive(USB_CONTROL_ENDPOINT, (uint8_t)control_transfer.length, true)) {
            control_report_type = control_transfer.report_type;
            control_report_id = control_transfer.report_id;
            control_stage = USB_CONTROL_STAGE_DATA_OUT;
        } else {
            stall_control();
        }
        break;
    case USB_CONTROL_TRANSFER_STALL:
        stall_control();
        break;
    }
}

static void handle_setup(void) {
    if (usb_event.endpoint != USB_CONTROL_ENDPOINT) {
        stall_control();
        return;
    }
    usb_device_control_cancel(&device_control);
    if (usb_event.length != USB_SETUP_PACKET_SIZE ||
        !usb_setup_packet_decode(usb_event.data, &setup_packet) ||
        !usb_control_request_classify(&setup_packet, &control_request)) {
        stall_control();
        return;
    }
    control_transfer =
        usb_device_control_handle(&device_control, &control_request, &descriptor_catalog);
    handle_control_transfer();
}

static void configure_hid_endpoint(void) {
    input_data_one = false;
    output_data_one = false;
    platform_usb_configure_hid_endpoint();
    if (platform_usb_receive(USB_HID_ENDPOINT, USB_DEVICE_REPORT_SIZE, output_data_one)) {
        output_data_one = !output_data_one;
    }
    if (platform_usb_receive(USB_HID_ENDPOINT, USB_DEVICE_REPORT_SIZE, output_data_one)) {
        output_data_one = !output_data_one;
    }
}

static void complete_control_change(void) {
    UsbDevicePendingChange pending_change = device_control.pending_change;
    usb_device_control_complete(&device_control);
    if (pending_change == USB_DEVICE_PENDING_ADDRESS) {
        platform_usb_set_address(device_control.address);
    } else if (pending_change == USB_DEVICE_PENDING_CONFIGURATION) {
        if (usb_device_control_configured(&device_control)) {
            configure_hid_endpoint();
        } else {
            platform_usb_unconfigure_hid_endpoint();
        }
    }
}

static void handle_control_input_complete(void) {
    if (control_stage == USB_CONTROL_STAGE_DATA_IN) {
        if (send_next_control_packet()) {
            return;
        }
        if (platform_usb_receive(USB_CONTROL_ENDPOINT, 0, true)) {
            control_stage = USB_CONTROL_STAGE_STATUS_OUT;
        } else {
            stall_control();
        }
        return;
    }
    if (control_stage == USB_CONTROL_STAGE_STATUS_IN) {
        complete_control_change();
        control_stage = USB_CONTROL_STAGE_IDLE;
        platform_usb_control_ready();
    }
}

static void store_output_report(uint8_t report_type, uint8_t report_id, const uint8_t *data,
                                uint8_t length) {
    output_report.report_type = report_type;
    output_report.report_id = report_id;
    output_report.length = length;
    for (uint8_t index = 0; index < length; index++) {
        output_report.data[index] = data[index];
    }
    output_ready = true;
}

static void handle_control_output(void) {
    if (control_stage == USB_CONTROL_STAGE_STATUS_OUT && usb_event.length == 0) {
        control_stage = USB_CONTROL_STAGE_IDLE;
        platform_usb_control_ready();
        return;
    }
    if (control_stage == USB_CONTROL_STAGE_DATA_OUT) {
        store_output_report(control_report_type, control_report_id, usb_event.data,
                            usb_event.length);
        if (platform_usb_send(USB_CONTROL_ENDPOINT, 0, 0, true)) {
            control_stage = USB_CONTROL_STAGE_STATUS_IN;
        } else {
            stall_control();
        }
        return;
    }
    stall_control();
}

static void handle_hid_output(void) {
    if (usb_event.length != 0) {
        uint8_t report_id = input_mode == USB_INPUT_REPORT_MODE_FANATEC ? usb_event.data[0] : 0;
        store_output_report(USB_DEVICE_HID_REPORT_OUTPUT, report_id, usb_event.data,
                            usb_event.length);
    }
    if (platform_usb_receive(USB_HID_ENDPOINT, USB_DEVICE_REPORT_SIZE, output_data_one)) {
        output_data_one = !output_data_one;
    }
}

static void handle_reset(void) {
    reset_state();
    platform_usb_control_ready();
}

static void handle_event(void) {
    switch (usb_event.type) {
    case PLATFORM_USB_EVENT_RESET:
        handle_reset();
        break;
    case PLATFORM_USB_EVENT_SETUP:
        handle_setup();
        break;
    case PLATFORM_USB_EVENT_OUT:
        if (usb_event.endpoint == USB_CONTROL_ENDPOINT) {
            handle_control_output();
        } else if (usb_event.endpoint == USB_HID_ENDPOINT) {
            handle_hid_output();
        }
        break;
    case PLATFORM_USB_EVENT_IN_COMPLETE:
        if (usb_event.endpoint == USB_CONTROL_ENDPOINT) {
            handle_control_input_complete();
        }
        break;
    case PLATFORM_USB_EVENT_SUSPEND:
        break;
    }
}

void usb_device_service(void) {
    while (platform_usb_take_event(&usb_event)) {
        handle_event();
    }
}

bool usb_device_configured(void) { return usb_device_control_configured(&device_control); }

bool usb_device_take_output(UsbDeviceOutputReport *report) {
    if (!output_ready || report == 0) {
        return false;
    }
    *report = output_report;
    output_ready = false;
    return true;
}

bool usb_device_send_input(const uint8_t *report, uint8_t length) {
    if (!usb_device_configured() || report == 0 || length == 0 || length > USB_DEVICE_REPORT_SIZE) {
        return false;
    }
    if (input_report_matches(report, length)) {
        return true;
    }
    if (!platform_usb_send(USB_HID_ENDPOINT, report, length, input_data_one)) {
        return false;
    }
    for (uint8_t index = 0; index < length; index++) {
        input_report[index] = report[index];
    }
    input_report_length = length;
    input_data_one = !input_data_one;
    return true;
}
