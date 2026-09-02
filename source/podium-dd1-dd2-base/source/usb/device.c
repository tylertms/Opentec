#include "usb/device.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

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

/** @brief USB endpoint, descriptor, and string-layout constants. */
enum {
    USB_CONTROL_ENDPOINT = 0,              /**< Endpoint-zero control pipe. */
    USB_PRIMARY_ENDPOINT = 1,              /**< Primary application endpoint. */
    USB_UPDATER_NOTIFICATION_ENDPOINT = 2, /**< Updater notification endpoint. */
    USB_UPDATER_DATA_ENDPOINT = 3,         /**< Updater bulk data endpoint. */
    USB_PLAYSTATION_OUTPUT_ENDPOINT = 3,   /**< PlayStation output endpoint. */
    USB_PLAYSTATION_INPUT_ENDPOINT = 4,    /**< PlayStation input endpoint. */
    USB_RECIPIENT_ENDPOINT = 2,            /**< Endpoint request recipient value. */
    USB_ENDPOINT_DIRECTION_IN = 0x80,      /**< Device-to-host endpoint direction bit. */
    USB_ENDPOINT_NUMBER_MASK = 0x0f,       /**< Endpoint-number bit mask. */
    USB_HID_DESCRIPTOR_OFFSET = 18, /**< HID descriptor offset in the configuration descriptor. */
    USB_HID_DESCRIPTOR_SIZE = 9,    /**< HID descriptor size in bytes. */
    USB_STRING_COUNT = 10,          /**< Number of string descriptor slots. */
    USB_MANUFACTURER_DESCRIPTOR_SIZE = 16,        /**< Manufacturer descriptor buffer size. */
    USB_PRODUCT_DESCRIPTOR_SIZE = 60,             /**< Native product descriptor buffer size. */
    USB_G27_HARDWARE_DESCRIPTOR_SIZE = 96,        /**< G27 hardware string buffer size. */
    USB_PLAYSTATION_PRODUCT_DESCRIPTOR_SIZE = 96, /**< PlayStation product descriptor size. */
};

/** @brief Endpoint-zero transfer stages used by the USB device service. */
typedef enum {
    USB_CONTROL_STAGE_IDLE,                           /**< No endpoint-zero transfer is active. */
    USB_CONTROL_STAGE_DATA_IN,                        /**< Device-to-host control data stage. */
    USB_CONTROL_STAGE_DATA_OUT,                       /**< Host-to-device control data stage. */
    USB_CONTROL_STAGE_PLAYSTATION_AUTHENTICATION_OUT, /**< PlayStation authentication data stage. */
    USB_CONTROL_STAGE_UPDATER_LINE_CODING_OUT,        /**< Updater line-coding data stage. */
    USB_CONTROL_STAGE_UPDATER_ENCAPSULATED_OUT,
    USB_CONTROL_STAGE_STATUS_IN,  /**< Device-to-host control status stage. */
    USB_CONTROL_STAGE_STATUS_OUT, /**< Host-to-device control status stage. */
} UsbControlStage;

