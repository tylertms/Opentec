#include "usb/device.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "platform/time.h"
#include "platform/usb.h"
#include "usb/compatibility_descriptor.h"
#include "usb/compatibility_report_descriptor.h"
#include "usb/console_descriptor.h"
#include "usb/control_pipe.h"
#include "usb/control_request.h"
#include "usb/descriptor.h"
#include "usb/device_control.h"
#include "usb/playstation_authentication.h"
#include "usb/playstation_input.h"
#include "usb/podium_report_descriptor.h"
#include "usb/updater_control.h"
#include "usb/updater_descriptor.h"
#include "usb/xbox_gip_metadata.h"
#include "usb/xbox_gip_service.h"

enum {
    USB_CONTROL_ENDPOINT = 0,
    USB_PRIMARY_ENDPOINT = 1,
    USB_UPDATER_NOTIFICATION_ENDPOINT = 2,
    USB_UPDATER_DATA_ENDPOINT = 3,
    USB_PLAYSTATION_OUTPUT_ENDPOINT = 3,
    USB_PLAYSTATION_INPUT_ENDPOINT = 4,
    USB_HID_DESCRIPTOR_OFFSET = 18,
    USB_HID_DESCRIPTOR_SIZE = 9,
    USB_STRING_COUNT = 10,
    USB_MANUFACTURER_DESCRIPTOR_SIZE = 16,
    USB_PRODUCT_DESCRIPTOR_SIZE = 60,
    USB_PLAYSTATION_PRODUCT_DESCRIPTOR_SIZE = 96,
};

typedef enum {
    USB_CONTROL_STAGE_IDLE,
    USB_CONTROL_STAGE_DATA_IN,
    USB_CONTROL_STAGE_DATA_OUT,
    USB_CONTROL_STAGE_PLAYSTATION_AUTHENTICATION_OUT,
    USB_CONTROL_STAGE_UPDATER_LINE_CODING_OUT,
    USB_CONTROL_STAGE_STATUS_IN,
    USB_CONTROL_STAGE_STATUS_OUT,
} UsbControlStage;

static uint8_t device_descriptor[USB_DEVICE_DESCRIPTOR_SIZE];
static uint8_t configuration_descriptor[USB_UPDATER_CONFIGURATION_DESCRIPTOR_SIZE];
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
static uint8_t updater_line_coding[USB_UPDATER_LINE_CODING_SIZE];
static uint8_t xbox_security_descriptor[USB_XBOX_GIP_SECURITY_DESCRIPTOR_SIZE];
static uint8_t xbox_os_string_descriptor[USB_XBOX_GIP_OS_STRING_DESCRIPTOR_SIZE];
static uint8_t xbox_digest[USB_XBOX_GIP_DIGEST_SIZE];
static uint8_t xbox_request[USB_XBOX_GIP_METADATA_PACKET_SIZE];
static uint8_t xbox_response[USB_XBOX_GIP_METADATA_PACKET_SIZE];
static char xbox_serial[USB_XBOX_GIP_SERIAL_SIZE];
static uint8_t xbox_serial_descriptor[USB_XBOX_GIP_SERIAL_TEXT_SIZE * 2 + 2];
static uint8_t xbox_response_length;
static uint8_t input_report[USB_DEVICE_REPORT_SIZE];
static uint8_t updater_response[USB_DEVICE_UPDATER_RESPONSE_SIZE];
static uint8_t input_report_length;
static uint8_t updater_response_length;
static uint8_t updater_response_offset;
static uint8_t control_report_type;
static uint8_t control_report_id;
static UsbControlStage control_stage;
static bool output_ready;
static bool updater_packet_ready;
static bool updater_response_ready;
static bool updater_input_busy;
static bool updater_zero_length_pending;
static bool input_data_one;
static bool output_data_one;
static bool updater_input_data_one;
static bool updater_output_data_one;
static bool xbox_identity_ready;
static bool xbox_request_ready;
static bool xbox_response_ready;
static bool xbox_input_busy;
static BoardVariant board_variant;
static UsbInputReportMode input_mode;
static UsbOperatingMode operating_mode;
static UsbUpdaterControl updater_control;
static UsbDeviceUpdaterPacket updater_packet;
static UsbXboxGipService xbox_service;
static UsbXboxGipServiceIdentity xbox_service_identity;
static UsbXboxGipSessionAction xbox_session_actions;

/** @brief Storage used only by PlayStation USB mode. */
typedef struct {
    UsbPlaystationAuthentication authentication;
    uint8_t feature_report[USB_PLAYSTATION_AUTHENTICATION_REPORT_SIZE];
    uint8_t product_descriptor[USB_PLAYSTATION_PRODUCT_DESCRIPTOR_SIZE];
} UsbPlaystationWorkspace;

/** @brief Storage shared by mutually exclusive Xbox and PlayStation USB modes. */
typedef union {
    uint8_t xbox_metadata[USB_XBOX_GIP_METADATA_SIZE];
    UsbPlaystationWorkspace playstation;
} UsbConsoleWorkspace;

static UsbConsoleWorkspace console_workspace;

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

/**
 * @brief Builds the descriptor catalog for one USB operating mode.
 *
 * Selects the device identity, configuration, report, and string descriptors used during the next
 * enumeration. PlayStation mode uses the dedicated HID profile and product string index nine.
 *
 * @param[in] variant Wheel-base hardware variant.
 * @param[in] mode USB operating mode to build.
 * @return True when the mode has a complete descriptor profile; otherwise false.
 */
