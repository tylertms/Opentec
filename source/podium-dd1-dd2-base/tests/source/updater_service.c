#include "usb/updater_service.h"

#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "motor/probe.h"
#include "platform/aux_bus.h"
#include "system/runtime_bridge.h"
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
static PlatformAuxBusStatus aux_bus_status;
static uint8_t aux_address;
static uint16_t aux_register;
static uint8_t aux_data[WHEEL_UPDATER_BRIDGE_MAX_REQUEST_SIZE];
static uint8_t *aux_read_destination;
static uint16_t aux_length;

bool platform_aux_bus_start_write(uint8_t address, uint16_t register_address, const uint8_t *data,
                                  uint16_t length) {
    aux_address = address;
    aux_register = register_address;
    memcpy(aux_data, data, length);
    aux_length = length;
    aux_bus_status = PLATFORM_AUX_BUS_BUSY;
    return true;
}

bool platform_aux_bus_start_read(uint8_t address, uint16_t register_address, uint8_t *data,
                                 uint16_t length) {
    aux_address = address;
    aux_register = register_address;
    aux_read_destination = data;
    aux_length = length;
    aux_bus_status = PLATFORM_AUX_BUS_BUSY;
    return true;
}

PlatformAuxBusStatus platform_aux_bus_status(void) { return aux_bus_status; }

void platform_aux_bus_clear(void) { aux_bus_status = PLATFORM_AUX_BUS_IDLE; }

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

void platform_serial_link_direct_clear(void) {
    direct_response = NULL;
    direct_response_length = 0;
}