/** @brief Encoded USB device descriptor storage. */
static uint8_t device_descriptor[USB_DEVICE_DESCRIPTOR_SIZE];
/** @brief Encoded active configuration descriptor storage. */
static uint8_t configuration_descriptor[USB_UPDATER_CONFIGURATION_DESCRIPTOR_SIZE];
/** @brief Encoded active HID report descriptor storage. */
static uint8_t report_descriptor[USB_PODIUM_REPORT_DESCRIPTOR_SIZE];
/** @brief Encoded USB language descriptor storage. */
static uint8_t language_descriptor[4];
/** @brief Encoded manufacturer string descriptor storage. */
static uint8_t manufacturer_descriptor[USB_MANUFACTURER_DESCRIPTOR_SIZE];
/** @brief Encoded native product string descriptor storage. */
static uint8_t product_descriptor[USB_PRODUCT_DESCRIPTOR_SIZE];
/** @brief Encoded G27 hardware identity string descriptor storage. */
static uint8_t g27_hardware_descriptor[USB_G27_HARDWARE_DESCRIPTOR_SIZE];
/** @brief Encoded compatibility-mode PlayStation product descriptor storage. */
static uint8_t compatibility_playstation_descriptor[USB_PLAYSTATION_PRODUCT_DESCRIPTOR_SIZE];
/** @brief Active USB string descriptor views. */
static UsbDescriptorView strings[USB_STRING_COUNT];
/** @brief Active descriptor catalog supplied to endpoint-zero handling. */
static UsbDescriptorCatalog descriptor_catalog;
/** @brief Active USB device identity used for enumeration. */
static UsbDeviceIdentity descriptor_identity;
/** @brief Active primary HID configuration used for enumeration. */
static UsbHidConfiguration hid_configuration;
/** @brief Endpoint-zero USB device state. */
static UsbDeviceControl device_control;
/** @brief Endpoint-zero input packetizer state. */
static UsbControlPipe control_pipe;
/** @brief Current endpoint-zero input packet. */
static UsbControlPacket control_packet;
/** @brief Decoded setup packet currently being handled. */
static UsbSetupPacket setup_packet;
/** @brief Classified control request currently being handled. */
static UsbControlRequest control_request;
/** @brief Transfer selected for the current control request. */
static UsbControlTransfer control_transfer;
/** @brief Most recently received platform USB event. */
static PlatformUsbEvent usb_event;
/** @brief Pending host HID output report. */
static UsbDeviceOutputReport output_report;
/** @brief Scalar value bytes used by endpoint-zero responses. */
static uint8_t value_data[2];
/** @brief Updater CDC line-coding bytes. */
static uint8_t updater_line_coding[USB_UPDATER_LINE_CODING_SIZE];
/** @brief Encoded Xbox GIP security descriptor. */
static uint8_t xbox_security_descriptor[USB_XBOX_GIP_SECURITY_DESCRIPTOR_SIZE];
/** @brief Encoded Xbox Microsoft OS string descriptor. */
static uint8_t xbox_os_string_descriptor[USB_XBOX_GIP_OS_STRING_DESCRIPTOR_SIZE];
/** @brief Attached-wheel digest used by Xbox identity and metadata. */
static uint8_t xbox_digest[USB_XBOX_GIP_DIGEST_SIZE];
/** @brief Pending Xbox GIP request packet. */
static uint8_t xbox_request[USB_XBOX_GIP_METADATA_PACKET_SIZE];
/** @brief Pending Xbox GIP response packet. */
static uint8_t xbox_response[USB_XBOX_GIP_METADATA_PACKET_SIZE];
/** @brief Encoded Xbox serial text. */
static char xbox_serial[USB_XBOX_GIP_SERIAL_SIZE];
/** @brief Encoded Xbox serial string descriptor. */
static uint8_t xbox_serial_descriptor[USB_XBOX_GIP_SERIAL_TEXT_SIZE * 2 + 2];
/** @brief Length of the pending Xbox response packet. */
static uint8_t xbox_response_length;
/** @brief Retained primary HID input report. */
static uint8_t input_report[USB_DEVICE_REPORT_SIZE];
/** @brief Retained native feature-report payloads by report slot. */
static uint8_t feature_reports[5][USB_DEVICE_REPORT_SIZE];
/** @brief Lengths of retained native feature reports by report slot. */
static uint8_t feature_report_lengths[5];
/** @brief Native feature report identifiers indexed by report slot. */
static const uint8_t feature_report_ids[5] = {0x31, 0x32, 0x33, 0x35, 0x36};
/** @brief One-shot native feature-report request bits. */
static uint8_t feature_report_requests;
/** @brief Official sparse queue backing native remote-HID report 0x35. */
static UsbRemoteHidQueue remote_hid_queue;
/** @brief Retained updater response bytes. */
static uint8_t updater_response[USB_DEVICE_UPDATER_RESPONSE_SIZE];
/** @brief Length of the retained primary input report. */
static uint8_t input_report_length;
/** @brief Length of the retained updater response. */
static uint8_t updater_response_length;
/** @brief Offset of the next updater response byte to send. */
static uint8_t updater_response_offset;
/** @brief Length of the current updater output packet. */
static uint8_t updater_input_length;
/** @brief HID report type for the current control output stage. */
static uint8_t control_report_type;
/** @brief HID report identifier for the current control output stage. */
static uint8_t control_report_id;
static uint8_t control_output[USB_DEVICE_REPORT_SIZE];
static uint8_t control_output_expected;
static uint8_t control_output_received;
static bool control_output_data_one;
/** @brief True while the endpoint-zero DATA1 input status packet is prearmed. */
static bool control_status_in_armed;
/** @brief Current endpoint-zero transfer stage. */
static UsbControlStage control_stage;
/** @brief True when a host output report is ready to take. */
static bool output_ready;
/** @brief Head index of the updater receive queue. */
static uint8_t updater_packet_head;
/** @brief Number of packets retained in the updater receive queue. */
static uint8_t updater_packet_count;
/** @brief True when an updater response is ready for transmission. */
static bool updater_response_ready;
/** @brief True while an updater input transfer is active. */
static bool updater_input_busy;
/** @brief True when an updater response requires a zero-length terminator. */
static bool updater_zero_length_pending;
/** @brief Data toggle for the primary input endpoint. */
static bool input_data_one;
/** @brief Data toggle for the primary output endpoint. */
static bool output_data_one;
/** @brief Data toggle for the updater input endpoint. */
static bool updater_input_data_one;
/** @brief Data toggle for the updater output endpoint. */
static bool updater_output_data_one;
/** @brief True when Xbox identity data is ready for enumeration. */
static bool xbox_identity_ready;
/** @brief True when an Xbox request packet is ready for application processing. */
static bool xbox_request_ready;
/** @brief True when an Xbox response packet is ready for transmission. */
static bool xbox_response_ready;
/** @brief True while an Xbox input transfer is active. */
static bool xbox_input_busy;
/** @brief True when a PlayStation remote-tuning report is ready. */
static bool playstation_remote_tuning_ready;
/** @brief Retained PlayStation remote-tuning report. */
static uint8_t playstation_remote_tuning_report[USB_DEVICE_REPORT_SIZE];
/** @brief Selected wheel-base hardware variant. */
static BoardVariant board_variant;
/** @brief Selected PlayStation wheel mode. */
static uint8_t playstation_wheel_mode;
/** @brief Active native or compatibility input-report mode. */
static UsbInputReportMode input_mode;
/** @brief Active USB operating mode. */
static UsbOperatingMode operating_mode;
/** @brief Motor-updater control state. */
static UsbUpdaterControl updater_control;
/** @brief Two-packet updater receive queue. */
static UsbDeviceUpdaterPacket updater_packets[2];
/** @brief Xbox GIP endpoint service state. */
static UsbXboxGipService xbox_service;
/** @brief Xbox GIP service identity and metadata views. */
static UsbXboxGipServiceIdentity xbox_service_identity;
/** @brief Pending Xbox GIP session actions. */
static UsbXboxGipSessionAction xbox_session_actions;

