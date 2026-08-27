#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "platform/usb.h"
#include "usb/device.h"

enum { EVENT_CAPACITY = 16 };

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
static TransferRecord received[4];
static uint8_t receive_count;
static uint8_t address;
static uint8_t control_ready_count;
static bool attached;
static bool hid_configured;
static bool stalled;

static void reset_platform(void) {
    memset(events, 0, sizeof(events));
    event_head = 0;
    event_tail = 0;
    memset(&sent, 0, sizeof(sent));
    memset(received, 0, sizeof(received));
    receive_count = 0;
    address = 0;
    control_ready_count = 0;
    attached = false;
    hid_configured = false;
    stalled = false;
}

static void push_event(PlatformUsbEventType type, uint8_t endpoint, const uint8_t *data,
                       uint8_t length) {
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

bool platform_usb_take_event(PlatformUsbEvent *event) {
    if (event_tail == event_head) {
        return false;
    }
    *event = events[event_tail++];
    return true;
}

bool platform_usb_send(uint8_t endpoint, const uint8_t *data, uint8_t length, bool data_one) {
    sent.endpoint = endpoint;
    sent.length = length;
    sent.data_one = data_one;
    if (length != 0) {
        memcpy(sent.data, data, length);
    }
    return true;
}

bool platform_usb_receive(uint8_t endpoint, uint8_t length, bool data_one) {
    TransferRecord *record = &received[receive_count++];
    record->endpoint = endpoint;
    record->length = length;
    record->data_one = data_one;
    return true;
}

void platform_usb_control_ready(void) { control_ready_count++; }
void platform_usb_set_address(uint8_t value) { address = value; }
void platform_usb_configure_hid_endpoint(void) { hid_configured = true; }
void platform_usb_unconfigure_hid_endpoint(void) { hid_configured = false; }
void platform_usb_stall(uint8_t endpoint) { stalled = endpoint == 0; }

static void complete_control_input(void) {
    push_event(PLATFORM_USB_EVENT_IN_COMPLETE, 0, 0, 0);
    usb_device_service();
    assert(received[receive_count - 1].endpoint == 0);
    assert(received[receive_count - 1].length == 0);
    assert(received[receive_count - 1].data_one);

    push_event(PLATFORM_USB_EVENT_OUT, 0, 0, 0);
    usb_device_service();
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
    assert(!usb_device_configured());
    push_event(PLATFORM_USB_EVENT_IN_COMPLETE, 0, 0, 0);
    usb_device_service();
    assert(usb_device_configured());
    assert(hid_configured);
    assert(receive_count == 2);
    assert(received[0].endpoint == 1 && received[0].length == 64 && !received[0].data_one);
    assert(received[1].endpoint == 1 && received[1].length == 64 && received[1].data_one);
}

static void test_exchanges_hid_reports(void) {
    static const uint8_t set_configuration[] = {0x00, 9, 1, 0, 0, 0, 0, 0};
    static const uint8_t input[] = {1, 2, 3, 4};
    static const uint8_t output[] = {2, 9, 8, 7};

    usb_device_init(BOARD_VARIANT_DD2);
    push_setup(set_configuration);
    usb_device_service();
    push_event(PLATFORM_USB_EVENT_IN_COMPLETE, 0, 0, 0);
    usb_device_service();

    assert(usb_device_send_input(input, sizeof(input)));
    assert(sent.endpoint == 1 && sent.length == sizeof(input) && !sent.data_one);
    assert(memcmp(sent.data, input, sizeof(input)) == 0);

    push_event(PLATFORM_USB_EVENT_OUT, 1, output, sizeof(output));
    usb_device_service();
    UsbDeviceOutputReport report;
    assert(usb_device_take_output(&report));
    assert(report.report_type == 2 && report.report_id == 2);
    assert(report.length == sizeof(output));
    assert(memcmp(report.data, output, sizeof(output)) == 0);
    assert(!usb_device_take_output(&report));
}

int main(void) {
    test_enumerates_podium_device();
    test_exchanges_hid_reports();
    return 0;
}