static bool build_descriptors(BoardVariant variant, UsbOperatingMode mode) {
    const char *product;
    uint8_t product_index;
    size_t configuration_length;
    size_t report_length;
    uint8_t *product_descriptor_data = product_descriptor;
    size_t product_descriptor_capacity = sizeof(product_descriptor);

    if (mode == USB_OPERATING_MODE_FANATEC) {
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
        configuration_length = USB_HID_CONFIGURATION_DESCRIPTOR_SIZE;
        report_length = USB_PODIUM_REPORT_DESCRIPTOR_SIZE;
    } else if (mode <= USB_OPERATING_MODE_G27) {
        UsbInputReportMode report_mode = (UsbInputReportMode)mode;
        if (!usb_compatibility_descriptor_profile(report_mode, &descriptor_identity,
                                                  &hid_configuration)) {
            return false;
        }
        report_length = usb_compatibility_report_descriptor_encode(report_mode, report_descriptor,
                                                                   sizeof(report_descriptor));
        if (report_length == 0) {
            return false;
        }
        configuration_length = USB_HID_CONFIGURATION_DESCRIPTOR_SIZE;
        if (mode == USB_OPERATING_MODE_FANATEC_COMPATIBILITY) {
            product = "FANATEC CSL Elite Wheel Base";
            product_index = 8;
        } else {
            product = "G27 Racing Wheel";
            product_index = 2;
        }
    } else if (mode == USB_OPERATING_MODE_UPDATER) {
        descriptor_identity = usb_updater_device_identity();
        product = usb_updater_product_name();
        product_index = descriptor_identity.product_string;
        usb_updater_configuration_descriptor_encode(configuration_descriptor);
        usb_updater_control_init(&updater_control);
        configuration_length = USB_UPDATER_CONFIGURATION_DESCRIPTOR_SIZE;
        report_length = 0;
    } else if (mode == USB_OPERATING_MODE_XBOX_GIP) {
        uint8_t mode_code = usb_xbox_gip_mode_code(variant, xbox_service_identity.wheel_mode);
        if (!xbox_identity_ready || mode_code == 0) {
            return false;
        }
        descriptor_identity = usb_xbox_gip_device_identity(0x0f00u | mode_code);
        product = usb_xbox_gip_product_name(variant);
        product_index = descriptor_identity.product_string;
        usb_xbox_gip_configuration_descriptor_encode(configuration_descriptor);
        configuration_length = USB_XBOX_GIP_CONFIGURATION_DESCRIPTOR_SIZE;
        report_length = 0;
    } else if (mode == USB_OPERATING_MODE_PLAYSTATION) {
        descriptor_identity = usb_playstation_device_identity(variant);
        product = usb_playstation_product_name(variant);
        product_index = descriptor_identity.product_string;
        usb_playstation_configuration_descriptor_encode(configuration_descriptor);
        usb_playstation_report_descriptor_encode(report_descriptor);
        configuration_length = USB_PLAYSTATION_CONFIGURATION_DESCRIPTOR_SIZE;
        report_length = USB_PLAYSTATION_REPORT_DESCRIPTOR_SIZE;
        product_descriptor_data = console_workspace.playstation.product_descriptor;
        product_descriptor_capacity = sizeof(console_workspace.playstation.product_descriptor);
        usb_playstation_authentication_init(&console_workspace.playstation.authentication);
    } else {
        return false;
    }

    usb_device_descriptor_encode(&descriptor_identity, device_descriptor);
    if (mode <= USB_OPERATING_MODE_G27) {
        usb_hid_configuration_descriptor_encode(&hid_configuration, configuration_descriptor);
    }
    size_t language_length =
        usb_language_descriptor_encode(0x0409, language_descriptor, sizeof(language_descriptor));
    size_t manufacturer_length = usb_string_descriptor_encode("Fanatec", manufacturer_descriptor,
                                                              sizeof(manufacturer_descriptor));
    size_t product_length =
        usb_string_descriptor_encode(product, product_descriptor_data, product_descriptor_capacity);

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
        (UsbDescriptorView){.data = product_descriptor_data, .length = (uint16_t)product_length};
    if (mode == USB_OPERATING_MODE_XBOX_GIP) {
        size_t serial_length = usb_string_descriptor_encode(xbox_serial, xbox_serial_descriptor,
                                                            sizeof(xbox_serial_descriptor));
        strings[descriptor_identity.serial_string] = (UsbDescriptorView){
            .data = xbox_serial_descriptor,
            .length = (uint16_t)serial_length,
        };
    }
    descriptor_catalog = (UsbDescriptorCatalog){
        .device = {.data = device_descriptor, .length = sizeof(device_descriptor)},
        .configuration =
            {
                .data = configuration_descriptor,
                .length = (uint16_t)configuration_length,
            },
        .hid =
            {
                .data =
                    report_length == 0 ? 0 : &configuration_descriptor[USB_HID_DESCRIPTOR_OFFSET],
                .length = report_length == 0 ? 0 : USB_HID_DESCRIPTOR_SIZE,
            },
        .report = {.data = report_length == 0 ? 0 : report_descriptor,
                   .length = (uint16_t)report_length},
        .strings = strings,
        .string_count = USB_STRING_COUNT,
    };
    operating_mode = mode;
    if (mode <= USB_OPERATING_MODE_G27) {
        input_mode = (UsbInputReportMode)mode;
    }
    return true;
}

/**
 * @brief Resets USB controller and endpoint service state.
 *
 * Clears pending control, HID, updater, and Xbox transfers while retaining the selected descriptor
 * catalog and PlayStation authentication payload across a bus reset.
 */
static void reset_state(void) {
    usb_device_control_init(&device_control, true);
    control_stage = USB_CONTROL_STAGE_IDLE;
    input_report_length = 0;
    output_ready = false;
    updater_packet_ready = false;
    updater_response_ready = false;
    updater_input_busy = false;
    updater_zero_length_pending = false;
    updater_response_length = 0;
    updater_response_offset = 0;
    input_data_one = false;
    output_data_one = false;
    updater_input_data_one = false;
    updater_output_data_one = false;
    xbox_request_ready = false;
    xbox_response_ready = false;
    xbox_input_busy = false;
    xbox_session_actions = USB_XBOX_GIP_SESSION_ACTION_NONE;
    for (uint8_t index = 0; index < USB_XBOX_GIP_METADATA_PACKET_SIZE; index++) {
        xbox_request[index] = 0;
    }
    usb_xbox_gip_service_init(&xbox_service);
}

