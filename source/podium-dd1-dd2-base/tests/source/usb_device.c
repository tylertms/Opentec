#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "platform/usb.h"
#include "usb/device.h"

enum { EVENT_CAPACITY = 32 };

typedef struct {
    uint8_t endpoint;
    uint8_t length;
    bool data_one;
    uint8_t data[PLATFORM_USB_PACKET_SIZE];
} TransferRecord;

static PlatformUsbEvent events[EVENT_CAPACITY];
static uint8_t event_head;
static uint8_t event_tail;
static TransferRecord sent;
static TransferRecord received[EVENT_CAPACITY];
static uint8_t receive_count;
static uint8_t send_count;
static uint8_t address;
static uint8_t control_ready_count;
static bool attached;
static bool hid_configured;
static bool endpoint_input[5];
static bool endpoint_output[5];
static bool endpoint_halted[5][2];
static bool stalled;
static uint8_t restart_count;
static uint32_t now_ms;

static void reset_platform(void) {
    memset(events, 0, sizeof(events));
    event_head = 0;
    event_tail = 0;
    memset(&sent, 0, sizeof(sent));
    memset(received, 0, sizeof(received));
    receive_count = 0;
    send_count = 0;
    address = 0;
    control_ready_count = 0;
    attached = false;
    hid_configured = false;
    memset(endpoint_input, 0, sizeof(endpoint_input));
    memset(endpoint_output, 0, sizeof(endpoint_output));
    memset(endpoint_halted, 0, sizeof(endpoint_halted));
    stalled = false;
    restart_count = 0;
    now_ms = 0;
}

static void push_event(PlatformUsbEventType type, uint8_t endpoint, const uint8_t *data,
                       uint8_t length) {
    if (event_head == event_tail) {
        event_head = 0;
        event_tail = 0;
    }
    assert(event_head < EVENT_CAPACITY);
    assert(length <= PLATFORM_USB_PACKET_SIZE);
    PlatformUsbEvent *event = &events[event_head++];
    event->type = type;
    event->endpoint = endpoint;
    event->length = length;
    if (length != 0) {
        memcpy(event->data, data, length);
    }
}

static void push_setup(const uint8_t data[8]) { push_event(PLATFORM_USB_EVENT_SETUP, 0, data, 8); }

void platform_usb_init(void) { reset_platform(); }
void platform_usb_attach(void) { attached = true; }
void platform_usb_detach(void) { attached = false; }
void platform_usb_restart(void) {
    restart_count++;
    event_head = 0;
    event_tail = 0;
    receive_count = 0;
    hid_configured = false;
    memset(endpoint_input, 0, sizeof(endpoint_input));
    memset(endpoint_output, 0, sizeof(endpoint_output));
    attached = true;
}

bool platform_usb_take_event(PlatformUsbEvent *event) {
    if (event_tail == event_head) {
        return false;
    }
    *event = events[event_tail++];
    return true;
}

bool platform_usb_send(uint8_t endpoint, const uint8_t *data, uint8_t length, bool data_one) {
    if (endpoint_halted[endpoint][1]) {
        return false;
    }
    send_count++;
    sent.endpoint = endpoint;
    sent.length = length;
    sent.data_one = data_one;
    if (length != 0) {
        memcpy(sent.data, data, length);
    }
    return true;
}

bool platform_usb_receive(uint8_t endpoint, uint8_t length, bool data_one) {
    if (endpoint_halted[endpoint][0]) {
        return false;
    }
    assert(receive_count < EVENT_CAPACITY);
    TransferRecord *record = &received[receive_count++];
    record->endpoint = endpoint;
    record->length = length;
    record->data_one = data_one;
    return true;
}

void platform_usb_control_ready(void) { control_ready_count++; }
void platform_usb_set_address(uint8_t value) { address = value; }
void platform_usb_configure_endpoint(uint8_t endpoint, bool input, bool output) {
    endpoint_input[endpoint] = input;
    endpoint_output[endpoint] = output;
    hid_configured = endpoint == 1 && input && output;
}
void platform_usb_unconfigure_endpoint(uint8_t endpoint) {
    endpoint_input[endpoint] = false;
    endpoint_output[endpoint] = false;
    if (endpoint == 1) {
        hid_configured = false;
    }
}
void platform_usb_stall(uint8_t endpoint) { stalled = endpoint == 0; }
bool platform_usb_endpoint_halted(uint8_t endpoint_address) {
    return endpoint_halted[endpoint_address & 0x0f][(endpoint_address & 0x80) != 0];
}
void platform_usb_set_endpoint_halt(uint8_t endpoint_address, bool halted) {
    endpoint_halted[endpoint_address & 0x0f][(endpoint_address & 0x80) != 0] = halted;
}
uint32_t platform_time_ms(void) { return now_ms; }

static void complete_control_input(void) {
    push_event(PLATFORM_USB_EVENT_IN_COMPLETE, 0, 0, 0);
    usb_device_service();
    assert(received[receive_count - 1].endpoint == 0);
    assert(received[receive_count - 1].length == 0);
    assert(received[receive_count - 1].data_one);

    push_event(PLATFORM_USB_EVENT_OUT, 0, 0, 0);
    usb_device_service();
}

static void complete_control_output(void) {
    push_event(PLATFORM_USB_EVENT_IN_COMPLETE, 0, 0, 0);
    usb_device_service();
}

static void assert_string_descriptor(uint8_t index, const char *expected) {
    uint8_t request[] = {0x80, 6, index, 3, 0x09, 0x04, 0xff, 0};
    uint8_t descriptor[256] = {0};
    size_t text_length = strlen(expected);
    size_t descriptor_length = text_length * 2 + 2;
    descriptor[0] = (uint8_t)descriptor_length;
    descriptor[1] = 3;
    for (size_t character = 0; character < text_length; character++) {
        descriptor[character * 2 + 2] = (uint8_t)expected[character];
    }

    push_setup(request);
    usb_device_service();
    size_t offset = 0;
    while (offset < descriptor_length) {
        size_t packet_length = descriptor_length - offset;
        if (packet_length > PLATFORM_USB_PACKET_SIZE) {
            packet_length = PLATFORM_USB_PACKET_SIZE;
        }
        assert(sent.length == packet_length);
        assert(memcmp(sent.data, descriptor + offset, packet_length) == 0);
        offset += packet_length;
        if (offset < descriptor_length) {
            push_event(PLATFORM_USB_EVENT_IN_COMPLETE, 0, 0, 0);
            usb_device_service();
        }
    }
    complete_control_input();
}

