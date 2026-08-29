#include "usb/updater_service.h"

#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "transfer/command.h"
#include "usb/device.h"
#include "usb/operating_mode_command.h"
#include "usb/updater_protocol.h"

static UsbDeviceUpdaterPacket host_packet;
static bool host_packet_ready;
static bool updater_channel_idle;
static uint8_t queued_response[USB_DEVICE_UPDATER_RESPONSE_SIZE];
static uint8_t queued_response_length;
static uint8_t transmitted[WHEEL_UPDATER_BRIDGE_MAX_REQUEST_SIZE];
static uint8_t transmitted_length;
static const uint8_t *direct_response;
static uint8_t direct_response_length;

bool usb_device_take_updater_packet(UsbDeviceUpdaterPacket *packet) {
    if (!host_packet_ready || packet == NULL) {
        return false;
    }
    *packet = host_packet;
    host_packet_ready = false;
    return true;
}

bool usb_device_updater_channel_idle(void) { return updater_channel_idle; }

bool usb_device_queue_updater_response(const uint8_t *data, uint8_t length) {
    if (!updater_channel_idle || data == NULL || length == 0 || length > sizeof(queued_response)) {
        return false;
    }
    memcpy(queued_response, data, length);
    queued_response_length = length;
    updater_channel_idle = false;
    return true;
}

void platform_serial_link_enter_direct_mode(void) {}

bool platform_serial_link_direct_write(const uint8_t *data, uint8_t length) {
    memcpy(transmitted, data, length);
    transmitted_length = length;
    return true;
}

bool platform_serial_link_direct_read(uint8_t *data, uint8_t length) {
    if (direct_response_length < length) {
        return false;
    }
    memcpy(data, direct_response, length);
    direct_response += length;
    direct_response_length -= length;
    return true;
}

static void reset_fakes(void) {
    host_packet = (UsbDeviceUpdaterPacket){0};
    host_packet_ready = false;
    updater_channel_idle = true;
    queued_response_length = 0;
    transmitted_length = 0;
    direct_response = NULL;
    direct_response_length = 0;
}

static void queue_host_packet(const uint8_t *data, uint8_t length) {
    assert(length <= sizeof(host_packet.data));
    memcpy(host_packet.data, data, length);
    host_packet.length = length;
    host_packet_ready = true;
}

static UsbUpdaterServiceInput input_at(uint32_t now_ms) {
    return (UsbUpdaterServiceInput){
        .now_ms = now_ms,
        .board_variant = BOARD_VARIANT_DD1,
    };
}

static void test_selects_supported_routes(void) {
    UsbUpdaterService service;
    CommandTransport transport;
    command_transport_init(&transport);
    usb_updater_service_init(&service, &transport);

    assert(!usb_updater_service_select_mode(NULL, USB_RUNTIME_MODE_STATUS_BRIDGE));
    assert(!usb_updater_service_select_mode(&service, USB_RUNTIME_MODE_NORMAL));
    assert(!usb_updater_service_select_mode(&service, USB_RUNTIME_MODE_RESET));
    for (UsbRuntimeMode mode = USB_RUNTIME_MODE_AUXILIARY;
         mode <= USB_RUNTIME_MODE_PROTOCOL_RECOVERY; mode++) {
        assert(usb_updater_service_select_mode(&service, mode));
    }

    usb_updater_service_init(&service, NULL);
    assert(usb_updater_service_select_mode(&service, USB_RUNTIME_MODE_STATUS_BRIDGE));
    assert(!usb_updater_service_select_mode(&service, USB_RUNTIME_MODE_USB_BRIDGE));
}

static void test_services_device_information_on_strict_cadence(void) {
    static const uint8_t request[] = {0xf8, 0x01};
    UsbUpdaterService service;
    reset_fakes();
    usb_updater_service_init(&service, NULL);
    assert(usb_updater_service_select_mode(&service, USB_RUNTIME_MODE_STATUS_BRIDGE));
    usb_updater_service_set_usb_active(&service, true);
    queue_host_packet(request, sizeof(request));

    UsbUpdaterServiceInput input = input_at(0);
    usb_updater_service_run(&service, &input);
    assert(host_packet_ready);
    input.now_ms = 1;
    usb_updater_service_run(&service, &input);
    assert(!host_packet_ready);
    assert(queued_response_length == 0);
    input.now_ms = 11;
    usb_updater_service_run(&service, &input);
    assert(queued_response_length == 0);
    input.now_ms = 12;
    usb_updater_service_run(&service, &input);
    static const uint8_t expected[] = {0xf8, 0x01, 'p', 'd', 'q', 'r'};
    assert(queued_response_length == sizeof(expected));
    assert(memcmp(queued_response, expected, sizeof(expected)) == 0);
}