static void reset_fakes(void) {
    host_packet = (UsbDeviceUpdaterPacket){0};
    host_packet_ready = false;
    updater_channel_idle = true;
    queued_response_length = 0;
    transmitted_length = 0;
    direct_response = NULL;
    direct_response_length = 0;
    aux_bus_status = PLATFORM_AUX_BUS_IDLE;
    aux_address = 0;
    aux_register = 0;
    aux_read_destination = NULL;
    aux_length = 0;
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
    assert(usb_updater_service_select_mode(&service, USB_RUNTIME_MODE_AUXILIARY));
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

static void test_routes_auxiliary_handshake_and_probe(void) {
    UsbUpdaterService service;
    reset_fakes();
    usb_updater_service_init(&service, NULL);
    assert(usb_updater_service_select_mode(&service, USB_RUNTIME_MODE_AUXILIARY));

    usb_updater_service_request_auxiliary_handshake(&service);
    assert(!usb_updater_service_auxiliary_handshake_complete(&service));
    UsbUpdaterServiceInput input = input_at(0);
    usb_updater_service_run(&service, &input);
    const uint8_t expected_handshake[] = {0xfa, 0x05};
    assert(aux_address == 0x78);
    assert(aux_register == 3);
    assert(aux_length == sizeof(expected_handshake));
    assert(memcmp(aux_data, expected_handshake, sizeof(expected_handshake)) == 0);

    aux_bus_status = PLATFORM_AUX_BUS_SUCCEEDED;
    input.now_ms = 1;
    usb_updater_service_run(&service, &input);
    assert(usb_updater_service_auxiliary_handshake_complete(&service));
    assert(usb_updater_service_start_probe(&service));
    input.now_ms = 2;
    usb_updater_service_run(&service, &input);
    const uint8_t expected_probe[] = {0x5a, 0xa6};
    assert(aux_address == 0x10);
    assert(aux_register == 0);
    assert(aux_length == sizeof(expected_probe));
    assert(memcmp(aux_data, expected_probe, sizeof(expected_probe)) == 0);
}

static void test_routes_startup_recovery_without_handshake(void) {
    UsbUpdaterService service;
    reset_fakes();
    usb_updater_service_init(&service, NULL);
    assert(usb_updater_service_select_startup_recovery(&service));
    assert(usb_updater_service_auxiliary_handshake_complete(&service));
    assert(usb_updater_service_start_probe(&service));

    UsbUpdaterServiceInput input = input_at(0);
    usb_updater_service_run(&service, &input);
    const uint8_t expected_probe[] = {0x5a, 0xa6};
    assert(aux_address == 0x10);
    assert(aux_register == 0);
    assert(aux_length == sizeof(expected_probe));
    assert(memcmp(aux_data, expected_probe, sizeof(expected_probe)) == 0);
}

static void test_fails_startup_recovery_probe_after_bus_error(void) {
    UsbUpdaterService service;
    reset_fakes();
    usb_updater_service_init(&service, NULL);
    assert(usb_updater_service_select_startup_recovery(&service));
    assert(usb_updater_service_start_probe(&service));

    UsbUpdaterServiceInput input = input_at(0);
    usb_updater_service_run(&service, &input);
    aux_bus_status = PLATFORM_AUX_BUS_FAILED;
    input.now_ms = 1;
    usb_updater_service_run(&service, &input);

    assert(usb_updater_service_probe_status(&service) == USB_UPDATER_PROBE_FAILED);
}

static void test_restores_normal_usb_when_motor_and_updater_are_absent(void) {
    MotorProbe motor_probe;
    UsbUpdaterService updater;
    RuntimeBridge runtime_bridge;
    reset_fakes();

    motor_probe_init(&motor_probe);
    motor_probe_start(&motor_probe, 0);
    motor_probe_run(&motor_probe, 0);
    aux_bus_status = PLATFORM_AUX_BUS_FAILED;
    motor_probe_run(&motor_probe, 1000);
    assert(motor_probe.phase == MOTOR_PROBE_FAILED);

    usb_updater_service_init(&updater, NULL);
    assert(usb_updater_service_select_startup_recovery(&updater));
    runtime_bridge_init(&runtime_bridge);
    assert(runtime_bridge_start_auxiliary_recovery(&runtime_bridge, 0) ==
           (RUNTIME_BRIDGE_ACTION_PREPARE_USB | RUNTIME_BRIDGE_ACTION_ENABLE_TRANSFER_TIMER));

    RuntimeBridgeInput bridge_input = {.now_ms = 11};
    assert(runtime_bridge_step(&runtime_bridge, &bridge_input) ==
           RUNTIME_BRIDGE_ACTION_START_TRANSFER);
    assert(usb_updater_service_start_probe(&updater));

    UsbUpdaterServiceInput updater_input = input_at(11);
    usb_updater_service_run(&updater, &updater_input);
    aux_bus_status = PLATFORM_AUX_BUS_FAILED;
    updater_input.now_ms = 12;
    usb_updater_service_run(&updater, &updater_input);
    assert(usb_updater_service_probe_status(&updater) == USB_UPDATER_PROBE_FAILED);

    bridge_input.now_ms = 12;
    bridge_input.transfer_status = RUNTIME_BRIDGE_TRANSFER_FAILED;
    assert(
        runtime_bridge_step(&runtime_bridge, &bridge_input) ==
        (RUNTIME_BRIDGE_ACTION_DISABLE_TRANSFER_TIMER | RUNTIME_BRIDGE_ACTION_RESTORE_NORMAL_USB));
    assert(runtime_bridge.phase == RUNTIME_BRIDGE_IDLE);
    assert(runtime_bridge.mode == USB_RUNTIME_MODE_NORMAL);
}

static void advance_startup_probe_to_header_read(UsbUpdaterService *service) {
    UsbUpdaterServiceInput input = input_at(0);
    usb_updater_service_run(service, &input);
    aux_bus_status = PLATFORM_AUX_BUS_SUCCEEDED;
    input.now_ms = 1;
    usb_updater_service_run(service, &input);
    input.now_ms = 2;
    usb_updater_service_run(service, &input);
    input.now_ms = 3;
    usb_updater_service_run(service, &input);
    assert(aux_bus_status == PLATFORM_AUX_BUS_BUSY);
    assert(aux_read_destination != NULL);
    assert(aux_length == 1);
}

static void test_fails_startup_recovery_probe_after_read_error(void) {
    UsbUpdaterService service;
    reset_fakes();
    usb_updater_service_init(&service, NULL);
    assert(usb_updater_service_select_startup_recovery(&service));
    assert(usb_updater_service_start_probe(&service));
    advance_startup_probe_to_header_read(&service);

    aux_bus_status = PLATFORM_AUX_BUS_FAILED;
    UsbUpdaterServiceInput input = input_at(4);
    usb_updater_service_run(&service, &input);
    assert(usb_updater_service_probe_status(&service) == USB_UPDATER_PROBE_FAILED);
}

static void test_fails_startup_recovery_probe_on_zero_header(void) {
    UsbUpdaterService service;
    reset_fakes();
    usb_updater_service_init(&service, NULL);
    assert(usb_updater_service_select_startup_recovery(&service));
    assert(usb_updater_service_start_probe(&service));
    advance_startup_probe_to_header_read(&service);

    *aux_read_destination = 0;
    aux_bus_status = PLATFORM_AUX_BUS_SUCCEEDED;
    UsbUpdaterServiceInput input = input_at(4);
    usb_updater_service_run(&service, &input);
    assert(usb_updater_service_probe_status(&service) == USB_UPDATER_PROBE_FAILED);
}

static void test_fails_startup_recovery_probe_on_non_a7_response(void) {
    UsbUpdaterService service;
    reset_fakes();
    usb_updater_service_init(&service, NULL);
    assert(usb_updater_service_select_startup_recovery(&service));
    assert(usb_updater_service_start_probe(&service));
    advance_startup_probe_to_header_read(&service);

    *aux_read_destination = 0x5a;
    aux_bus_status = PLATFORM_AUX_BUS_SUCCEEDED;
    UsbUpdaterServiceInput input = input_at(4);
    usb_updater_service_run(&service, &input);
    assert(aux_read_destination != NULL);
    assert(aux_length == 1);
    *aux_read_destination = 0xa2;
    aux_bus_status = PLATFORM_AUX_BUS_SUCCEEDED;
    input.now_ms = 5;
    usb_updater_service_run(&service, &input);
    assert(usb_updater_service_probe_status(&service) == USB_UPDATER_PROBE_FAILED);
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
    static const uint8_t response[] = {0x5a, 0xa7, 1, 2, 3, 4, 0x8a, 5, 6, 7};
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
    assert(memcmp(transmitted, "\x5a\xa6", 2) == 0);
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
    assert(request[2] == 0 && request[3] == 0x5a && request[4] == 0xa6);
}

static void test_routes_probe_to_protocol_recovery_target(void) {
    UsbUpdaterService service;
    CommandTransport transport;
    reset_fakes();
    command_transport_init(&transport);
    usb_updater_service_init(&service, &transport);
    assert(usb_updater_service_select_mode(&service, USB_RUNTIME_MODE_PROTOCOL_RECOVERY));
    assert(usb_updater_service_start_probe(&service));

    UsbUpdaterServiceInput input = input_at(0);
    usb_updater_service_run(&service, &input);
    const uint8_t *request;
    uint16_t length;
    assert(command_transport_request(&transport, &request, &length));
    assert(length == 5);
    assert(request[0] == 2);
    assert(request[1] == (WHEEL_UPDATER_TARGET_USB << 1));
    assert(request[2] == 0 && request[3] == 0x5a && request[4] == 0xa6);
}

int main(void) {
    test_selects_supported_routes();
    test_services_device_information_on_strict_cadence();
    test_routes_auxiliary_handshake_and_probe();
    test_routes_startup_recovery_without_handshake();
    test_fails_startup_recovery_probe_after_bus_error();
    test_restores_normal_usb_when_motor_and_updater_are_absent();
    test_fails_startup_recovery_probe_after_read_error();
    test_fails_startup_recovery_probe_on_zero_header();
    test_fails_startup_recovery_probe_on_non_a7_response();
    test_latches_guarded_reset();
    test_probes_direct_route_and_selects_identity();
    test_forwards_host_bridge_response();
    test_routes_probe_to_protocol_target();
    test_routes_probe_to_protocol_recovery_target();
    return 0;
}