static void test_prepares_without_attaching(void) {
    usb_device_prepare(BOARD_VARIANT_DD1);
    assert(!attached);
    assert(control_ready_count == 1);
    assert(usb_device_operating_mode() == USB_OPERATING_MODE_FANATEC);
}

static void test_prepares_updater_without_restarting_or_attaching(void) {
    assert(usb_device_prepare_updater(BOARD_VARIANT_DD1));
    assert(!attached);
    assert(restart_count == 0);
    assert(control_ready_count == 1);
    assert(usb_device_operating_mode() == USB_OPERATING_MODE_UPDATER);
}

static void test_enumerates_podium_device(void) {
    static const uint8_t get_device_descriptor[] = {0x80, 6, 0, 1, 0, 0, 18, 0};
    static const uint8_t set_address[] = {0x00, 5, 42, 0, 0, 0, 0, 0};
    static const uint8_t set_configuration[] = {0x00, 9, 1, 0, 0, 0, 0, 0};

    usb_device_init(BOARD_VARIANT_DD1);
    assert(attached);
    assert(control_ready_count == 1);

    push_setup(get_device_descriptor);
    usb_device_service();
    assert(!stalled);
    assert(sent.endpoint == 0 && sent.length == 18 && sent.data_one);
    assert(sent.data[1] == 1);
    assert(sent.data[8] == 0xb7 && sent.data[9] == 0x0e);
    complete_control_input();

    push_setup(set_address);
    usb_device_service();
    assert(sent.length == 0 && sent.data_one);
    assert(address == 0);
    push_event(PLATFORM_USB_EVENT_IN_COMPLETE, 0, 0, 0);
    usb_device_service();
    assert(address == 42);

    receive_count = 0;
    push_setup(set_configuration);
    usb_device_service();
    assert(usb_device_configured());
    assert(hid_configured);
    assert(receive_count == 2);
    assert(received[0].endpoint == 1 && received[0].length == 64 && !received[0].data_one);
    assert(received[1].endpoint == 1 && received[1].length == 64 && received[1].data_one);
    push_event(PLATFORM_USB_EVENT_IN_COMPLETE, 0, 0, 0);
    usb_device_service();

    uint8_t input[USB_DEVICE_REPORT_SIZE + 1] = {1};
    UsbDeviceOutputReport output;
    assert(!usb_device_take_output(NULL));
    assert(!usb_device_take_output(&output));
    assert(!usb_device_send_input(NULL, 1));
    assert(!usb_device_send_input(input, 0));
    assert(!usb_device_send_input(input, sizeof(input)));
    assert(usb_device_send_input(input, USB_DEVICE_REPORT_SIZE));
    assert(usb_device_send_input(input, USB_DEVICE_REPORT_SIZE));
    assert(!usb_device_send_vendor_report(NULL, 1));
    assert(!usb_device_send_vendor_report(input, 0));
    assert(!usb_device_send_vendor_report(input, sizeof(input)));
    endpoint_halted[1][1] = true;
    input[1] = 1;
    assert(!usb_device_send_input(input, USB_DEVICE_REPORT_SIZE));
    assert(!usb_device_send_vendor_report(input, USB_DEVICE_REPORT_SIZE));
    endpoint_halted[1][1] = false;
}

static void test_returns_xbox_security_descriptor(void) {
    static const uint8_t request[] = {0xc0, 0x90, 0, 0, 4, 0, 40, 0};
    usb_device_init(BOARD_VARIANT_DD1);
    push_setup(request);
    usb_device_service();
    assert(!stalled);
    assert(sent.endpoint == 0 && sent.length == 40 && sent.data_one);
    assert(sent.data[0] == 40);
    assert(memcmp(&sent.data[18], "XGIP10", 6) == 0);
    complete_control_input();
}

static void test_exchanges_hid_reports(void) {
    static const uint8_t set_configuration[] = {0x00, 9, 1, 0, 0, 0, 0, 0};
    static const uint8_t input[] = {1, 2, 3, 4};
    static const uint8_t changed_input[] = {1, 2, 3, 5};
    static const uint8_t output[] = {2, 9, 8, 7};

    usb_device_init(BOARD_VARIANT_DD2);
    push_setup(set_configuration);
    usb_device_service();
    push_event(PLATFORM_USB_EVENT_IN_COMPLETE, 0, 0, 0);
    usb_device_service();

    send_count = 0;
    assert(usb_device_send_input(input, sizeof(input)));
    assert(sent.endpoint == 1 && sent.length == sizeof(input) && !sent.data_one);
    assert(memcmp(sent.data, input, sizeof(input)) == 0);
    assert(send_count == 1);
    assert(usb_device_send_input(input, sizeof(input)));
    assert(send_count == 1);
    assert(usb_device_send_input(changed_input, sizeof(changed_input)));
    assert(send_count == 2 && sent.data_one);

    uint8_t vendor_report[USB_DEVICE_REPORT_SIZE] = {6, 0x30, 1, 9};
    assert(usb_device_send_vendor_report(vendor_report, USB_DEVICE_REPORT_SIZE));
    assert(send_count == 3 && sent.endpoint == 1 && sent.length == USB_DEVICE_REPORT_SIZE);
    assert(!sent.data_one && memcmp(sent.data, vendor_report, sizeof(vendor_report)) == 0);
    assert(usb_device_send_vendor_report(vendor_report, 22));
    assert(send_count == 4 && sent.data_one);
    assert(sent.length == 22);
    assert(!usb_device_send_vendor_report(vendor_report, 0));
    assert(!usb_device_send_vendor_report(vendor_report, USB_DEVICE_REPORT_SIZE + 1));

    push_event(PLATFORM_USB_EVENT_OUT, 1, output, sizeof(output));
    usb_device_service();
    UsbDeviceOutputReport report;
    assert(usb_device_take_output(&report));
    assert(report.report_type == 2 && report.report_id == 2);
    assert(report.length == sizeof(output));
    assert(memcmp(report.data, output, sizeof(output)) == 0);
    assert(!usb_device_take_output(&report));
}