static void test_latches_guarded_reset(void) {
    static const uint8_t request[] = {0xf8, 0x09, 0x01, 0xfe};
    UsbUpdaterService service;
    reset_fakes();
    usb_updater_service_init(&service, NULL);
    assert(usb_updater_service_select_mode(&service, USB_RUNTIME_MODE_STATUS_BRIDGE));
    usb_updater_service_set_usb_active(&service, true);
    queue_host_packet(request, sizeof(request));

    UsbUpdaterServiceInput input = input_at(1);
    usb_updater_service_run(&service, &input);
    assert(usb_updater_service_take_reset(&service));
    assert(!usb_updater_service_take_reset(&service));
    assert(!usb_updater_service_take_reset(NULL));
}

static void complete_direct_exchange(UsbUpdaterService *service, const uint8_t *response,
                                     uint8_t length, uint32_t now_ms) {
    direct_response = response;
    direct_response_length = length;
    UsbUpdaterServiceInput input = input_at(now_ms);
    for (uint8_t iteration = 0; iteration < 16; iteration++) {
        usb_updater_service_run(service, &input);
        input.now_ms += 2;
    }
}

static void test_probes_direct_route_and_selects_identity(void) {
    static const uint8_t response[] = {0x5a, 0xa7, 0x8a, 1, 2, 3, 4, 5, 6, 7};
    static const uint8_t identity_request[] = {0xf8, 0x01};
    UsbUpdaterService service;
    reset_fakes();
    usb_updater_service_init(&service, NULL);
    assert(usb_updater_service_select_mode(&service, USB_RUNTIME_MODE_STATUS_BRIDGE));
    assert(usb_updater_service_probe_status(&service) == USB_UPDATER_PROBE_IDLE);
    assert(usb_updater_service_start_probe(&service));
    assert(!usb_updater_service_start_probe(&service));
    complete_direct_exchange(&service, response, sizeof(response), 100);
    assert(transmitted_length == 2);
    assert(memcmp(transmitted, "\x5a\xa7", 2) == 0);
    assert(usb_updater_service_probe_status(&service) == USB_UPDATER_PROBE_COMPLETE);

    assert(usb_updater_service_select_mode(&service, USB_RUNTIME_MODE_USB_BRIDGE) == false);
    assert(usb_updater_service_select_mode(&service, USB_RUNTIME_MODE_STATUS_BRIDGE));
    usb_updater_service_set_usb_active(&service, true);
    queue_host_packet(identity_request, sizeof(identity_request));
    UsbUpdaterServiceInput input = input_at(103);
    usb_updater_service_run(&service, &input);
    input.now_ms = 114;
    usb_updater_service_run(&service, &input);
    assert(queued_response_length == USB_UPDATER_DEVICE_INFO_RESPONSE_SIZE);
}

static void test_forwards_host_bridge_response(void) {
    static const uint8_t request[] = {0x5a, 0xb0};
    static const uint8_t response[] = {0x5a, 0xa7, 0x14, 1, 2, 3, 4, 5, 6, 7};
    UsbUpdaterService service;
    reset_fakes();
    usb_updater_service_init(&service, NULL);
    assert(usb_updater_service_select_mode(&service, USB_RUNTIME_MODE_STATUS_BRIDGE));
    usb_updater_service_set_usb_active(&service, true);
    queue_host_packet(request, sizeof(request));

    UsbUpdaterServiceInput input = input_at(1);
    usb_updater_service_run(&service, &input);
    assert(!host_packet_ready);
    complete_direct_exchange(&service, response, sizeof(response), 2);
    assert(queued_response_length == sizeof(response));
    assert(memcmp(queued_response, response, sizeof(response)) == 0);
}

static void test_routes_probe_to_protocol_target(void) {
    UsbUpdaterService service;
    CommandTransport transport;
    reset_fakes();
    command_transport_init(&transport);
    usb_updater_service_init(&service, &transport);
    assert(usb_updater_service_select_mode(&service, USB_RUNTIME_MODE_PROTOCOL_BRIDGE));
    assert(usb_updater_service_start_probe(&service));

    UsbUpdaterServiceInput input = input_at(0);
    usb_updater_service_run(&service, &input);
    const uint8_t *request;
    uint16_t length;
    assert(command_transport_request(&transport, &request, &length));
    assert(length == 5);
    assert(request[0] == 2);
    assert(request[1] == (WHEEL_UPDATER_TARGET_PROTOCOL << 1));
    assert(request[2] == 0 && request[3] == 0x5a && request[4] == 0xa7);
}

int main(void) {
    test_selects_supported_routes();
    test_services_device_information_on_strict_cadence();
    test_latches_guarded_reset();
    test_probes_direct_route_and_selects_identity();
    test_forwards_host_bridge_response();
    test_routes_probe_to_protocol_target();
    return 0;
}