/**
 * @brief Initializes the wheel-base USB device.
 *
 * Builds the native Fanatec descriptor profile, initializes console service data, resets transfer
 * state, and attaches the USB controller.
 *
 * @param[in] variant Wheel-base hardware variant.
 */
void usb_device_init(BoardVariant variant) {
    board_variant = variant;
    xbox_identity_ready = false;
    usb_xbox_gip_security_descriptor_encode(xbox_security_descriptor);
    usb_xbox_gip_os_string_descriptor_encode(xbox_os_string_descriptor);
    usb_xbox_gip_metadata_encode(console_workspace.xbox_metadata);
    xbox_service_identity = (UsbXboxGipServiceIdentity){
        .variant = variant,
        .digest = xbox_digest,
        .metadata = console_workspace.xbox_metadata,
    };
    (void)build_descriptors(variant, USB_OPERATING_MODE_FANATEC);
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
    return usb_device_set_operating_mode((UsbOperatingMode)mode);
}

/**
 * @brief Selects the active USB operating mode.
 *
 * Rebuilds and activates the descriptor and endpoint profile for primary HID modes zero through
 * four, the motor updater CDC mode five, a prepared Xbox GIP mode six identity, or the PlayStation
 * HID profile in mode seven.
 *
 * @param[in] mode USB operating-mode selector.
 * @return True when the mode has a complete transport profile; otherwise false.
 */