static void test_retains_hid_state_across_bus_reset(void) {
    static const uint8_t set_idle[] = {0x21, 0x0a, 0x00, 0x07, 0x00, 0x00, 0x00, 0x00};
    static const uint8_t set_protocol[] = {0x21, 0x0b, 0x09, 0x00, 0x00, 0x00, 0x00, 0x00};
    static const uint8_t get_idle[] = {0xa1, 0x02, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00};
    static const uint8_t get_protocol[] = {0xa1, 0x03, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00};

    usb_device_init(BOARD_VARIANT_DD1);

    push_setup(set_idle);
    usb_device_service();
    push_event(PLATFORM_USB_EVENT_IN_COMPLETE, 0, 0, 0);
    usb_device_service();

    push_setup(set_protocol);
    usb_device_service();
    push_event(PLATFORM_USB_EVENT_IN_COMPLETE, 0, 0, 0);
    usb_device_service();

    push_event(PLATFORM_USB_EVENT_RESET, 0, 0, 0);
    usb_device_service();

    push_setup(get_idle);
    usb_device_service();
    assert(sent.length == 1 && sent.data[0] == 7);
    complete_control_input();

    push_setup(get_protocol);
    usb_device_service();
    assert(sent.length == 1 && sent.data[0] == 9);
    complete_control_input();
}

static void test_serves_hidden_feature_reports(void) {
    static const uint8_t get_feature_report[] = {0xa1, 1, 0x31, 3, 0, 0, 64, 0};
    uint8_t report[USB_DEVICE_REPORT_SIZE] = {0x31, 1, 0xa5, 0x5a};

    usb_device_init(BOARD_VARIANT_DD1);
    assert(!usb_device_publish_feature_report(0x30, report, sizeof(report)));
    assert(!usb_device_publish_feature_report(0x31, NULL, sizeof(report)));
    assert(usb_device_publish_feature_report(0x31, report, sizeof(report)));
    assert(!usb_device_take_feature_report_request(0x31));

    push_setup(get_feature_report);
    usb_device_service();
    assert(sent.length == USB_DEVICE_REPORT_SIZE);
    assert(memcmp(sent.data, report, sizeof(report)) == 0);
    assert(usb_device_take_feature_report_request(0x31));
    assert(!usb_device_take_feature_report_request(0x31));
    complete_control_input();
}

static void test_controls_endpoint_halt(void) {
    static const uint8_t set_configuration[] = {0x00, 9, 1, 0, 0, 0, 0, 0};
    static const uint8_t set_input_halt[] = {0x02, 3, 0, 0, 0x81, 0, 0, 0};
    static const uint8_t get_input_status[] = {0x82, 0, 0, 0, 0x81, 0, 2, 0};
    static const uint8_t clear_input_halt[] = {0x02, 1, 0, 0, 0x81, 0, 0, 0};
    static const uint8_t set_output_halt[] = {0x02, 3, 0, 0, 0x01, 0, 0, 0};
    static const uint8_t clear_output_halt[] = {0x02, 1, 0, 0, 0x01, 0, 0, 0};
    static const uint8_t report[] = {1, 2, 3};

    usb_device_init(BOARD_VARIANT_DD1);
    push_setup(set_configuration);
    usb_device_service();
    complete_control_output();

    push_setup(set_input_halt);
    usb_device_service();
    assert(endpoint_halted[1][1]);
    complete_control_output();
    assert(!usb_device_send_input(report, sizeof(report)));

    push_setup(get_input_status);
    usb_device_service();
    assert(sent.length == 2 && sent.data[0] == 1 && sent.data[1] == 0);
    complete_control_input();

    push_setup(clear_input_halt);
    usb_device_service();
    assert(!endpoint_halted[1][1]);
    complete_control_output();
    assert(usb_device_send_input(report, sizeof(report)));
    assert(!sent.data_one);

    push_setup(set_output_halt);
    usb_device_service();
    assert(endpoint_halted[1][0]);
    complete_control_output();

    receive_count = 0;
    push_setup(clear_output_halt);
    usb_device_service();
    assert(!endpoint_halted[1][0]);
    assert(receive_count == 2);
    assert(!received[0].data_one && received[1].data_one);
    complete_control_output();
}