static void apply_configuration(void);

/** @brief Storage used only by PlayStation USB mode. */
typedef struct {
    UsbPlaystationAuthentication authentication; /**< PlayStation authentication transport state. */
    uint8_t feature_report[USB_PLAYSTATION_AUTHENTICATION_REPORT_SIZE];  /**< Authentication
                                                                            feature-report buffer. */
    uint8_t product_descriptor[USB_PLAYSTATION_PRODUCT_DESCRIPTOR_SIZE]; /**< PlayStation product
                                                                            descriptor. */
} UsbPlaystationWorkspace;

/** @brief Storage shared by mutually exclusive Xbox and PlayStation USB modes. */
typedef union {
    uint8_t xbox_metadata[USB_XBOX_GIP_METADATA_SIZE]; /**< Xbox metadata descriptor storage. */
    UsbPlaystationWorkspace playstation;               /**< PlayStation-only workspace storage. */
} UsbConsoleWorkspace;

/** @brief Shared Xbox and PlayStation console workspace storage. */
static UsbConsoleWorkspace console_workspace;

/**
 * @brief Compares a primary input report with the retained report.
 *
 * Requires equal lengths and equal bytes so unchanged HID state can be suppressed.
 *
 * @param[in] report Candidate input report.
 * @param[in] length Number of candidate bytes.
 * @return True when the candidate equals the retained report; otherwise false.
 */
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
 * enumeration. G27 mode publishes its product at index two and exposes its hardware identity only
 * through the index FE alias. PlayStation mode uses the dedicated HID profile, product string
 * index nine, and the product identity selected by retained base mode two, four, or five.
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
            .device_version = 0x0059,
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
        product_index = 3;
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
        descriptor_identity =
            usb_playstation_device_identity_for_mode(variant, playstation_wheel_mode);
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
    if (mode == USB_OPERATING_MODE_G27) {
        size_t hardware_length =
            usb_string_descriptor_encode("NV=046D,NP=C29B,ND=1238,HV=046D,HP=FE01,HD=0005",
                                         g27_hardware_descriptor, sizeof(g27_hardware_descriptor));
        strings[4] = (UsbDescriptorView){
            .data = g27_hardware_descriptor,
            .length = (uint16_t)hardware_length,
        };
    }
    if (mode == USB_OPERATING_MODE_FANATEC_COMPATIBILITY) {
        size_t playstation_length = usb_string_descriptor_encode(
            usb_playstation_product_name(variant), compatibility_playstation_descriptor,
            sizeof(compatibility_playstation_descriptor));
        strings[9] = (UsbDescriptorView){
            .data = compatibility_playstation_descriptor,
            .length = (uint16_t)playstation_length,
        };
    }
    if (mode == USB_OPERATING_MODE_XBOX_GIP) {
        size_t serial_length = usb_string_descriptor_encode(xbox_serial, xbox_serial_descriptor,
                                                            sizeof(xbox_serial_descriptor));
        strings[descriptor_identity.serial_string] = (UsbDescriptorView){
            .data = xbox_serial_descriptor,
            .length = (uint16_t)serial_length,
        };
    }
    uint16_t string_mask = UINT16_MAX;
    if (mode == USB_OPERATING_MODE_FANATEC) {
        string_mask = (1u << 0) | (1u << 1) | (1u << 3);
    } else if (mode == USB_OPERATING_MODE_FANATEC_COMPATIBILITY) {
        string_mask = (1u << 0) | (1u << 1) | (1u << 8) | (1u << 9);
    } else if (mode >= USB_OPERATING_MODE_DRIVING_FORCE_EX && mode < USB_OPERATING_MODE_G27) {
        string_mask = 1u << 2;
    } else if (mode == USB_OPERATING_MODE_G27) {
        string_mask = (1u << 2) | (1u << 4);
    } else if (mode == USB_OPERATING_MODE_UPDATER) {
        string_mask = (1u << 1) | (1u << 3);
    } else if (mode == USB_OPERATING_MODE_XBOX_GIP) {
        string_mask = (1u << 0) | (1u << 1) | (1u << 2) | (1u << 3);
    } else if (mode == USB_OPERATING_MODE_PLAYSTATION) {
        string_mask = (1u << 0) | (1u << 1) | (1u << 9);
    }
    for (uint8_t index = 0; index < USB_STRING_COUNT; index++) {
        if ((string_mask & (1u << index)) == 0) {
            strings[index] = (UsbDescriptorView){0};
        }
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
        .string_alias_valid = mode == USB_OPERATING_MODE_G27,
        .string_alias = 0xfe,
        .string_alias_target = 4,
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
 * catalog and PlayStation authentication payload. Bus and mode resets retain the host-selected HID
 * idle rate and protocol; cold preparation initializes both values to zero.
 *
 * @param[in] preserve_hid_state True to retain HID idle and protocol selections.
 */
static void reset_state(bool preserve_hid_state) {
    uint8_t hid_idle_rate = device_control.hid_idle_rate;
    uint8_t hid_protocol = device_control.hid_protocol;
    usb_device_control_init(&device_control, true, false);
    if (preserve_hid_state) {
        device_control.hid_idle_rate = hid_idle_rate;
        device_control.hid_protocol = hid_protocol;
    }
    control_stage = USB_CONTROL_STAGE_IDLE;
    control_output_expected = 0;
    control_output_received = 0;
    control_status_in_armed = false;
    input_report_length = 0;
    output_ready = false;
    updater_packet_head = 0;
    updater_packet_count = 0;
    updater_response_ready = false;
    updater_input_busy = false;
    updater_zero_length_pending = false;
    updater_response_length = 0;
    updater_response_offset = 0;
    updater_input_length = 0;
    input_data_one = false;
    output_data_one = false;
    updater_input_data_one = false;
    updater_output_data_one = false;
    xbox_request_ready = false;
    xbox_response_ready = false;
    xbox_input_busy = false;
    playstation_remote_tuning_ready = false;
    usb_remote_hid_queue_init(&remote_hid_queue);
    xbox_session_actions = USB_XBOX_GIP_SESSION_ACTION_NONE;
    for (uint8_t index = 0; index < USB_XBOX_GIP_METADATA_PACKET_SIZE; index++) {
        xbox_request[index] = 0;
    }
    usb_xbox_gip_service_init(&xbox_service);
}

/**
 * @brief Prepares one detached wheel-base USB operating mode.
 *
 * Initializes shared console identity data, builds the selected descriptor profile, resets device
 * transfer state, and prepares endpoint zero without attaching the controller to the host.
 *
 * @param[in] variant Wheel-base hardware variant.
 * @param[in] mode Initial USB operating mode.
 * @return True when the selected descriptor profile was prepared; otherwise false.
 */
static bool prepare_device(BoardVariant variant, UsbOperatingMode mode) {
    board_variant = variant;
    playstation_wheel_mode = 4;
    xbox_identity_ready = false;
    usb_xbox_gip_security_descriptor_encode(xbox_security_descriptor);
    usb_xbox_gip_os_string_descriptor_encode(xbox_os_string_descriptor);
    usb_xbox_gip_metadata_encode(console_workspace.xbox_metadata);
    xbox_service_identity = (UsbXboxGipServiceIdentity){
        .variant = variant,
        .digest = xbox_digest,
        .metadata = console_workspace.xbox_metadata,
    };
    if (!build_descriptors(variant, mode)) {
        return false;
    }
    reset_state(false);
    platform_usb_init();
    platform_usb_control_ready();
    return true;
}

void usb_device_prepare(BoardVariant variant) {
    (void)prepare_device(variant, USB_OPERATING_MODE_FANATEC);
}

bool usb_device_prepare_updater(BoardVariant variant) {
    return prepare_device(variant, USB_OPERATING_MODE_UPDATER);
}

void usb_device_init(BoardVariant variant) {
    usb_device_prepare(variant);
    platform_usb_attach();
}

void usb_device_shutdown(void) {
    platform_usb_detach();
    reset_state(false);
}

bool usb_device_set_input_mode(UsbInputReportMode mode) {
    if (mode > USB_INPUT_REPORT_MODE_G27) {
        return false;
    }
    return usb_device_set_operating_mode((UsbOperatingMode)mode);
}

static bool apply_operating_mode(UsbOperatingMode mode, bool restart) {
    if (mode > USB_OPERATING_MODE_PLAYSTATION ||
        (mode == USB_OPERATING_MODE_XBOX_GIP && !xbox_identity_ready)) {
        return false;
    }
    platform_usb_detach();
    if (!build_descriptors(board_variant, mode)) {
        if (restart) {
            platform_usb_attach();
        }
        return false;
    }
    reset_state(true);
    platform_usb_control_ready();
    if (restart) {
        platform_usb_restart();
    }
    return true;
}

bool usb_device_set_operating_mode(UsbOperatingMode mode) {
    return apply_operating_mode(mode, true);
}

bool usb_device_set_playstation_mode(void) { return usb_device_set_playstation_wheel_mode(4); }

bool usb_device_set_playstation_wheel_mode(uint8_t wheel_mode) {
    if (wheel_mode != 2 && wheel_mode != 4 && wheel_mode != 5) {
        return false;
    }
    playstation_wheel_mode = wheel_mode;
    return apply_operating_mode(USB_OPERATING_MODE_PLAYSTATION, true);
}

bool usb_device_prepare_playstation_wheel_mode(uint8_t wheel_mode) {
    if (wheel_mode != 2 && wheel_mode != 4 && wheel_mode != 5) {
        return false;
    }
    playstation_wheel_mode = wheel_mode;
    return apply_operating_mode(USB_OPERATING_MODE_PLAYSTATION, false);
}

static bool apply_xbox_mode(uint8_t wheel_mode, const uint8_t digest[USB_XBOX_GIP_DIGEST_SIZE],
                            bool restart) {
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
    return apply_operating_mode(USB_OPERATING_MODE_XBOX_GIP, restart);
}

bool usb_device_set_xbox_mode(uint8_t wheel_mode, const uint8_t digest[USB_XBOX_GIP_DIGEST_SIZE]) {
    return apply_xbox_mode(wheel_mode, digest, true);
}

bool usb_device_prepare_xbox_mode(uint8_t wheel_mode,
                                  const uint8_t digest[USB_XBOX_GIP_DIGEST_SIZE]) {
    return apply_xbox_mode(wheel_mode, digest, false);
}

UsbInputReportMode usb_device_input_mode(void) { return input_mode; }

UsbOperatingMode usb_device_operating_mode(void) { return operating_mode; }

/**
 * @brief Stalls the active endpoint-zero transfer.
 *
 * Returns the control state to idle and publishes the endpoint-zero input and output stall
 * descriptors.
 */
static void stall_control(void) {
    control_stage = USB_CONTROL_STAGE_IDLE;
    control_status_in_armed = false;
    platform_usb_stall(USB_CONTROL_ENDPOINT);
}

/**
 * @brief Prearms the endpoint-zero DATA1 input status packet.
 *
 * Keeps one status descriptor reserved throughout a host-to-device data stage and avoids
 * reprogramming the descriptor after the final output packet.
 *
 * @return True when the status packet is already or newly armed; otherwise false.
 */
static bool arm_control_status_in(void) {
    if (control_status_in_armed) {
        return true;
    }
    if (!platform_usb_control_arm_status(true)) {
        return false;
    }
    control_status_in_armed = true;
    return true;
}

/**
 * @brief Sends the next endpoint-zero input packet.
 *
 * Takes the next packet from the control pipe and submits it with the pipe-selected data toggle.
 *
 * @return True when a packet was available and accepted; otherwise false.
 */
static bool send_next_control_packet(void) {
    if (!usb_control_pipe_next(&control_pipe, &control_packet)) {
        return false;
    }
    return platform_usb_send(USB_CONTROL_ENDPOINT, control_packet.data.data,
                             (uint8_t)control_packet.data.length, control_packet.data_one);
}

/**
 * @brief Starts an endpoint-zero input data stage.
 *
 * Reserves the output status stage, limits the source to the host-requested length, sends its first
 * packet, and enters the input stage or stalls when either descriptor cannot be submitted.
 *
 * @param[in] data Response bytes and available length.
 * @param[in] requested_length Maximum response length requested by the host.
 */
static void begin_control_input(UsbDescriptorView data, uint16_t requested_length) {
    if (!platform_usb_control_arm_status(false)) {
        stall_control();
        return;
    }
    usb_control_pipe_begin(&control_pipe, data, requested_length);
    if (!send_next_control_packet()) {
        stall_control();
        return;
    }
    control_stage = USB_CONTROL_STAGE_DATA_IN;
}

/**
 * @brief Starts a short endpoint-zero value response.
 *
 * Writes the retained response low byte first and begins its one- or two-byte input data stage.
 */
static void begin_value_input(void) {
    value_data[0] = (uint8_t)control_transfer.value;
    value_data[1] = (uint8_t)(control_transfer.value >> 8);
    begin_control_input((UsbDescriptorView){.data = value_data, .length = control_transfer.length},
                        control_request.length);
}

/**
 * @brief Selects the retained HID report for a control request.
 *
 * Returns the retained input or feature report view selected by the active control request.
 *
 * @return Retained report view, or an empty view when no report satisfies the request.
 */
static UsbDescriptorView requested_report(void) {
    if (control_transfer.report_type == USB_DEVICE_HID_REPORT_INPUT && input_report_length != 0 &&
        (input_mode == USB_INPUT_REPORT_MODE_FANATEC ? input_report[0] == control_transfer.report_id
                                                     : control_transfer.report_id == 0)) {
        return (UsbDescriptorView){.data = input_report, .length = input_report_length};
    }
    if (control_transfer.report_type == USB_DEVICE_HID_REPORT_FEATURE) {
        if (operating_mode == USB_OPERATING_MODE_FANATEC &&
            control_transfer.report_id == USB_REMOTE_HID_MARKER_PLAYSTATION) {
            uint8_t *report = feature_reports[3];
            (void)usb_remote_hid_queue_encode(&remote_hid_queue, USB_REMOTE_HID_HOST_NATIVE,
                                              report);
            feature_report_lengths[3] = USB_DEVICE_REPORT_SIZE;
            feature_report_requests |= 1u << 3;
            return (UsbDescriptorView){.data = report, .length = USB_DEVICE_REPORT_SIZE};
        }
        for (uint8_t index = 0; index < 5; index++) {
            if (feature_report_ids[index] == control_transfer.report_id &&
                feature_report_lengths[index] != 0) {
                feature_report_requests |= (uint8_t)(1u << index);
                return (UsbDescriptorView){.data = feature_reports[index],
                                           .length = feature_report_lengths[index]};
            }
        }
    }
    return (UsbDescriptorView){0};
}

/**
 * @brief Starts an endpoint-zero output data stage.
 *
 * Arms the requested output packet and reserves its DATA1 input status packet before accepting
 * host data.
 *
 * @param[in] stage Control stage to enter after arming.
 * @param[in] length Maximum packet length.
 * @return True when both data and status stages were armed; otherwise false.
 */
static bool begin_control_output(UsbControlStage stage, uint8_t length) {
    control_output_expected = length;
    control_output_received = 0;
    control_output_data_one = true;
    if (!platform_usb_receive(USB_CONTROL_ENDPOINT, length, control_output_data_one)) {
        return false;
    }
    if (!arm_control_status_in()) {
        return false;
    }
    control_stage = stage;
    return true;
}

/**
 * @brief Handles PlayStation feature reports.
 *
 * Builds the fixed report 3, returns and consumes a pending remote-tuning report 35, and builds F1
 * response fragments, F2 status, and F3 format reports for feature reads. A feature write for F0
 * arms one 64-byte authentication output transfer.
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
        if (report_id == 3) {
            uint8_t *report = console_workspace.playstation.feature_report;
            for (uint8_t index = 0; index < USB_PLAYSTATION_AUTHENTICATION_REPORT_SIZE; index++) {
                report[index] = 0;
            }
            report[0] = 3;
            report[1] = 0x21;
            report[2] = 0x27;
            report[3] = 4;
            report[4] = 0x18;
            report[5] = 6;
            report[24] = 0x0f;
            report[25] = 0xd8;
            report[26] = 9;
            report_length = 48;
        } else if (report_id == 0x35) {
            uint8_t *report = console_workspace.playstation.feature_report;
            bool queued = usb_remote_hid_queue_encode(&remote_hid_queue,
                                                      USB_REMOTE_HID_HOST_PLAYSTATION, report);
            if (!queued) {
                memset(report, 0, USB_DEVICE_REPORT_SIZE);
            }
            if (playstation_remote_tuning_ready && !queued) {
                for (uint8_t index = 0; index < USB_DEVICE_REPORT_SIZE; index++) {
                    report[index] = playstation_remote_tuning_report[index];
                }
            }
            if (playstation_remote_tuning_ready) {
                playstation_remote_tuning_ready = false;
            }
            report_length = USB_DEVICE_REPORT_SIZE;
        } else if (report_id == 0xf1) {
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
        if (!begin_control_output(USB_CONTROL_STAGE_PLAYSTATION_AUTHENTICATION_OUT,
                                  USB_PLAYSTATION_AUTHENTICATION_REPORT_SIZE)) {
            stall_control();
        }
        return true;
    }
    return false;
}

/**
 * @brief Restarts an endpoint direction after its halt is cleared.
 *
 * Restores DATA0 as the next transfer, makes interrupted retained input responses eligible for
 * retry, invalidates a cached input report, and rearms both output banks when the selected
 * direction accepts host data.
 *
 * @param[in] endpoint_address Endpoint number with bit seven set for device-to-host direction.
 */
static void restart_endpoint_after_halt(uint8_t endpoint_address) {
    uint8_t endpoint = endpoint_address & USB_ENDPOINT_NUMBER_MASK;
    if ((endpoint_address & USB_ENDPOINT_DIRECTION_IN) != 0) {
        if (endpoint == USB_PRIMARY_ENDPOINT || endpoint == USB_PLAYSTATION_INPUT_ENDPOINT) {
            input_data_one = false;
            input_report_length = 0;
        }
        if (operating_mode == USB_OPERATING_MODE_XBOX_GIP && endpoint == USB_PRIMARY_ENDPOINT &&
            xbox_input_busy) {
            xbox_input_busy = false;
            xbox_response_ready = true;
        }
        if (operating_mode == USB_OPERATING_MODE_UPDATER && endpoint == USB_UPDATER_DATA_ENDPOINT &&
            updater_input_busy) {
            updater_input_busy = false;
            updater_response_offset -= updater_input_length;
            updater_response_ready = true;
            updater_zero_length_pending =
                updater_response_length == USB_DEVICE_REPORT_SIZE && updater_response_offset == 0;
            updater_input_length = 0;
            updater_input_data_one = false;
        }
        return;
    }

    bool *data_one = &output_data_one;
    if (operating_mode == USB_OPERATING_MODE_UPDATER) {
        if (endpoint != USB_UPDATER_DATA_ENDPOINT) {
            return;
        }
        data_one = &updater_output_data_one;
    } else {
        uint8_t output_endpoint = operating_mode == USB_OPERATING_MODE_PLAYSTATION
                                      ? USB_PLAYSTATION_OUTPUT_ENDPOINT
                                      : USB_PRIMARY_ENDPOINT;
        if (endpoint != output_endpoint) {
            return;
        }
    }

    *data_one = false;
    for (uint8_t bank = 0; bank < 2; bank++) {
        if (platform_usb_receive(endpoint, USB_DEVICE_REPORT_SIZE, *data_one)) {
            *data_one = !*data_one;
        }
    }
}

/**
 * @brief Starts the selected endpoint-zero transfer.
 *
 * Dispatches status, descriptor, value, HID input, HID output, and stall results to the controller
 * and records the corresponding control stage.
 */
static void handle_control_transfer(void) {
    switch (control_transfer.kind) {
    case USB_CONTROL_TRANSFER_ACKNOWLEDGE:
        if (arm_control_status_in()) {
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
        control_transfer.data = requested_report();
        if (control_transfer.data.data != 0) {
            begin_control_input(control_transfer.data, control_request.length);
        } else {
            stall_control();
        }
        break;
    case USB_CONTROL_TRANSFER_REPORT_OUT:
        if (control_transfer.length <= USB_DEVICE_REPORT_SIZE &&
            begin_control_output(USB_CONTROL_STAGE_DATA_OUT, (uint8_t)control_transfer.length)) {
            control_report_type = control_transfer.report_type;
            control_report_id = control_transfer.report_id;
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
    control_status_in_armed = false;
    platform_usb_control_reset();
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
        if (control_request.kind == USB_CONTROL_CDC_GET_ENCAPSULATED_RESPONSE) {
            begin_control_input((UsbDescriptorView){.data = value_data, .length = 0},
                                control_request.length);
            return;
        }
        if (control_request.kind == USB_CONTROL_CDC_SEND_ENCAPSULATED_COMMAND) {
            if (control_request.length == 0) {
                if (arm_control_status_in()) {
                    control_stage = USB_CONTROL_STAGE_STATUS_IN;
                } else {
                    stall_control();
                }
            } else if (!begin_control_output(USB_CONTROL_STAGE_UPDATER_ENCAPSULATED_OUT,
                                             (uint8_t)control_request.length)) {
                stall_control();
            }
            return;
        }
        if (control_request.kind == USB_CONTROL_CDC_SET_LINE_CODING) {
            if (begin_control_output(USB_CONTROL_STAGE_UPDATER_LINE_CODING_OUT,
                                     USB_UPDATER_LINE_CODING_SIZE)) {
                return;
            }
            stall_control();
            return;
        }
        if (control_request.kind == USB_CONTROL_CDC_SET_CONTROL_LINE_STATE) {
            usb_updater_control_set_lines(&updater_control, (uint8_t)control_request.value);
            if (arm_control_status_in()) {
                control_stage = USB_CONTROL_STAGE_STATUS_IN;
            } else {
                stall_control();
            }
            return;
        }
    }
    bool endpoint_halted = control_request.recipient == USB_RECIPIENT_ENDPOINT &&
                           platform_usb_endpoint_halted((uint8_t)control_request.index);
    device_control.remote_wakeup_forced =
        operating_mode == USB_OPERATING_MODE_XBOX_GIP && xbox_service_identity.wheel_mode == 6;
    control_transfer = usb_device_control_handle(&device_control, &control_request,
                                                 &descriptor_catalog, endpoint_halted);
    if (control_transfer.kind == USB_CONTROL_TRANSFER_ACKNOWLEDGE &&
        control_request.kind == USB_CONTROL_SET_CONFIGURATION) {
        apply_configuration();
    }
    if (control_transfer.kind == USB_CONTROL_TRANSFER_ACKNOWLEDGE &&
        control_request.recipient == USB_RECIPIENT_ENDPOINT &&
        (control_request.kind == USB_CONTROL_SET_FEATURE ||
         control_request.kind == USB_CONTROL_CLEAR_FEATURE)) {
        bool halted = control_request.kind == USB_CONTROL_SET_FEATURE;
        platform_usb_set_endpoint_halt((uint8_t)control_request.index, halted);
        if (!halted) {
            restart_endpoint_after_halt((uint8_t)control_request.index);
        }
    }
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
 * @brief Applies the active USB configuration to endpoints.
 *
 * Resets endpoint ping-pong state, configures the endpoint set for any nonzero selection, and
 * removes the active mode's endpoints when configuration zero is selected.
 */
static void apply_configuration(void) {
    platform_usb_reset_endpoint_state();
    if (usb_device_control_configured(&device_control)) {
        if (operating_mode == USB_OPERATING_MODE_UPDATER) {
            configure_updater_endpoints();
        } else {
            configure_application_endpoints();
        }
    }
}

/**
 * @brief Applies a completed USB address change.
 *
 * Commits the pending address after the endpoint-zero status stage and updates the controller.
 */
static void complete_control_change(void) {
    UsbDevicePendingChange pending_change = device_control.pending_change;
    usb_device_control_complete(&device_control);
    if (pending_change == USB_DEVICE_PENDING_ADDRESS) {
        platform_usb_set_address(device_control.address);
    }
}

/**
 * @brief Advances a completed endpoint-zero input transaction.
 *
 * Sends the next data packet, enters the prearmed zero-length output status stage after the last
 * packet, or commits an acknowledged state change and rearms setup reception.
 */
static void handle_control_input_complete(void) {
    if (control_stage == USB_CONTROL_STAGE_DATA_IN) {
        if (send_next_control_packet()) {
            return;
        }
        control_stage = USB_CONTROL_STAGE_STATUS_OUT;
        return;
    }
    if (control_stage == USB_CONTROL_STAGE_STATUS_IN) {
        complete_control_change();
        control_status_in_armed = false;
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
    if (control_stage == USB_CONTROL_STAGE_DATA_OUT ||
        control_stage == USB_CONTROL_STAGE_PLAYSTATION_AUTHENTICATION_OUT ||
        control_stage == USB_CONTROL_STAGE_UPDATER_LINE_CODING_OUT ||
        control_stage == USB_CONTROL_STAGE_UPDATER_ENCAPSULATED_OUT) {
        uint8_t remaining = control_output_expected - control_output_received;
        if (usb_event.length > remaining) {
            stall_control();
            return;
        }
        for (uint8_t index = 0; index < usb_event.length; index++) {
            control_output[control_output_received + index] = usb_event.data[index];
        }
        control_output_received += usb_event.length;
        if (control_output_received < control_output_expected) {
            control_output_data_one = !control_output_data_one;
            remaining = control_output_expected - control_output_received;
            if (!platform_usb_receive(USB_CONTROL_ENDPOINT, remaining, control_output_data_one)) {
                stall_control();
            }
            return;
        }
    }
    if (control_stage == USB_CONTROL_STAGE_UPDATER_LINE_CODING_OUT) {
        if (!usb_updater_line_coding_decode(&updater_control, control_output,
                                            control_output_received)) {
            stall_control();
            return;
        }
        if (arm_control_status_in()) {
            control_stage = USB_CONTROL_STAGE_STATUS_IN;
        } else {
            stall_control();
        }
        return;
    }
    if (control_stage == USB_CONTROL_STAGE_UPDATER_ENCAPSULATED_OUT) {
        if (arm_control_status_in()) {
            control_stage = USB_CONTROL_STAGE_STATUS_IN;
        } else {
            stall_control();
        }
        return;
    }
    if (control_stage == USB_CONTROL_STAGE_PLAYSTATION_AUTHENTICATION_OUT) {
        if (control_output_received != USB_PLAYSTATION_AUTHENTICATION_REPORT_SIZE) {
            stall_control();
            return;
        }
        (void)usb_playstation_authentication_receive(&console_workspace.playstation.authentication,
                                                     control_output);
        if (arm_control_status_in()) {
            control_stage = USB_CONTROL_STAGE_STATUS_IN;
        } else {
            stall_control();
        }
        return;
    }
    if (control_stage == USB_CONTROL_STAGE_DATA_OUT) {
        store_output_report(control_report_type, control_report_id, control_output,
                            control_output_received);
        if (arm_control_status_in()) {
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
 * Appends the received endpoint 3 payload to the two-packet updater queue when space is available,
 * then rearms the completed output bank for another 64-byte transfer.
 */
static void handle_updater_output(void) {
    if (updater_packet_count < 2) {
        uint8_t slot = (updater_packet_head + updater_packet_count) & 1u;
        updater_packets[slot].length = usb_event.length;
        for (uint8_t index = 0; index < usb_event.length; index++) {
            updater_packets[slot].data[index] = usb_event.data[index];
        }
        updater_packet_count++;
    }
    if (platform_usb_receive(USB_UPDATER_DATA_ENDPOINT, USB_DEVICE_REPORT_SIZE,
                             updater_output_data_one)) {
        updater_output_data_one = !updater_output_data_one;
    }
}

/**
 * @brief Handles a USB bus reset.
 *
 * Clears transfer and endpoint service state while retaining HID selections, then rearms
 * endpoint-zero setup reception.
 */
static void handle_reset(void) {
    reset_state(true);
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
            updater_input_length = 0;
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
 * packets for the main device service, and passes requests and idle polls through the discovery
 * and session service. A received request can be processed while a prior input transfer is still
 * completing.
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
    updater_input_length = length;
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

bool usb_device_publish_feature_report(uint8_t report_id, const uint8_t *report, uint8_t length) {
    if (report == 0 || length == 0 || length > USB_DEVICE_REPORT_SIZE) {
        return false;
    }
    for (uint8_t index = 0; index < 5; index++) {
        if (feature_report_ids[index] != report_id) {
            continue;
        }
        for (uint8_t offset = 0; offset < length; offset++) {
            feature_reports[index][offset] = report[offset];
        }
        feature_report_lengths[index] = length;
        return true;
    }
    return false;
}

bool usb_device_take_feature_report_request(uint8_t report_id) {
    for (uint8_t index = 0; index < 5; index++) {
        uint8_t mask = (uint8_t)(1u << index);
        if (feature_report_ids[index] == report_id && (feature_report_requests & mask) != 0) {
            feature_report_requests &= (uint8_t)~mask;
            return true;
        }
    }
    return false;
}

bool usb_device_enqueue_remote_hid_record(const UsbRemoteHidQueueRecord *record) {
    if (operating_mode != USB_OPERATING_MODE_FANATEC &&
        operating_mode != USB_OPERATING_MODE_XBOX_GIP &&
        operating_mode != USB_OPERATING_MODE_PLAYSTATION) {
        return false;
    }
    return usb_remote_hid_queue_enqueue(&remote_hid_queue, record);
}

bool usb_device_remote_hid_pending(void) { return usb_remote_hid_queue_pending(&remote_hid_queue); }

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

bool usb_device_send_vendor_report(const uint8_t *report, uint8_t length) {
    if (operating_mode != USB_OPERATING_MODE_FANATEC || !usb_device_configured() || report == 0 ||
        length == 0 || length > USB_DEVICE_REPORT_SIZE ||
        !platform_usb_send(USB_PRIMARY_ENDPOINT, report, length, input_data_one)) {
        return false;
    }
    input_data_one = !input_data_one;
    return true;
}

bool usb_device_take_updater_packet(UsbDeviceUpdaterPacket *packet) {
    if (updater_packet_count == 0 || packet == 0) {
        return false;
    }
    *packet = updater_packets[updater_packet_head];
    updater_packet_head = (updater_packet_head + 1) & 1u;
    updater_packet_count--;
    return true;
}

bool usb_device_updater_channel_idle(void) {
    return !updater_response_ready && !updater_input_busy;
}

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

UsbXboxGipSessionAction usb_device_take_xbox_session_actions(void) {
    UsbXboxGipSessionAction actions = xbox_session_actions;
    xbox_session_actions = USB_XBOX_GIP_SESSION_ACTION_NONE;
    return actions;
}

bool usb_device_take_playstation_authentication_request(
    uint8_t request[USB_PLAYSTATION_AUTHENTICATION_REQUEST_SIZE]) {
    return operating_mode == USB_OPERATING_MODE_PLAYSTATION &&
           usb_playstation_authentication_take_request(
               &console_workspace.playstation.authentication, request);
}

bool usb_device_publish_playstation_authentication_response(const uint8_t *response,
                                                            uint16_t response_length) {
    return operating_mode == USB_OPERATING_MODE_PLAYSTATION &&
           usb_playstation_authentication_publish_response(
               &console_workspace.playstation.authentication, response, response_length);
}

bool usb_device_playstation_authentication_response_active(void) {
    return operating_mode == USB_OPERATING_MODE_PLAYSTATION &&
           usb_playstation_authentication_response_active(
               &console_workspace.playstation.authentication);
}

void usb_device_fail_playstation_authentication(void) {
    if (operating_mode == USB_OPERATING_MODE_PLAYSTATION) {
        usb_playstation_authentication_fail(&console_workspace.playstation.authentication);
    }
}

bool usb_device_publish_playstation_remote_tuning_report(
    const uint8_t report[USB_DEVICE_REPORT_SIZE]) {
    if (operating_mode != USB_OPERATING_MODE_PLAYSTATION || report == NULL || report[0] != 0x35 ||
        playstation_remote_tuning_ready) {
        return false;
    }
    for (uint8_t index = 0; index < USB_DEVICE_REPORT_SIZE; index++) {
        playstation_remote_tuning_report[index] = report[index];
    }
    playstation_remote_tuning_ready = true;
    return true;
}

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