bool usb_device_set_operating_mode(UsbOperatingMode mode) {
    if (mode > USB_OPERATING_MODE_PLAYSTATION ||
        (mode == USB_OPERATING_MODE_XBOX_GIP && !xbox_identity_ready)) {
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
 * @brief Selects PlayStation USB mode.
 *
 * Rebuilds the device around the PlayStation HID descriptors, resets authentication transport
 * state, and restarts enumeration.
 *
 * @return True when the PlayStation profile was activated; otherwise false.
 */
bool usb_device_set_playstation_mode(void) {
    return usb_device_set_operating_mode(USB_OPERATING_MODE_PLAYSTATION);
}

/**
 * @brief Selects Xbox GIP mode for an attached wheel.
 *
 * Applies the wheel-specific product identifier, reverse-digest serial text, discovery digest,
 * and Xbox endpoint service before restarting USB in operating mode six.
 *
 * @param[in] wheel_mode Attached-wheel operating-mode selector.
 * @param[in] digest Eight-byte attached-wheel status digest.
 * @return True when the wheel mode has a supported Xbox identity; otherwise false.
 */
bool usb_device_set_xbox_mode(uint8_t wheel_mode, const uint8_t digest[USB_XBOX_GIP_DIGEST_SIZE]) {
    if (digest == 0 || usb_xbox_gip_mode_code(board_variant, wheel_mode) == 0) {
        return false;
    }
    xbox_service_identity.wheel_mode = wheel_mode;
    for (uint8_t index = 0; index < USB_XBOX_GIP_DIGEST_SIZE; index++) {
        xbox_digest[index] = digest[index];
    }
    usb_xbox_gip_serial_encode(xbox_digest, xbox_serial);
    usb_xbox_gip_metadata_encode(console_workspace.xbox_metadata);
    xbox_identity_ready = true;
    return usb_device_set_operating_mode(USB_OPERATING_MODE_XBOX_GIP);
}

/**
 * @brief Returns the active primary USB input-report operating mode.
 *
 * Reports the selector used for descriptor enumeration and primary input-report encoding.
 *
 * @return Active primary USB operating-mode selector.
 */
UsbInputReportMode usb_device_input_mode(void) { return input_mode; }

/**
 * @brief Returns the active USB operating mode.
 *
 * Reports the selector used for enumeration, control requests, and endpoint routing.
 *
 * @return Active USB operating-mode selector.
 */
UsbOperatingMode usb_device_operating_mode(void) { return operating_mode; }

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

/**
 * @brief Handles PlayStation authentication feature reports.
 *
 * Builds F1 response fragments, F2 status, and F3 format reports for feature reads. A feature write
 * for F0 arms one 64-byte control output transfer.
 *
 * @return True when the active control request belongs to the authentication transport; otherwise
 * false.
 */
static bool handle_playstation_authentication_setup(void) {
    if (operating_mode != USB_OPERATING_MODE_PLAYSTATION ||
        (uint8_t)(control_request.value >> 8) != USB_DEVICE_HID_REPORT_FEATURE) {
        return false;
    }

    if (control_request.kind == USB_CONTROL_HID_GET_REPORT) {
        uint16_t report_length;
        uint8_t report_id = (uint8_t)control_request.value;
        if (report_id == 0xf1) {
            if (!usb_playstation_authentication_response_report(
                    &console_workspace.playstation.authentication,
                    console_workspace.playstation.feature_report)) {
                stall_control();
                return true;
            }
            report_length = USB_PLAYSTATION_AUTHENTICATION_REPORT_SIZE;
        } else if (report_id == 0xf2) {
            usb_playstation_authentication_status_report(
                &console_workspace.playstation.authentication,
                console_workspace.playstation.feature_report);
            report_length = USB_PLAYSTATION_AUTHENTICATION_STATUS_REPORT_SIZE;
        } else if (report_id == 0xf3) {
            usb_playstation_authentication_format_report(
                &console_workspace.playstation.authentication,
                console_workspace.playstation.feature_report);
            report_length = USB_PLAYSTATION_AUTHENTICATION_FORMAT_REPORT_SIZE;
        } else {
            return false;
        }
        begin_control_input(
            (UsbDescriptorView){.data = console_workspace.playstation.feature_report,
                                .length = report_length},
            control_request.length);
        return true;
    }

    if (control_request.kind == USB_CONTROL_HID_SET_REPORT &&
        (uint8_t)control_request.value == 0xf0 &&
        control_request.length == USB_PLAYSTATION_AUTHENTICATION_REPORT_SIZE) {
        if (platform_usb_receive(USB_CONTROL_ENDPOINT, USB_PLAYSTATION_AUTHENTICATION_REPORT_SIZE,
                                 true)) {
            control_stage = USB_CONTROL_STAGE_PLAYSTATION_AUTHENTICATION_OUT;
        } else {
            stall_control();
        }
        return true;
    }
    return false;
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

/**
 * @brief Dispatches one endpoint-zero setup packet.
 *
 * Routes console-specific, updater, descriptor, and standard HID requests to their owning control
 * transfer state machines and stalls malformed or unsupported packets.
 */
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
    if (control_request.kind == USB_CONTROL_XBOX_SECURITY_DESCRIPTOR) {
        begin_control_input((UsbDescriptorView){.data = xbox_security_descriptor,
                                                .length = sizeof(xbox_security_descriptor)},
                            control_request.length);
        return;
    }
    if (operating_mode == USB_OPERATING_MODE_XBOX_GIP &&
        control_request.kind == USB_CONTROL_GET_DESCRIPTOR &&
        control_request.descriptor_type == 3 && control_request.descriptor_index == 0xee) {
        begin_control_input((UsbDescriptorView){.data = xbox_os_string_descriptor,
                                                .length = sizeof(xbox_os_string_descriptor)},
                            control_request.length);
        return;
    }
    if (handle_playstation_authentication_setup()) {
        return;
    }
    if (operating_mode == USB_OPERATING_MODE_UPDATER) {
        if (control_request.kind == USB_CONTROL_CDC_GET_LINE_CODING) {
            usb_updater_line_coding_encode(&updater_control, updater_line_coding);
            begin_control_input((UsbDescriptorView){.data = updater_line_coding,
                                                    .length = sizeof(updater_line_coding)},
                                control_request.length);
            return;
        }
        if (control_request.kind == USB_CONTROL_CDC_SET_LINE_CODING) {
            if (platform_usb_receive(USB_CONTROL_ENDPOINT, USB_UPDATER_LINE_CODING_SIZE, true)) {
                control_stage = USB_CONTROL_STAGE_UPDATER_LINE_CODING_OUT;
                return;
            }
            stall_control();
            return;
        }
        if (control_request.kind == USB_CONTROL_CDC_SET_CONTROL_LINE_STATE) {
            usb_updater_control_set_lines(&updater_control, (uint8_t)control_request.value);
            if (platform_usb_send(USB_CONTROL_ENDPOINT, 0, 0, true)) {
                control_stage = USB_CONTROL_STAGE_STATUS_IN;
            } else {
                stall_control();
            }
            return;
        }
    }
    control_transfer =
        usb_device_control_handle(&device_control, &control_request, &descriptor_catalog);
    handle_control_transfer();
}

/**
 * @brief Configures the application USB endpoints.
 *
 * Native HID and Xbox GIP use endpoint one in both directions. PlayStation mode uses endpoint three
 * for host output and endpoint four for device input. Both output banks are armed with alternating
 * data toggles.
 *
 */
static void configure_application_endpoints(void) {
    input_data_one = false;
    output_data_one = false;
    uint8_t output_endpoint = USB_PRIMARY_ENDPOINT;
    if (operating_mode == USB_OPERATING_MODE_PLAYSTATION) {
        output_endpoint = USB_PLAYSTATION_OUTPUT_ENDPOINT;
        platform_usb_configure_endpoint(USB_PLAYSTATION_INPUT_ENDPOINT, true, true);
        platform_usb_configure_endpoint(USB_PLAYSTATION_OUTPUT_ENDPOINT, true, true);
    } else {
        platform_usb_configure_endpoint(USB_PRIMARY_ENDPOINT, true, true);
    }
    if (platform_usb_receive(output_endpoint, USB_DEVICE_REPORT_SIZE, output_data_one)) {
        output_data_one = !output_data_one;
    }
    if (platform_usb_receive(output_endpoint, USB_DEVICE_REPORT_SIZE, output_data_one)) {
        output_data_one = !output_data_one;
    }
}

/**
 * @brief Configures the motor updater USB endpoints.
 *
 * Enables interrupt input endpoint 2, enables both directions on bulk endpoint 3, and arms both
 * endpoint 3 output banks for 64-byte transfers.
 */
static void configure_updater_endpoints(void) {
    updater_input_data_one = false;
    updater_output_data_one = false;
    platform_usb_configure_endpoint(USB_UPDATER_NOTIFICATION_ENDPOINT, true, false);
    platform_usb_configure_endpoint(USB_UPDATER_DATA_ENDPOINT, true, true);
    if (platform_usb_receive(USB_UPDATER_DATA_ENDPOINT, USB_DEVICE_REPORT_SIZE,
                             updater_output_data_one)) {
        updater_output_data_one = !updater_output_data_one;
    }
    if (platform_usb_receive(USB_UPDATER_DATA_ENDPOINT, USB_DEVICE_REPORT_SIZE,
                             updater_output_data_one)) {
        updater_output_data_one = !updater_output_data_one;
    }
}

/**
 * @brief Applies a completed USB address or configuration change.
 *
 * Commits the pending control request, updates the device address, and configures or removes the
 * endpoint set selected by the active operating mode.
 */
static void complete_control_change(void) {
    UsbDevicePendingChange pending_change = device_control.pending_change;
    usb_device_control_complete(&device_control);
    if (pending_change == USB_DEVICE_PENDING_ADDRESS) {
        platform_usb_set_address(device_control.address);
    } else if (pending_change == USB_DEVICE_PENDING_CONFIGURATION) {
        if (usb_device_control_configured(&device_control)) {
            if (operating_mode == USB_OPERATING_MODE_UPDATER) {
                configure_updater_endpoints();
            } else {
                configure_application_endpoints();
            }
        } else {
            if (operating_mode == USB_OPERATING_MODE_UPDATER) {
                platform_usb_unconfigure_endpoint(USB_UPDATER_NOTIFICATION_ENDPOINT);
                platform_usb_unconfigure_endpoint(USB_UPDATER_DATA_ENDPOINT);
            } else {
                platform_usb_unconfigure_endpoint(USB_PRIMARY_ENDPOINT);
                if (operating_mode == USB_OPERATING_MODE_PLAYSTATION) {
                    platform_usb_unconfigure_endpoint(USB_PLAYSTATION_OUTPUT_ENDPOINT);
                    platform_usb_unconfigure_endpoint(USB_PLAYSTATION_INPUT_ENDPOINT);
                }
            }
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

/**
 * @brief Stores one host output report for application processing.
 *
 * Replaces the pending output workspace with the supplied report classification and payload, then
 * marks it available to the main device service.
 *
 * @param[in] report_type HID report type associated with the payload.
 * @param[in] report_id HID report identifier, or zero for an unnumbered application packet.
 * @param[in] data Report payload to store.
 * @param[in] length Number of payload bytes to store.
 */
static void store_output_report(uint8_t report_type, uint8_t report_id, const uint8_t *data,
                                uint8_t length) {
    output_report.report_type = report_type;
    output_report.report_id = report_id;
    output_report.length = length;
    for (uint8_t index = 0; index < USB_DEVICE_REPORT_SIZE; index++) {
        output_report.data[index] = index < length ? data[index] : 0;
    }
    output_ready = true;
}

/**
 * @brief Completes an endpoint-zero output stage.
 *
 * Applies status, updater line-coding, PlayStation authentication, or generic HID output data and
 * starts the zero-length acknowledgement response.
 */
static void handle_control_output(void) {
    if (control_stage == USB_CONTROL_STAGE_STATUS_OUT && usb_event.length == 0) {
        control_stage = USB_CONTROL_STAGE_IDLE;
        platform_usb_control_ready();
        return;
    }
    if (control_stage == USB_CONTROL_STAGE_UPDATER_LINE_CODING_OUT) {
        if (!usb_updater_line_coding_decode(&updater_control, usb_event.data, usb_event.length)) {
            stall_control();
            return;
        }
        if (platform_usb_send(USB_CONTROL_ENDPOINT, 0, 0, true)) {
            control_stage = USB_CONTROL_STAGE_STATUS_IN;
        } else {
            stall_control();
        }
        return;
    }
    if (control_stage == USB_CONTROL_STAGE_PLAYSTATION_AUTHENTICATION_OUT) {
        if (usb_event.length != USB_PLAYSTATION_AUTHENTICATION_REPORT_SIZE) {
            stall_control();
            return;
        }
        (void)usb_playstation_authentication_receive(&console_workspace.playstation.authentication,
                                                     usb_event.data);
        if (platform_usb_send(USB_CONTROL_ENDPOINT, 0, 0, true)) {
            control_stage = USB_CONTROL_STAGE_STATUS_IN;
        } else {
            stall_control();
        }
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

/**
 * @brief Captures and rearms one HID output transfer.
 *
 * Uses the first byte as the report identifier for native and PlayStation HID modes. PlayStation
 * output is exposed as the complete zero-padded 64-byte receive buffer used by the command service.
 */
static void handle_hid_output(void) {
    if (usb_event.length != 0) {
        uint8_t report_id = operating_mode == USB_OPERATING_MODE_PLAYSTATION ||
                                    input_mode == USB_INPUT_REPORT_MODE_FANATEC
                                ? usb_event.data[0]
                                : 0;
        store_output_report(USB_DEVICE_HID_REPORT_OUTPUT, report_id, usb_event.data,
                            usb_event.length);
        if (operating_mode == USB_OPERATING_MODE_PLAYSTATION) {
            output_report.length = USB_DEVICE_REPORT_SIZE;
        }
    }
    uint8_t endpoint = operating_mode == USB_OPERATING_MODE_PLAYSTATION
                           ? USB_PLAYSTATION_OUTPUT_ENDPOINT
                           : USB_PRIMARY_ENDPOINT;
    if (platform_usb_receive(endpoint, USB_DEVICE_REPORT_SIZE, output_data_one)) {
        output_data_one = !output_data_one;
    }
}

/**
 * @brief Captures an Xbox GIP request packet.
 *
 * Zero-fills the 64-byte request workspace, copies the received endpoint payload, and rearms the
 * next endpoint 1 output bank.
 *
 */
static void handle_xbox_output(void) {
    if (!xbox_request_ready) {
        for (uint8_t index = 0; index < USB_XBOX_GIP_METADATA_PACKET_SIZE; index++) {
            xbox_request[index] = 0;
        }
        for (uint8_t index = 0; index < usb_event.length; index++) {
            xbox_request[index] = usb_event.data[index];
        }
        xbox_request_ready = true;
    }
    if (platform_usb_receive(USB_PRIMARY_ENDPOINT, USB_DEVICE_REPORT_SIZE, output_data_one)) {
        output_data_one = !output_data_one;
    }
}

/**
 * @brief Captures one motor-updater output packet.
 *
 * Stores the received endpoint 3 payload for the updater protocol service and rearms the completed
 * output bank for another 64-byte transfer.
 */
static void handle_updater_output(void) {
    updater_packet.length = usb_event.length;
    for (uint8_t index = 0; index < usb_event.length; index++) {
        updater_packet.data[index] = usb_event.data[index];
    }
    updater_packet_ready = true;
    if (platform_usb_receive(USB_UPDATER_DATA_ENDPOINT, USB_DEVICE_REPORT_SIZE,
                             updater_output_data_one)) {
        updater_output_data_one = !updater_output_data_one;
    }
}

static void handle_reset(void) {
    reset_state();
    platform_usb_control_ready();
}

/**
 * @brief Dispatches one USB controller event.
 *
 * Routes control traffic, mode-specific output endpoints, input completion, reset, and suspend
 * notifications to their owning device services.
 */
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
        } else if (usb_event.endpoint == USB_PRIMARY_ENDPOINT) {
            if (operating_mode == USB_OPERATING_MODE_XBOX_GIP) {
                handle_xbox_output();
            } else {
                handle_hid_output();
            }
        } else if (operating_mode == USB_OPERATING_MODE_PLAYSTATION &&
                   usb_event.endpoint == USB_PLAYSTATION_OUTPUT_ENDPOINT) {
            handle_hid_output();
        } else if (operating_mode == USB_OPERATING_MODE_UPDATER &&
                   usb_event.endpoint == USB_UPDATER_DATA_ENDPOINT) {
            handle_updater_output();
        }
        break;
    case PLATFORM_USB_EVENT_IN_COMPLETE:
        if (usb_event.endpoint == USB_CONTROL_ENDPOINT) {
            handle_control_input_complete();
        } else if (operating_mode == USB_OPERATING_MODE_XBOX_GIP &&
                   usb_event.endpoint == USB_PRIMARY_ENDPOINT) {
            xbox_input_busy = false;
        } else if (operating_mode == USB_OPERATING_MODE_UPDATER &&
                   usb_event.endpoint == USB_UPDATER_DATA_ENDPOINT) {
            updater_input_busy = false;
        }
        break;
    case PLATFORM_USB_EVENT_SUSPEND:
        break;
    }
}

/**
 * @brief Services the Xbox GIP endpoint exchange.
 *
 * Preserves one response until endpoint 1 can accept it, captures force-feedback application
 * packets for the main device service, and passes all requests through the discovery and session
 * service. A received request can be processed while a prior input transfer is still completing.
 *
 */
static void service_xbox_gip(void) {
    if (operating_mode != USB_OPERATING_MODE_XBOX_GIP || !usb_device_configured()) {
        return;
    }
    if (!xbox_response_ready && (!xbox_input_busy || xbox_request_ready)) {
        UsbXboxGipServiceResult result = usb_xbox_gip_service_poll(
            &xbox_service, &xbox_service_identity, xbox_request, platform_time_ms(), xbox_response);
        if (result.application_output) {
            store_output_report(USB_DEVICE_HID_REPORT_OUTPUT, 0, xbox_request,
                                USB_DEVICE_REPORT_SIZE);
        }
        xbox_request_ready = false;
        for (uint8_t index = 0; index < USB_XBOX_GIP_METADATA_PACKET_SIZE; index++) {
            xbox_request[index] = 0;
        }
        xbox_session_actions |= result.session_actions;
        if (result.response_length == 0) {
            return;
        }
        xbox_response_length = result.response_length;
        xbox_response_ready = true;
    }
    if (xbox_input_busy) {
        return;
    }
    if (platform_usb_send(USB_PRIMARY_ENDPOINT, xbox_response, xbox_response_length,
                          input_data_one)) {
        input_data_one = !input_data_one;
        xbox_response_ready = false;
        xbox_input_busy = true;
    }
}

/**
 * @brief Advances the motor-updater input stream.
 *
 * Sends at most one endpoint 3 packet while preserving the remaining response until each input
 * transfer completes. Responses are split into 64-byte chunks, and an exact 64-byte response is
 * terminated by a zero-length packet.
 */
static void service_updater_input(void) {
    if (operating_mode != USB_OPERATING_MODE_UPDATER || !usb_device_configured() ||
        updater_input_busy || !updater_response_ready) {
        return;
    }

    uint8_t remaining = updater_response_length - updater_response_offset;
    uint8_t length = remaining > USB_DEVICE_REPORT_SIZE ? USB_DEVICE_REPORT_SIZE : remaining;
    const uint8_t *data = length == 0 ? NULL : &updater_response[updater_response_offset];
    if (!platform_usb_send(USB_UPDATER_DATA_ENDPOINT, data, length, updater_input_data_one)) {
        return;
    }

    updater_input_data_one = !updater_input_data_one;
    updater_input_busy = true;
    updater_response_offset += length;
    if (updater_response_offset != updater_response_length) {
        return;
    }
    if (updater_zero_length_pending) {
        updater_zero_length_pending = false;
        return;
    }
    updater_response_ready = false;
}

/**
 * @brief Services pending USB controller and mode-specific transfers.
 *
 * Drains controller events, then advances Xbox GIP and motor-updater input exchanges.
 */
void usb_device_service(void) {
    while (platform_usb_take_event(&usb_event)) {
        handle_event();
    }
    service_xbox_gip();
    service_updater_input();
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

/**
 * @brief Sends a primary native or compatibility HID input report.
 *
 * Suppresses an unchanged report and submits a changed payload through endpoint one. Console modes
 * use their dedicated input services and are not accepted here.
 *
 * @param[in] report Encoded HID input report.
 * @param[in] length Number of report bytes from one through 64.
 * @return True when the report was unchanged or accepted by the active endpoint; otherwise false.
 */
bool usb_device_send_input(const uint8_t *report, uint8_t length) {
    if (operating_mode > USB_OPERATING_MODE_G27 || !usb_device_configured() || report == 0 ||
        length == 0 || length > USB_DEVICE_REPORT_SIZE) {
        return false;
    }
    if (input_report_matches(report, length)) {
        return true;
    }
    if (!platform_usb_send(USB_PRIMARY_ENDPOINT, report, length, input_data_one)) {
        return false;
    }
    for (uint8_t index = 0; index < length; index++) {
        input_report[index] = report[index];
    }
    input_report_length = length;
    input_data_one = !input_data_one;
    return true;
}

/**
 * @brief Queues the current Xbox GIP controller state.
 *
 * Encodes one state packet with the next shared sequence value. A queued packet can wait behind an
 * active endpoint transfer, but it does not replace a pending discovery, session, or state reply.
 *
 * @param[in] snapshot Current logical Xbox controller state.
 * @return True when the state packet was queued.
 */
bool usb_device_queue_xbox_input(const UsbXboxGipInputSnapshot *snapshot) {
    if (operating_mode != USB_OPERATING_MODE_XBOX_GIP || !usb_device_configured() ||
        xbox_service.session.state != USB_XBOX_GIP_SESSION_ACTIVE || snapshot == 0 ||
        xbox_response_ready) {
        return false;
    }
    uint8_t sequence = usb_xbox_gip_sequence_take(&xbox_service.next_sequence);
    usb_xbox_gip_input_response_encode(sequence, snapshot, xbox_response);
    xbox_response_length = USB_XBOX_GIP_INPUT_RESPONSE_SIZE;
    xbox_response_ready = true;
    return true;
}

/**
 * @brief Queues the Xbox GIP wheel capability response.
 *
 * Encodes the fixed type-21 capability packet with the next shared sequence and retains it until
 * endpoint 1 accepts the transfer. Existing endpoint responses keep priority.
 *
 * @return True when the capability response was queued.
 */
bool usb_device_queue_xbox_capabilities(void) {
    if (operating_mode != USB_OPERATING_MODE_XBOX_GIP || !usb_device_configured() ||
        xbox_service.session.state != USB_XBOX_GIP_SESSION_ACTIVE || xbox_response_ready) {
        return false;
    }
    uint8_t sequence = usb_xbox_gip_sequence_take(&xbox_service.next_sequence);
    usb_xbox_gip_capability_response_encode(sequence, xbox_response);
    xbox_response_length = USB_XBOX_GIP_CAPABILITY_RESPONSE_SIZE;
    xbox_response_ready = true;
    return true;
}

/**
 * @brief Queues the Xbox GIP attached-device status response.
 *
 * Encodes the current logical base and attached-device state with the next shared sequence and
 * retains it until endpoint 1 accepts the transfer. Existing endpoint responses keep priority.
 *
 * @param[in] status Current logical attached-device status.
 * @return True when the extended-status response was queued.
 */
bool usb_device_queue_xbox_extended_status(const UsbXboxGipExtendedStatus *status) {
    if (operating_mode != USB_OPERATING_MODE_XBOX_GIP || !usb_device_configured() ||
        xbox_service.session.state != USB_XBOX_GIP_SESSION_ACTIVE || status == NULL ||
        xbox_response_ready) {
        return false;
    }
    uint8_t sequence = usb_xbox_gip_sequence_take(&xbox_service.next_sequence);
    usb_xbox_gip_extended_status_response_encode(sequence, status, xbox_response);
    xbox_response_length = USB_XBOX_GIP_EXTENDED_STATUS_RESPONSE_SIZE;
    xbox_response_ready = true;
    return true;
}

/**
 * @brief Queues an Xbox GIP command transfer-status response.
 *
 * Echoes the triggering packet type and group with the current shared sequence without consuming
 * it, then retains the response until endpoint 1 accepts the transfer. Existing responses keep
 * priority.
 *
 * @param[in] request First two bytes of the triggering command packet.
 * @return True when the transfer-status response was queued.
 */
bool usb_device_queue_xbox_transfer_status(const uint8_t request[2]) {
    if (operating_mode != USB_OPERATING_MODE_XBOX_GIP || !usb_device_configured() ||
        xbox_service.session.state != USB_XBOX_GIP_SESSION_ACTIVE || request == NULL ||
        xbox_response_ready) {
        return false;
    }
    usb_xbox_gip_transfer_status_response_encode(xbox_service.next_sequence, request,
                                                 xbox_response);
    xbox_response_length = USB_XBOX_GIP_TRANSFER_STATUS_RESPONSE_SIZE;
    xbox_response_ready = true;
    return true;
}

/**
 * @brief Queues one Xbox GIP application response.
 *
 * Copies the complete response, replaces its envelope sequence with the next shared GIP sequence,
 * and retains it until endpoint 1 accepts the transfer. Discovery, session, and input responses
 * already waiting on the endpoint keep priority.
 *
 * @param[in] report Complete application response packet.
 * @param[in] length Number of response bytes from three through 64.
 * @return True when the application response was queued.
 */
bool usb_device_queue_xbox_response(const uint8_t *report, uint8_t length) {
    if (operating_mode != USB_OPERATING_MODE_XBOX_GIP || !usb_device_configured() ||
        xbox_service.session.state != USB_XBOX_GIP_SESSION_ACTIVE || report == NULL || length < 3 ||
        length > USB_DEVICE_REPORT_SIZE || xbox_response_ready) {
        return false;
    }
    for (uint8_t index = 0; index < length; index++) {
        xbox_response[index] = report[index];
    }
    xbox_response[2] = usb_xbox_gip_sequence_take(&xbox_service.next_sequence);
    xbox_response_length = length;
    xbox_response_ready = true;
    return true;
}

/**
 * @brief Queues one raw Xbox vendor report.
 *
 * Copies all 64 report bytes without changing its marker, report identifier, or type fields and
 * retains it until endpoint 1 accepts the transfer. Existing endpoint responses keep priority.
 *
 * @param[in] report Complete raw Xbox vendor report.
 * @return True when the vendor report was queued.
 */
bool usb_device_queue_xbox_vendor_report(const uint8_t report[USB_DEVICE_REPORT_SIZE]) {
    if (operating_mode != USB_OPERATING_MODE_XBOX_GIP || !usb_device_configured() ||
        xbox_service.session.state != USB_XBOX_GIP_SESSION_ACTIVE || report == NULL ||
        xbox_response_ready) {
        return false;
    }
    for (uint8_t index = 0; index < USB_DEVICE_REPORT_SIZE; index++) {
        xbox_response[index] = report[index];
    }
    xbox_response_length = USB_DEVICE_REPORT_SIZE;
    xbox_response_ready = true;
    return true;
}

/**
 * @brief Sends one native vendor HID report.
 *
 * Submits the requested bytes to endpoint 1 without comparing them with prior input, so repeated
 * vendor responses remain observable by the host.
 *
 * @param[in] report Native vendor response bytes.
 * @param[in] length Number of response bytes from one through 64.
 * @return True when the configured native HID endpoint accepted the transfer.
 */
bool usb_device_send_vendor_report(const uint8_t *report, uint8_t length) {
    if (operating_mode != USB_OPERATING_MODE_FANATEC || !usb_device_configured() || report == 0 ||
        length == 0 || length > USB_DEVICE_REPORT_SIZE ||
        !platform_usb_send(USB_PRIMARY_ENDPOINT, report, length, input_data_one)) {
        return false;
    }
    input_data_one = !input_data_one;
    return true;
}

/**
 * @brief Takes one received motor-updater packet.
 *
 * Transfers ownership of the pending endpoint 3 output payload to the caller.
 *
 * @param[out] packet Destination for the packet bytes and length.
 * @return True when a packet was available; otherwise false.
 */
bool usb_device_take_updater_packet(UsbDeviceUpdaterPacket *packet) {
    if (!updater_packet_ready || packet == 0) {
        return false;
    }
    *packet = updater_packet;
    updater_packet_ready = false;
    return true;
}

/**
 * @brief Reports whether the motor-updater input stream can accept a response.
 *
 * Requires no retained response and no endpoint 3 input transfer awaiting completion.
 *
 * @return True when updater protocol service may process its next request; otherwise false.
 */
bool usb_device_updater_channel_idle(void) {
    return !updater_response_ready && !updater_input_busy;
}

/**
 * @brief Queues one complete motor-updater response.
 *
 * Retains responses of up to 66 bytes while the endpoint service emits 64-byte bulk packets and
 * the required zero-length terminator for an exact 64-byte response.
 *
 * @param[in] data Complete response bytes to retain.
 * @param[in] length Number of response bytes from one through 66.
 * @return True when the idle updater input stream accepted the response; otherwise false.
 */
bool usb_device_queue_updater_response(const uint8_t *data, uint8_t length) {
    if (operating_mode != USB_OPERATING_MODE_UPDATER || !usb_device_configured() || data == NULL ||
        length == 0 || length > USB_DEVICE_UPDATER_RESPONSE_SIZE || updater_response_ready ||
        updater_input_busy) {
        return false;
    }

    for (uint8_t index = 0; index < length; index++) {
        updater_response[index] = data[index];
    }
    updater_response_length = length;
    updater_response_offset = 0;
    updater_zero_length_pending = length == USB_DEVICE_REPORT_SIZE;
    updater_response_ready = true;
    service_updater_input();
    return true;
}

/**
 * @brief Takes pending Xbox GIP session actions.
 *
 * Returns every accepted session action accumulated since the previous take and clears the
 * pending action set.
 *
 * @return Pending Xbox GIP session actions.
 */
UsbXboxGipSessionAction usb_device_take_xbox_session_actions(void) {
    UsbXboxGipSessionAction actions = xbox_session_actions;
    xbox_session_actions = USB_XBOX_GIP_SESSION_ACTION_NONE;
    return actions;
}

/**
 * @brief Takes a completed PlayStation authentication request.
 *
 * Copies the assembled 256-byte host challenge once for processing by the secure-element service.
 *
 * @param[out] request Completed authentication challenge.
 * @return True when a completed request was available; otherwise false.
 */
bool usb_device_take_playstation_authentication_request(
    uint8_t request[USB_PLAYSTATION_AUTHENTICATION_REQUEST_SIZE]) {
    return operating_mode == USB_OPERATING_MODE_PLAYSTATION &&
           usb_playstation_authentication_take_request(
               &console_workspace.playstation.authentication, request);
}

/**
 * @brief Publishes a PlayStation authentication response.
 *
 * Makes the exact 1,040-byte secure-element response available through feature report F1.
 *
 * @param[in] response Complete authentication response.
 * @param[in] response_length Number of response bytes.
 * @return True when the response was accepted in PlayStation mode; otherwise false.
 */
bool usb_device_publish_playstation_authentication_response(const uint8_t *response,
                                                            uint16_t response_length) {
    return operating_mode == USB_OPERATING_MODE_PLAYSTATION &&
           usb_playstation_authentication_publish_response(
               &console_workspace.playstation.authentication, response, response_length);
}

/**
 * @brief Reports whether PlayStation response fragments remain available.
 *
 * Keeps the response owner active until the host consumes the final F1 feature report.
 *
 * @return True while a PlayStation response is being retrieved; otherwise false.
 */
bool usb_device_playstation_authentication_response_active(void) {
    return operating_mode == USB_OPERATING_MODE_PLAYSTATION &&
           usb_playstation_authentication_response_active(
               &console_workspace.playstation.authentication);
}

/**
 * @brief Reports a PlayStation authentication failure.
 *
 * Changes feature report F2 to the response-error state while PlayStation mode is active.
 */
void usb_device_fail_playstation_authentication(void) {
    if (operating_mode == USB_OPERATING_MODE_PLAYSTATION) {
        usb_playstation_authentication_fail(&console_workspace.playstation.authentication);
    }
}

/**
 * @brief Encodes and sends the current PlayStation input state.
 *
 * Suppresses an unchanged 64-byte report and submits a changed report through endpoint four while
 * the PlayStation interface is configured.
 *
 * @param[in] state Current logical PlayStation controls and axes.
 * @return True when the report was unchanged or accepted by endpoint four; otherwise false.
 */
bool usb_device_send_playstation_input(const UsbPlaystationInputState *state) {
    uint8_t *report = console_workspace.playstation.feature_report;
    if (operating_mode != USB_OPERATING_MODE_PLAYSTATION || !usb_device_configured() ||
        !usb_playstation_input_encode(report, state)) {
        return false;
    }
    if (input_report_matches(report, USB_PLAYSTATION_INPUT_REPORT_SIZE)) {
        return true;
    }
    if (!platform_usb_send(USB_PLAYSTATION_INPUT_ENDPOINT, report,
                           USB_PLAYSTATION_INPUT_REPORT_SIZE, input_data_one)) {
        return false;
    }
    for (uint8_t index = 0; index < USB_PLAYSTATION_INPUT_REPORT_SIZE; index++) {
        input_report[index] = report[index];
    }
    input_report_length = USB_PLAYSTATION_INPUT_REPORT_SIZE;
    input_data_one = !input_data_one;
    return true;
}