static void test_reenumerates_compatibility_modes(void) {
    static const uint8_t get_device_descriptor[] = {0x80, 6, 0, 1, 0, 0, 18, 0};
    static const uint8_t get_configuration_descriptor[] = {0x80, 6, 0, 2, 0, 0, 41, 0};
    static const uint8_t set_configuration[] = {0x00, 9, 1, 0, 0, 0, 0, 0};
    static const uint8_t unnumbered_output[] = {0xf8, 9, 1, 1, 0, 0, 0};
    static const uint16_t vendor_ids[] = {0x0eb7, 0x046d, 0x046d, 0x046d};
    static const uint16_t product_ids[] = {0x0e03, 0xc294, 0xc298, 0xc29b};
    static const uint16_t report_sizes[] = {133, 130, 97, 133};
    static const uint8_t product_indices[] = {8, 2, 2, 2};
    static const char *products[] = {
        "FANATEC CSL Elite Wheel Base",
        "G27 Racing Wheel",
        "G27 Racing Wheel",
        "G27 Racing Wheel",
    };

    usb_device_init(BOARD_VARIANT_DD1);
    for (uint8_t index = 0; index < 4; index++) {
        UsbInputReportMode mode = (UsbInputReportMode)(index + 1);
        assert(usb_device_set_input_mode(mode));
        assert(attached && restart_count == index + 1);
        assert(usb_device_input_mode() == mode);

        push_setup(get_device_descriptor);
        usb_device_service();
        uint16_t vendor_id = (uint16_t)sent.data[8] | (uint16_t)sent.data[9] << 8;
        uint16_t product_id = (uint16_t)sent.data[10] | (uint16_t)sent.data[11] << 8;
        assert(vendor_id == vendor_ids[index]);
        assert(product_id == product_ids[index]);
        complete_control_input();

        push_setup(get_configuration_descriptor);
        usb_device_service();
        uint16_t report_size = (uint16_t)sent.data[25] | (uint16_t)sent.data[26] << 8;
        assert(report_size == report_sizes[index]);
        if (mode == USB_INPUT_REPORT_MODE_DRIVING_FORCE_PRO) {
            assert(sent.data[8] == 0x28);
        }
        if (mode == USB_INPUT_REPORT_MODE_DRIVING_FORCE_PRO || mode == USB_INPUT_REPORT_MODE_G27) {
            assert(sent.data[16] == 0 && sent.data[17] == 0xfe);
        }
        complete_control_input();

        assert_string_descriptor(product_indices[index], products[index]);
        if (mode == USB_INPUT_REPORT_MODE_FANATEC_COMPATIBILITY) {
            assert_string_descriptor(9, products[index]);
        }
        if (mode == USB_INPUT_REPORT_MODE_DRIVING_FORCE_PRO || mode == USB_INPUT_REPORT_MODE_G27) {
            assert_string_descriptor(0xfe, products[index]);
        }
    }
    assert(!usb_device_set_input_mode((UsbInputReportMode)5));
    assert(usb_device_input_mode() == USB_INPUT_REPORT_MODE_G27);

    receive_count = 0;
    push_setup(set_configuration);
    usb_device_service();
    push_event(PLATFORM_USB_EVENT_IN_COMPLETE, 0, 0, 0);
    usb_device_service();
    push_event(PLATFORM_USB_EVENT_OUT, 1, unnumbered_output, sizeof(unnumbered_output));
    usb_device_service();
    UsbDeviceOutputReport report;
    assert(usb_device_take_output(&report));
    assert(report.report_id == 0 && report.length == sizeof(unnumbered_output));
    assert(memcmp(report.data, unnumbered_output, sizeof(unnumbered_output)) == 0);
}

static void test_exchanges_updater_packets(void) {
    static const uint8_t get_device_descriptor[] = {0x80, 6, 0, 1, 0, 0, 18, 0};
    static const uint8_t get_configuration_descriptor[] = {0x80, 6, 0, 2, 0, 0, 64, 0};
    static const uint8_t get_line_coding[] = {0xa1, 0x21, 0, 0, 0, 0, 7, 0};
    static const uint8_t set_line_coding[] = {0x21, 0x20, 0, 0, 0, 0, 7, 0};
    static const uint8_t set_configuration[] = {0x00, 9, 1, 0, 0, 0, 0, 0};
    static const uint8_t set_input_halt[] = {0x02, 3, 0, 0, 0x83, 0, 0, 0};
    static const uint8_t clear_input_halt[] = {0x02, 1, 0, 0, 0x83, 0, 0, 0};
    static const uint8_t updated_line_coding[] = {0x00, 0xc2, 0x01, 0x00, 0, 0, 8};
    static const uint8_t bulk_output[] = {0x12, 0x34, 0x56};
    static const uint8_t bulk_input[] = {0x78, 0x9a};

    usb_device_init(BOARD_VARIANT_DD1);
    assert(usb_device_set_operating_mode(USB_OPERATING_MODE_UPDATER));
    assert(usb_device_operating_mode() == USB_OPERATING_MODE_UPDATER);
    assert(!usb_device_send_input(bulk_input, sizeof(bulk_input)));

    push_setup(get_device_descriptor);
    usb_device_service();
    assert(sent.data[4] == 0x02);
    assert(sent.data[10] == 0x18 && sent.data[11] == 0x07);
    assert(sent.data[15] == 7);
    complete_control_input();

    push_setup(get_configuration_descriptor);
    usb_device_service();
    assert(sent.length == 64);
    assert(sent.data[0] == 9 && sent.data[1] == 2 && sent.data[2] == 67);
    assert(sent.data[4] == 2);
    complete_control_input();
    assert_string_descriptor(7, "FANATEC EBLDC Updater");

    push_setup(get_line_coding);
    usb_device_service();
    const uint8_t default_line_coding[] = {0x00, 0x4b, 0x00, 0x00, 0, 0, 8};
    assert(sent.length == sizeof(default_line_coding));
    assert(memcmp(sent.data, default_line_coding, sizeof(default_line_coding)) == 0);
    complete_control_input();

    receive_count = 0;
    push_setup(set_line_coding);
    usb_device_service();
    assert(receive_count == 1 && received[0].endpoint == 0 && received[0].length == 7);
    push_event(PLATFORM_USB_EVENT_OUT, 0, updated_line_coding, sizeof(updated_line_coding));
    usb_device_service();
    assert(sent.endpoint == 0 && sent.length == 0);

    receive_count = 0;
    push_setup(set_configuration);
    usb_device_service();
    push_event(PLATFORM_USB_EVENT_IN_COMPLETE, 0, 0, 0);
    usb_device_service();
    assert(endpoint_input[2] && !endpoint_output[2]);
    assert(endpoint_input[3] && endpoint_output[3]);
    assert(receive_count == 2);
    assert(received[0].endpoint == 3 && received[0].length == 64);
    assert(received[1].endpoint == 3 && received[1].length == 64);
    assert(usb_device_updater_channel_idle());
    assert(!usb_device_take_updater_packet(NULL));
    UsbDeviceUpdaterPacket empty_packet;
    assert(!usb_device_take_updater_packet(&empty_packet));
    assert(!usb_device_queue_updater_response(NULL, 1));
    assert(!usb_device_queue_updater_response(bulk_input, 0));
    uint8_t oversized_response[USB_DEVICE_UPDATER_RESPONSE_SIZE + 1] = {0};
    assert(!usb_device_queue_updater_response(oversized_response, sizeof(oversized_response)));

    push_event(PLATFORM_USB_EVENT_OUT, 3, bulk_output, sizeof(bulk_output));
    static const uint8_t second_output[] = {0x78, 0x9a};
    push_event(PLATFORM_USB_EVENT_OUT, 3, second_output, sizeof(second_output));
    usb_device_service();
    UsbDeviceUpdaterPacket packet;
    assert(usb_device_take_updater_packet(&packet));
    assert(packet.length == sizeof(bulk_output));
    assert(memcmp(packet.data, bulk_output, sizeof(bulk_output)) == 0);
    assert(usb_device_take_updater_packet(&packet));
    assert(packet.length == sizeof(second_output));
    assert(memcmp(packet.data, second_output, sizeof(second_output)) == 0);
    assert(!usb_device_take_updater_packet(&packet));

    assert(usb_device_queue_updater_response(bulk_input, sizeof(bulk_input)));
    assert(!usb_device_updater_channel_idle());
    assert(sent.endpoint == 3 && sent.length == sizeof(bulk_input));
    assert(memcmp(sent.data, bulk_input, sizeof(bulk_input)) == 0);

    push_event(PLATFORM_USB_EVENT_IN_COMPLETE, 3, NULL, 0);
    usb_device_service();
    assert(usb_device_updater_channel_idle());

    uint8_t split_response[USB_DEVICE_UPDATER_RESPONSE_SIZE];
    for (uint8_t index = 0; index < sizeof(split_response); index++) {
        split_response[index] = index;
    }
    assert(usb_device_queue_updater_response(split_response, sizeof(split_response)));
    assert(sent.endpoint == 3 && sent.length == USB_DEVICE_REPORT_SIZE);
    assert(memcmp(sent.data, split_response, USB_DEVICE_REPORT_SIZE) == 0);
    assert(!usb_device_queue_updater_response(bulk_input, sizeof(bulk_input)));

    push_setup(set_input_halt);
    usb_device_service();
    assert(endpoint_halted[3][1]);
    complete_control_output();
    push_setup(clear_input_halt);
    usb_device_service();
    assert(!endpoint_halted[3][1]);
    assert(sent.endpoint == 3 && sent.length == USB_DEVICE_REPORT_SIZE);
    assert(memcmp(sent.data, split_response, USB_DEVICE_REPORT_SIZE) == 0);
    complete_control_output();

    push_event(PLATFORM_USB_EVENT_IN_COMPLETE, 3, NULL, 0);
    usb_device_service();
    assert(sent.endpoint == 3 && sent.length == 2);
    assert(memcmp(sent.data, split_response + USB_DEVICE_REPORT_SIZE, 2) == 0);

    push_event(PLATFORM_USB_EVENT_IN_COMPLETE, 3, NULL, 0);
    usb_device_service();
    assert(usb_device_queue_updater_response(split_response, USB_DEVICE_REPORT_SIZE));
    assert(sent.length == USB_DEVICE_REPORT_SIZE);
    push_event(PLATFORM_USB_EVENT_IN_COMPLETE, 3, NULL, 0);
    usb_device_service();
    assert(sent.endpoint == 3 && sent.length == 0);
    assert(!usb_device_queue_updater_response(bulk_input, sizeof(bulk_input)));
    push_event(PLATFORM_USB_EVENT_IN_COMPLETE, 3, NULL, 0);
    usb_device_service();
    assert(usb_device_queue_updater_response(bulk_input, sizeof(bulk_input)));
    assert(!usb_device_updater_channel_idle());
}

static void test_exchanges_xbox_gip_discovery(void) {
    static const uint8_t digest[USB_XBOX_GIP_DIGEST_SIZE] = {
        0x12, 0x34, 0x56, 0x78, 0x9a, 0xbc, 0xde, 0xf0,
    };
    static const uint8_t get_device_descriptor[] = {0x80, 6, 0, 1, 0, 0, 18, 0};
    static const uint8_t get_configuration_descriptor[] = {0x80, 6, 0, 2, 0, 0, 64, 0};
    static const uint8_t set_configuration[] = {0x00, 9, 1, 0, 0, 0, 0, 0};
    static const uint8_t activate[] = {5, 0, 0, 0, 0};

    usb_device_init(BOARD_VARIANT_DD1);
    assert(!usb_device_set_operating_mode((UsbOperatingMode)8));
    assert(!usb_device_set_operating_mode(USB_OPERATING_MODE_XBOX_GIP));
    assert(!usb_device_set_xbox_mode(6, NULL));
    assert(!usb_device_set_xbox_mode(0, digest));
    assert(usb_device_set_xbox_mode(6, digest));
    assert(usb_device_operating_mode() == USB_OPERATING_MODE_XBOX_GIP);

    push_setup(get_device_descriptor);
    usb_device_service();
    assert(sent.data[4] == 0xff && sent.data[5] == 0xff && sent.data[6] == 0xff);
    assert(sent.data[10] == 0x50 && sent.data[11] == 0x0f);
    assert(sent.data[16] == 3);
    complete_control_input();

    push_setup(get_configuration_descriptor);
    usb_device_service();
    assert(sent.length == USB_XBOX_GIP_CONFIGURATION_DESCRIPTOR_SIZE);
    assert(sent.data[20] == 0x01 && sent.data[27] == 0x81);
    complete_control_input();
    assert_string_descriptor(2, "FANATEC Podium Wheel Base DD1");
    assert_string_descriptor(3, "F0DEBC9A78563412");

    receive_count = 0;
    push_setup(set_configuration);
    usb_device_service();
    push_event(PLATFORM_USB_EVENT_IN_COMPLETE, 0, 0, 0);
    now_ms = 100;
    usb_device_service();
    assert(endpoint_input[1] && endpoint_output[1]);
    assert(receive_count == 2);
    assert(sent.endpoint == 1 && sent.length == 32);
    assert(sent.data[0] == 2 && sent.data[2] == 1);
    assert(memcmp(&sent.data[4], digest, sizeof(digest)) == 0);

    push_event(PLATFORM_USB_EVENT_IN_COMPLETE, 1, 0, 0);
    push_event(PLATFORM_USB_EVENT_OUT, 1, activate, sizeof(activate));
    usb_device_service();
    assert(sent.endpoint == 1 && sent.length == 8);
    assert(sent.data[0] == 3 && sent.data[2] == 2);
    assert(usb_device_take_xbox_session_actions() ==
           (USB_XBOX_GIP_SESSION_ACTION_SEND_READY | USB_XBOX_GIP_SESSION_ACTION_REFRESH_STATE));
    assert(usb_device_take_xbox_session_actions() == USB_XBOX_GIP_SESSION_ACTION_NONE);

    uint8_t invalid_response[USB_DEVICE_REPORT_SIZE + 1] = {0};
    UsbDeviceOutputReport empty_output;
    assert(!usb_device_take_output(NULL));
    assert(!usb_device_take_output(&empty_output));
    assert(!usb_device_queue_xbox_input(NULL));
    assert(!usb_device_queue_xbox_extended_status(NULL));
    assert(!usb_device_queue_xbox_transfer_status(NULL));
    assert(!usb_device_queue_xbox_response(NULL, 3));
    assert(!usb_device_queue_xbox_response(invalid_response, 2));
    assert(!usb_device_queue_xbox_response(invalid_response, sizeof(invalid_response)));
    assert(!usb_device_queue_xbox_vendor_report(NULL));

    UsbXboxGipInputSnapshot snapshot = {
        .buttons = {0x12, 0x34},
        .steering = 0x5678,
        .pedals = {0x1234, 0x2345, 0x3456},
        .auxiliary_pedal = 0x45,
        .steering_range_degrees = 1080,
    };
    assert(usb_device_queue_xbox_input(&snapshot));
    assert(!usb_device_queue_xbox_input(&snapshot));
    push_event(PLATFORM_USB_EVENT_IN_COMPLETE, 1, 0, 0);
    usb_device_service();
    assert(sent.endpoint == 1 && sent.length == USB_XBOX_GIP_INPUT_RESPONSE_SIZE);
    assert(sent.data[0] == 0x20 && sent.data[2] == 3 && sent.data[3] == 0x32);
    assert(sent.data[4] == 0x12 && sent.data[5] == 0x34);
    assert(sent.data[6] == 0x78 && sent.data[7] == 0x56);
    assert(sent.data[17] == 0x30 && sent.data[18] == 0x2a);

    uint8_t application_response[22] = {0x25, 0, 0xa5, 0x12, 6};
    assert(usb_device_queue_xbox_response(application_response, sizeof(application_response)));
    assert(application_response[2] == 0xa5);
    assert(!usb_device_queue_xbox_response(application_response, sizeof(application_response)));
    push_event(PLATFORM_USB_EVENT_IN_COMPLETE, 1, 0, 0);
    usb_device_service();
    assert(sent.endpoint == 1 && sent.length == sizeof(application_response));
    assert(sent.data[0] == 0x25 && sent.data[2] == 4 && sent.data[3] == 0x12);

    assert(usb_device_queue_xbox_capabilities());
    push_event(PLATFORM_USB_EVENT_IN_COMPLETE, 1, 0, 0);
    usb_device_service();
    assert(sent.endpoint == 1 && sent.length == USB_XBOX_GIP_CAPABILITY_RESPONSE_SIZE);
    assert(sent.data[0] == 0x21 && sent.data[2] == 5 && sent.data[3] == 0x33);
    assert(sent.data[4] == 0x10 && sent.data[9] == 0x5a && sent.data[14] == 0x48);

    UsbXboxGipExtendedStatus extended_status = {
        .board_variant = BOARD_VARIANT_DD1,
        .wheel_mode = 0x1d,
        .multi_position_supported = true,
    };
    assert(usb_device_queue_xbox_extended_status(&extended_status));
    push_event(PLATFORM_USB_EVENT_IN_COMPLETE, 1, 0, 0);
    usb_device_service();
    assert(sent.endpoint == 1 && sent.length == USB_XBOX_GIP_EXTENDED_STATUS_RESPONSE_SIZE);
    assert(sent.data[0] == 0x11 && sent.data[2] == 6 && sent.data[3] == 0x0d);
    assert(sent.data[5] == 0x1d && sent.data[12] == 1 && sent.data[14] == 6);

    const uint8_t control_request[] = {0x0a, 0};
    assert(usb_device_queue_xbox_transfer_status(control_request));
    push_event(PLATFORM_USB_EVENT_IN_COMPLETE, 1, 0, 0);
    usb_device_service();
    assert(sent.endpoint == 1 && sent.length == USB_XBOX_GIP_TRANSFER_STATUS_RESPONSE_SIZE);
    assert(sent.data[0] == 1 && sent.data[2] == 7 && sent.data[3] == 9);
    assert(sent.data[4] == 2 && sent.data[5] == 0x0a && sent.data[6] == 0);

    uint8_t vendor_report[USB_DEVICE_REPORT_SIZE] = {0x36, 5, 1, 0xa5};
    assert(usb_device_queue_xbox_vendor_report(vendor_report));
    push_event(PLATFORM_USB_EVENT_IN_COMPLETE, 1, 0, 0);
    usb_device_service();
    assert(sent.endpoint == 1 && sent.length == USB_DEVICE_REPORT_SIZE);
    assert(memcmp(sent.data, vendor_report, sizeof(vendor_report)) == 0);

    uint8_t force_feedback_input[USB_DEVICE_REPORT_SIZE] = {0};
    force_feedback_input[0] = 0x0e;
    force_feedback_input[4] = 1;
    force_feedback_input[5] = 0x34;
    force_feedback_input[6] = 0x12;
    force_feedback_input[7] = 3;
    push_event(PLATFORM_USB_EVENT_OUT, 1, force_feedback_input, sizeof(force_feedback_input));
    usb_device_service();

    UsbDeviceOutputReport output;
    assert(usb_device_take_output(&output));
    assert(output.report_type == USB_DEVICE_HID_REPORT_OUTPUT);
    assert(output.report_id == 0 && output.length == USB_DEVICE_REPORT_SIZE);
    assert(memcmp(output.data, force_feedback_input, sizeof(force_feedback_input)) == 0);
    assert(!usb_device_take_output(&output));

    uint8_t vendor_tunnel[USB_DEVICE_REPORT_SIZE] = {[0] = 0x0f, [3] = 60, [4] = 0x36, [5] = 5};
    push_event(PLATFORM_USB_EVENT_OUT, 1, vendor_tunnel, sizeof(vendor_tunnel));
    usb_device_service();
    assert(usb_device_take_output(&output));
    assert(memcmp(output.data, vendor_tunnel, sizeof(vendor_tunnel)) == 0);
}

static void test_exchanges_playstation_authentication(void) {
    static const uint8_t get_device_descriptor[] = {0x80, 6, 0, 1, 0, 0, 18, 0};
    static const uint8_t get_configuration_descriptor[] = {0x80, 6, 0, 2, 0, 0, 41, 0};
    static const uint8_t set_configuration[] = {0x00, 9, 1, 0, 0, 0, 0, 0};
    static const uint8_t get_format_report[] = {0xa1, 1, 0xf3, 3, 0, 0, 8, 0};
    static const uint8_t get_feature_report[] = {0xa1, 1, 3, 3, 0, 0, 48, 0};
    static const uint8_t get_remote_tuning_report[] = {0xa1, 1, 0x35, 3, 0, 0, 64, 0};
    static const uint8_t get_status_report[] = {0xa1, 1, 0xf2, 3, 0, 0, 16, 0};
    static const uint8_t get_response_report[] = {0xa1, 1, 0xf1, 3, 0, 0, 64, 0};
    static const uint8_t set_request_report[] = {0x21, 9, 0xf0, 3, 0, 0, 64, 0};
    uint8_t expected_request[USB_PLAYSTATION_AUTHENTICATION_REQUEST_SIZE];
    uint8_t actual_request[USB_PLAYSTATION_AUTHENTICATION_REQUEST_SIZE];
    uint8_t response[USB_PLAYSTATION_AUTHENTICATION_RESPONSE_SIZE];

    for (uint16_t index = 0; index < sizeof(expected_request); index++) {
        expected_request[index] = (uint8_t)(index ^ 0x5a);
    }
    for (uint16_t index = 0; index < sizeof(response); index++) {
        response[index] = (uint8_t)(index ^ 0xa5);
    }

    usb_device_init(BOARD_VARIANT_DD2);
    assert(!usb_device_set_playstation_wheel_mode(3));
    assert(usb_device_set_playstation_wheel_mode(5));
    assert(usb_device_operating_mode() == USB_OPERATING_MODE_PLAYSTATION);

    push_setup(get_device_descriptor);
    usb_device_service();
    assert(sent.data[8] == 0xb7 && sent.data[9] == 0x0e);
    assert(sent.data[10] == 0x04 && sent.data[11] == 0x0e);
    assert(sent.data[15] == 9);
    complete_control_input();

    uint8_t remote_tuning_report[USB_DEVICE_REPORT_SIZE] = {0x35, 5, 1, 0xa5};
    assert(usb_device_publish_playstation_remote_tuning_report(remote_tuning_report));
    assert(!usb_device_publish_playstation_remote_tuning_report(remote_tuning_report));
    push_setup(get_remote_tuning_report);
    usb_device_service();
    assert(sent.length == USB_DEVICE_REPORT_SIZE);
    assert(memcmp(sent.data, remote_tuning_report, sizeof(remote_tuning_report)) == 0);
    complete_control_input();
    assert(usb_device_publish_playstation_remote_tuning_report(remote_tuning_report));

    push_setup(get_configuration_descriptor);
    usb_device_service();
    assert(sent.length == 41);
    assert(sent.data[25] == 160 && sent.data[26] == 0);
    assert(sent.data[29] == 0x03 && sent.data[36] == 0x84);
    complete_control_input();
    assert_string_descriptor(9, "FANATEC Podium Wheel Base DD2 PlayStation 4");

    receive_count = 0;
    push_setup(set_configuration);
    usb_device_service();
    push_event(PLATFORM_USB_EVENT_IN_COMPLETE, 0, 0, 0);
    usb_device_service();
    assert(usb_device_configured());
    assert(endpoint_input[3] && endpoint_output[3]);
    assert(endpoint_input[4] && endpoint_output[4]);
    assert(!endpoint_input[1] && !endpoint_output[1]);
    assert(receive_count == 2);
    assert(received[0].endpoint == 3 && received[0].length == 64 && !received[0].data_one);
    assert(received[1].endpoint == 3 && received[1].length == 64 && received[1].data_one);

    uint8_t playstation_output[32] = {5, 0x12, 0x34, 0x56};
    push_event(PLATFORM_USB_EVENT_OUT, 3, playstation_output, sizeof(playstation_output));
    usb_device_service();
    UsbDeviceOutputReport output;
    assert(usb_device_take_output(&output));
    assert(output.report_type == USB_DEVICE_HID_REPORT_OUTPUT);
    assert(output.report_id == 5 && output.length == USB_DEVICE_REPORT_SIZE);
    assert(memcmp(output.data, playstation_output, sizeof(playstation_output)) == 0);
    assert(memcmp(output.data + sizeof(playstation_output),
                  (uint8_t[USB_DEVICE_REPORT_SIZE - sizeof(playstation_output)]){0},
                  USB_DEVICE_REPORT_SIZE - sizeof(playstation_output)) == 0);
    assert(received[receive_count - 1].endpoint == 3);

    UsbPlaystationInputState input_state = {
        .clutch_axes = {0x7f, 0x80},
        .hat = 8,
        .buttons = 0x1234,
        .steering = 0x5678,
        .pedals = {0x9abc, 0xdef0, 0x1357},
        .wheel_hat = 0x81,
        .auxiliary_axis = 0x2468,
    };
    uint8_t previous_send_count = send_count;
    assert(usb_device_send_playstation_input(&input_state));
    assert(send_count == previous_send_count + 1);
    assert(sent.endpoint == 4 && sent.length == USB_PLAYSTATION_INPUT_REPORT_SIZE);
    assert(sent.data[0] == 1 && sent.data[3] == 0x7f && sent.data[4] == 0x80);
    assert(memcmp(sent.data + 0x2b, (uint8_t[]){0x78, 0x56, 0xbc, 0x9a, 0xf0, 0xde, 0x57, 0x13},
                  8) == 0);
    assert(usb_device_send_playstation_input(&input_state));
    assert(send_count == previous_send_count + 1);

    push_setup(get_feature_report);
    usb_device_service();
    static const uint8_t expected_feature_prefix[] = {3, 0x21, 0x27, 4, 0x18, 6};
    assert(sent.length == 48);
    assert(memcmp(sent.data, expected_feature_prefix, sizeof(expected_feature_prefix)) == 0);
    assert(sent.data[24] == 0x0f && sent.data[25] == 0xd8 && sent.data[26] == 9);
    complete_control_input();

    push_setup(get_format_report);
    usb_device_service();
    assert(sent.length == 8);
    assert(sent.data[0] == 0xf3 && sent.data[1] == 0);
    assert(sent.data[2] == 0x38 && sent.data[3] == 0x38);
    complete_control_input();

    for (uint8_t fragment = 0; fragment <= 4; fragment++) {
        uint8_t report[USB_PLAYSTATION_AUTHENTICATION_REPORT_SIZE] = {0xf0, 0x27, fragment, 0};
        uint16_t offset = (uint16_t)fragment * 0x38;
        uint16_t remaining = sizeof(expected_request) - offset;
        uint8_t count = remaining < 0x38 ? (uint8_t)remaining : 0x38;
        memcpy(report + 4, expected_request + offset, count);

        receive_count = 0;
        push_setup(set_request_report);
        usb_device_service();
        assert(receive_count == 1 && received[0].endpoint == 0 && received[0].length == 64);
        assert(received[0].data_one);
        push_event(PLATFORM_USB_EVENT_OUT, 0, report, sizeof(report));
        usb_device_service();
        assert(sent.endpoint == 0 && sent.length == 0 && sent.data_one);
        complete_control_output();
    }

    assert(usb_device_take_playstation_authentication_request(actual_request));
    assert(memcmp(actual_request, expected_request, sizeof(expected_request)) == 0);
    assert(!usb_device_take_playstation_authentication_request(actual_request));

    push_setup(get_status_report);
    usb_device_service();
    assert(sent.length == 16 && sent.data[0] == 0xf2 && sent.data[1] == 0x27);
    assert(sent.data[2] == USB_PLAYSTATION_AUTHENTICATION_PENDING);
    complete_control_input();

    assert(usb_device_publish_playstation_authentication_response(response, sizeof(response)));
    assert(usb_device_playstation_authentication_response_active());
    push_setup(get_status_report);
    usb_device_service();
    assert(sent.data[2] == USB_PLAYSTATION_AUTHENTICATION_IDLE);
    complete_control_input();

    for (uint8_t fragment = 0; fragment < 19; fragment++) {
        push_setup(get_response_report);
        usb_device_service();
        assert(sent.length == 64 && sent.data[0] == 0xf1 && sent.data[1] == 0x27);
        assert(sent.data[2] == fragment && sent.data[3] == 0);
        uint16_t offset = (uint16_t)fragment * 0x38;
        uint16_t remaining = sizeof(response) - offset;
        uint8_t count = remaining < 0x38 ? (uint8_t)remaining : 0x38;
        assert(memcmp(sent.data + 4, response + offset, count) == 0);
        for (uint8_t index = (uint8_t)(count + 4); index < sizeof(sent.data); index++) {
            assert(sent.data[index] == 0);
        }
        complete_control_input();
    }
    assert(!usb_device_playstation_authentication_response_active());

    push_setup(get_response_report);
    usb_device_service();
    assert(stalled);
}

static void test_handles_representative_zero_shape_control_requests(void) {
    static const uint8_t request_types[] = {
        0x00, 0x01, 0x02, 0x03, 0x1f, 0x20, 0x21, 0x22, 0x23, 0x3f, 0x40, 0x41, 0x42, 0x43,
        0x5f, 0x60, 0x61, 0x62, 0x63, 0x7f, 0x80, 0x81, 0x82, 0x83, 0x9f, 0xa0, 0xa1, 0xa2,
        0xa3, 0xbf, 0xc0, 0xc1, 0xc2, 0xc3, 0xdf, 0xe0, 0xe1, 0xe2, 0xe3, 0xff,
    };
    static const uint8_t requests[] = {
        0x00, 0x01, 0x02, 0x03, 0x05, 0x06, 0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x20, 0x21, 0x90, 0xff,
    };
    uint8_t setup[8] = {0};
    for (uint8_t request_type_index = 0; request_type_index < sizeof(request_types);
         request_type_index++) {
        for (uint8_t request_index = 0; request_index < sizeof(requests); request_index++) {
            usb_device_init(BOARD_VARIANT_DD1);
            setup[0] = request_types[request_type_index];
            setup[1] = requests[request_index];
            push_setup(setup);
            usb_device_service();
            assert(event_head == event_tail);
        }
    }
}

int main(void) {
    test_prepares_without_attaching();
    test_prepares_updater_without_restarting_or_attaching();
    test_enumerates_podium_device();
    test_returns_xbox_security_descriptor();
    test_exchanges_hid_reports();
    test_retains_hid_state_across_bus_reset();
    test_serves_hidden_feature_reports();
    test_controls_endpoint_halt();
    test_reenumerates_compatibility_modes();
    test_exchanges_updater_packets();
    test_exchanges_xbox_gip_discovery();
    test_exchanges_playstation_authentication();
    test_handles_representative_zero_shape_control_requests();
    return 0;
}
