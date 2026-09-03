#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "motor/command_channel_mailbox.h"
#include "motor/command_packet.h"
#include "usb/feature_upload_acknowledgement.h"
#include "usb/motor_vendor_service.h"

typedef struct {
    UsbMotorVendorService service;
    MotorCommandChannel channel;
    MotorCommandMailboxExchange exchange;
    CommandTransport transport;
    uint8_t upload_assembly[128];
    uint8_t receive_assembly[128];
    uint8_t mailbox_receive[128];
    uint8_t motor_transmit[128];
    uint8_t pending_payload[128];
    uint8_t application_data[128];
    uint8_t usb_packet[USB_FEATURE_UPLOAD_PACKET_SIZE];
} Fixture;

static void fixture_init(Fixture *fixture) {
    memset(fixture, 0, sizeof(*fixture));
    MotorCommandChannelBuffers channel_buffers = {
        .receive_assembly = fixture->receive_assembly,
        .receive_assembly_capacity = sizeof(fixture->receive_assembly),
        .transmit = fixture->motor_transmit,
        .transmit_capacity = sizeof(fixture->motor_transmit),
        .pending_payload = fixture->pending_payload,
        .pending_payload_capacity = sizeof(fixture->pending_payload),
    };
    UsbMotorVendorServiceBuffers buffers = {
        .upload_assembly = fixture->upload_assembly,
        .upload_assembly_capacity = sizeof(fixture->upload_assembly),
        .application_data = fixture->application_data,
        .application_data_capacity = sizeof(fixture->application_data),
    };
    bool channel_initialized = motor_command_channel_init(&fixture->channel, &channel_buffers);
    assert(channel_initialized);
    bool service_initialized =
        usb_motor_vendor_service_init(&fixture->service, &fixture->channel, &buffers);
    assert(service_initialized);
    bool exchange_initialized = motor_command_mailbox_exchange_init(
        &fixture->exchange, fixture->mailbox_receive, sizeof(fixture->mailbox_receive));
    assert(exchange_initialized);
    command_transport_init(&fixture->transport);
}

static void complete_mailbox_read(Fixture *fixture, const uint8_t *data, uint16_t length) {
    uint8_t response[MEMORY_TRANSFER_MAX_READ_SIZE + 2] = {1, 0};
    memcpy(response + 2, data, length);
    assert(command_transport_request_sent(&fixture->transport));
    command_transport_receive(&fixture->transport, response, length + 2);
}

static void complete_mailbox_write(Fixture *fixture) {
    static const uint8_t response[] = {1};
    assert(command_transport_request_sent(&fixture->transport));
    command_transport_receive(&fixture->transport, response, sizeof(response));
}

static void test_bridges_compact_command_and_response(void) {
    Fixture fixture;
    fixture_init(&fixture);
    uint8_t request[USB_FEATURE_UPLOAD_PACKET_SIZE] = {6, 0x30, 0x2a, 12, 0, 0xc1, 0x12, 0x34};

    UsbMotorVendorServiceResult result = usb_motor_vendor_service_accept_usb(
        &fixture.service, request, sizeof(request), fixture.usb_packet);

    assert((result.actions & USB_MOTOR_VENDOR_ACTION_CLAIM) != 0);
    assert((result.actions & USB_MOTOR_VENDOR_ACTION_WRITE_USB) != 0);
    assert((result.actions & USB_MOTOR_VENDOR_ACTION_WRITE_MOTOR) != 0);
    assert(result.usb_packet_length == USB_FEATURE_UPLOAD_ACKNOWLEDGEMENT_SIZE);
    assert(fixture.usb_packet[2] == 0x2a && fixture.usb_packet[6] == 0x30);
    assert(fixture.usb_packet[7] == 12 && fixture.usb_packet[11] == 12);
    assert(result.motor_packet == fixture.motor_transmit && result.motor_packet_length == 9);
    assert(fixture.motor_transmit[0] == 7);
    assert(memcmp(fixture.motor_transmit + 4, request + 5, 3) == 0);
    assert(motor_command_packet_checksum_valid(fixture.motor_transmit, result.motor_packet_length));
    motor_command_channel_mark_written(&fixture.channel, result.motor_packet);

    static const uint8_t motor_payload[] = {0xc1, 0xaa, 0xbb};
    uint8_t motor_response[16];
    uint16_t motor_response_length;
    bool response_encoded = motor_command_packet_payload_encode(
        0, 0, 0, motor_payload, sizeof(motor_payload), motor_response, sizeof(motor_response),
        &motor_response_length);
    assert(response_encoded);
    result = usb_motor_vendor_service_accept_motor(&fixture.service, motor_response,
                                                   motor_response_length);
    assert((result.actions & USB_MOTOR_VENDOR_ACTION_WRITE_MOTOR) != 0);
    assert((result.actions & USB_MOTOR_VENDOR_ACTION_RESPONSE_READY) != 0);
    assert(result.motor_packet_length == MOTOR_COMMAND_PACKET_CONTROL_PACKET_SIZE);
    assert(result.motor_packet[0] == 0x80);

    uint8_t response_length =
        usb_motor_vendor_service_prepare_response(&fixture.service, fixture.usb_packet);
    assert(response_length == 8);
    static const uint8_t expected[] = {6, 0x30, 0x2a, 4, 0, 0xc1, 0xaa, 0xbb};
    assert(memcmp(fixture.usb_packet, expected, sizeof(expected)) == 0);
    assert(fixture.service.download.offset == 0);
    response_length = usb_motor_vendor_service_next_response(&fixture.service, fixture.usb_packet);
    assert(response_length == 8);

    uint8_t acknowledgement[USB_MOTOR_RESPONSE_ACKNOWLEDGEMENT_SIZE] = {0};
    acknowledgement[0] = 1;
    acknowledgement[5] = USB_MOTOR_RESPONSE_REPORT_ID;
    acknowledgement[7] = 4;
    bool response_acknowledged =
        usb_motor_vendor_service_acknowledge_response(&fixture.service, acknowledgement);
    assert(response_acknowledged);
    response_length = usb_motor_vendor_service_next_response(&fixture.service, fixture.usb_packet);
    assert(response_length == 6);
    assert(fixture.usb_packet[0] == 6 && fixture.usb_packet[1] == 0xa0 &&
           fixture.usb_packet[2] == 0x2a && fixture.usb_packet[3] == 0 &&
           fixture.usb_packet[4] == 4);
    assert(!fixture.service.response_active);
}

static void test_acknowledges_segmented_upload_progress(void) {
    Fixture fixture;
    fixture_init(&fixture);
    uint8_t request[USB_FEATURE_UPLOAD_PACKET_SIZE] = {6, 0xf0, 7, 9, 0x89, 0};

    UsbMotorVendorServiceResult result = usb_motor_vendor_service_accept_usb(
        &fixture.service, request, sizeof(request), fixture.usb_packet);

    assert(result.actions == (USB_MOTOR_VENDOR_ACTION_CLAIM | USB_MOTOR_VENDOR_ACTION_WRITE_USB));
    assert(result.usb_packet_length == USB_FEATURE_UPLOAD_ACKNOWLEDGEMENT_SIZE);
    assert(fixture.usb_packet[2] == 7 && fixture.usb_packet[5] == 6 &&
           fixture.usb_packet[6] == 0xf0 && fixture.usb_packet[7] == 9 &&
           fixture.usb_packet[11] == 0);
}

static void test_preserves_active_response_storage(void) {
    Fixture fixture;
    fixture_init(&fixture);
    uint8_t request[USB_FEATURE_UPLOAD_PACKET_SIZE] = {6, 0x30, 0x2a, 12, 0, 0xc1, 0x12, 0x34};
    UsbMotorVendorServiceResult result = usb_motor_vendor_service_accept_usb(
        &fixture.service, request, sizeof(request), fixture.usb_packet);
    assert((result.actions & USB_MOTOR_VENDOR_ACTION_WRITE_MOTOR) != 0);
    motor_command_channel_mark_written(&fixture.channel, result.motor_packet);

    static const uint8_t first_payload[] = {0xc1, 0xaa, 0xbb};
    static const uint8_t second_payload[] = {0xc1, 0xcc, 0xdd};
    uint8_t first_response[16];
    uint8_t second_response[16];
    uint16_t first_length;
    uint16_t second_length;
    bool first_response_encoded = motor_command_packet_payload_encode(
        0, 0, 0, first_payload, sizeof(first_payload), first_response, sizeof(first_response),
        &first_length);
    assert(first_response_encoded);
    bool second_response_encoded = motor_command_packet_payload_encode(
        0, 0, 0, second_payload, sizeof(second_payload), second_response, sizeof(second_response),
        &second_length);
    assert(second_response_encoded);
    result = usb_motor_vendor_service_accept_motor(&fixture.service, first_response, first_length);
    assert((result.actions & USB_MOTOR_VENDOR_ACTION_RESPONSE_READY) != 0);

    static const uint8_t expected_response[] = {6, 0x30, 0x2a, 4, 0, 0xc1, 0xaa, 0xbb};
    uint8_t response_length =
        usb_motor_vendor_service_prepare_response(&fixture.service, fixture.usb_packet);
    assert(response_length == sizeof(expected_response));
    assert(memcmp(fixture.usb_packet, expected_response, sizeof(expected_response)) == 0);

    result = usb_motor_vendor_service_accept_usb(&fixture.service, request, sizeof(request),
                                                 fixture.usb_packet);
    assert(result.actions == USB_MOTOR_VENDOR_ACTION_CLAIM);
    assert(!fixture.channel.command_pending);

    result = usb_motor_vendor_service_accept_motor(&fixture.service, second_response, second_length);
    assert((result.actions & USB_MOTOR_VENDOR_ACTION_RESPONSE_READY) == 0);
    response_length =
        usb_motor_vendor_service_prepare_response(&fixture.service, fixture.usb_packet);
    assert(response_length == sizeof(expected_response));
    assert(memcmp(fixture.usb_packet, expected_response, sizeof(expected_response)) == 0);
}

static void test_rejects_shared_response_storage(void) {
    Fixture fixture;
    fixture_init(&fixture);
    UsbMotorVendorService service;
    UsbMotorVendorServiceBuffers buffers = fixture.service.buffers;

    buffers.application_data = fixture.pending_payload;
    bool initialized = usb_motor_vendor_service_init(&service, &fixture.channel, &buffers);
    assert(!initialized);
    buffers.application_data = fixture.upload_assembly;
    initialized = usb_motor_vendor_service_init(&service, &fixture.channel, &buffers);
    assert(!initialized);
    buffers.application_data = fixture.application_data;
    buffers.upload_assembly = fixture.pending_payload;
    initialized = usb_motor_vendor_service_init(&service, &fixture.channel, &buffers);
    assert(!initialized);
}

static void test_retries_completed_segmented_upload_after_channel_space(void) {
    Fixture fixture;
    fixture_init(&fixture);
    static const uint8_t occupied_payload[] = {0xc1, 0x10};
    bool payload_queued = motor_command_channel_queue_payload(
        &fixture.channel, occupied_payload, sizeof(occupied_payload));
    assert(payload_queued);

    uint8_t logical[12] = {0};
    logical[9] = 0xc1;
    logical[10] = 0xaa;
    logical[11] = 0xbb;
    uint8_t request[USB_FEATURE_UPLOAD_PACKET_SIZE] = {0};
    request[0] = USB_MOTOR_COMMAND_REPORT_ID;
    request[1] = 0xf0;
    request[2] = 7;
    request[3] = sizeof(logical);
    request[4] = (uint8_t)(0x80 | sizeof(logical));
    memcpy(request + 6, logical, sizeof(logical));
    UsbMotorVendorServiceResult result = usb_motor_vendor_service_accept_usb(
        &fixture.service, request, sizeof(request), fixture.usb_packet);
    assert((result.actions & USB_MOTOR_VENDOR_ACTION_WRITE_USB) != 0);

    request[1] = 0xa0;
    request[2] = 8;
    request[3] = 0;
    result = usb_motor_vendor_service_accept_usb(&fixture.service, request, sizeof(request),
                                                 fixture.usb_packet);
    assert(result.actions == USB_MOTOR_VENDOR_ACTION_CLAIM);
    assert(fixture.service.pending_payload == fixture.upload_assembly + 1);
    assert(fixture.service.pending_payload_length == sizeof(logical) - 9);
    assert(!fixture.service.upload.feature.active && !fixture.service.upload.feature.complete);
    assert(memcmp(fixture.service.pending_payload, logical + 1,
                  fixture.service.pending_payload_length) == 0);

    motor_command_channel_reset(&fixture.channel);
    command_transport_claim(&fixture.transport, MOTOR_COMMAND_MAILBOX_OWNER);
    result = usb_motor_vendor_service_run_mailbox(&fixture.service, &fixture.exchange,
                                                  &fixture.transport);
    assert((result.actions & USB_MOTOR_VENDOR_ACTION_WRITE_MOTOR) != 0);
    assert(result.motor_packet == fixture.motor_transmit);
    assert(fixture.service.pending_payload == 0);
    assert(fixture.service.pending_payload_length == 0);
    assert(!fixture.service.upload.feature.active && !fixture.service.upload.feature.complete);
    assert(fixture.channel.pending_payload_length == sizeof(logical) - 9);
    assert(memcmp(fixture.pending_payload, logical + 1, sizeof(logical) - 9) == 0);
}

static void test_maps_restart_release_and_retry(void) {
    Fixture fixture;
    fixture_init(&fixture);
    uint8_t request[USB_FEATURE_UPLOAD_PACKET_SIZE] = {6, 0x30, 1, 9, 1, 1};

    UsbMotorVendorServiceResult result = usb_motor_vendor_service_accept_usb(
        &fixture.service, request, sizeof(request), fixture.usb_packet);
    assert(result.actions == (USB_MOTOR_VENDOR_ACTION_CLAIM | USB_MOTOR_VENDOR_ACTION_RESTART));

    request[5] = 0;
    result = usb_motor_vendor_service_accept_usb(&fixture.service, request, sizeof(request),
                                                 fixture.usb_packet);
    assert(result.actions == (USB_MOTOR_VENDOR_ACTION_CLAIM | USB_MOTOR_VENDOR_ACTION_RELEASE));

    uint8_t invalid[MOTOR_COMMAND_PACKET_CONTROL_PACKET_SIZE] = {0};
    result = usb_motor_vendor_service_accept_motor(&fixture.service, invalid, sizeof(invalid));
    assert(result.actions == USB_MOTOR_VENDOR_ACTION_WRITE_MOTOR);
    assert(result.motor_packet_length == MOTOR_COMMAND_PACKET_CONTROL_PACKET_SIZE);
    assert(result.motor_packet[0] == 0xa0);
    assert(motor_command_packet_checksum_valid(fixture.motor_transmit, result.motor_packet_length));
}

static void test_runs_usb_channel_through_mailbox(void) {
    Fixture fixture;
    fixture_init(&fixture);
    uint8_t request[USB_FEATURE_UPLOAD_PACKET_SIZE] = {6, 0x30, 0x2a, 12, 0, 0xc1, 0x12, 0x34};

    UsbMotorVendorServiceResult result = usb_motor_vendor_service_accept_usb_mailbox(
        &fixture.service, &fixture.exchange, &fixture.transport, request, sizeof(request),
        fixture.usb_packet);
    assert(command_transport_is_owner(&fixture.transport, MOTOR_COMMAND_MAILBOX_OWNER));
    assert(fixture.exchange.write_packet == fixture.motor_transmit);
    assert(fixture.exchange.write_length == 9);

    result = usb_motor_vendor_service_run_mailbox(&fixture.service, &fixture.exchange,
                                                  &fixture.transport);
    assert(result.mailbox_event == MOTOR_COMMAND_MAILBOX_EXCHANGE_NONE);

    static const uint8_t idle_control[MOTOR_COMMAND_MAILBOX_CONTROL_SIZE] = {0};
    complete_mailbox_read(&fixture, idle_control, sizeof(idle_control));
    result = usb_motor_vendor_service_run_mailbox(&fixture.service, &fixture.exchange,
                                                  &fixture.transport);
    assert(result.mailbox_event == MOTOR_COMMAND_MAILBOX_EXCHANGE_NONE);
    complete_mailbox_write(&fixture);
    result = usb_motor_vendor_service_run_mailbox(&fixture.service, &fixture.exchange,
                                                  &fixture.transport);
    assert(result.mailbox_event == MOTOR_COMMAND_MAILBOX_EXCHANGE_PACKET_WRITTEN);
    assert(!fixture.channel.command_pending || fixture.channel.command_sent);
    result = usb_motor_vendor_service_run_mailbox(&fixture.service, &fixture.exchange,
                                                  &fixture.transport);
    assert(result.mailbox_event == MOTOR_COMMAND_MAILBOX_EXCHANGE_NONE);

    static const uint8_t motor_payload[] = {0xc1, 0xaa, 0xbb};
    uint8_t motor_response[16];
    uint16_t motor_response_length;
    bool response_encoded = motor_command_packet_payload_encode(
        0, 0, 0, motor_payload, sizeof(motor_payload), motor_response, sizeof(motor_response),
        &motor_response_length);
    assert(response_encoded);
    uint8_t control[MOTOR_COMMAND_MAILBOX_CONTROL_SIZE] = {
        MOTOR_COMMAND_MAILBOX_CONTROL_PAYLOAD_AVAILABLE,
        0,
        (uint8_t)(motor_response_length >> 8),
        (uint8_t)motor_response_length,
    };
    complete_mailbox_read(&fixture, control, sizeof(control));
    (void)usb_motor_vendor_service_run_mailbox(&fixture.service, &fixture.exchange,
                                               &fixture.transport);
    complete_mailbox_read(&fixture, motor_response, motor_response_length);

    result = usb_motor_vendor_service_run_mailbox(&fixture.service, &fixture.exchange,
                                                  &fixture.transport);
    assert(result.mailbox_event == MOTOR_COMMAND_MAILBOX_EXCHANGE_PACKET_READ);
    assert((result.actions & USB_MOTOR_VENDOR_ACTION_RESPONSE_READY) != 0);
    assert((result.actions & USB_MOTOR_VENDOR_ACTION_WRITE_MOTOR) != 0);
    assert(fixture.exchange.write_packet == fixture.channel.control_packet);
    assert(fixture.exchange.write_length == MOTOR_COMMAND_PACKET_CONTROL_PACKET_SIZE);
}

static void test_applies_mailbox_restart_and_release(void) {
    Fixture fixture;
    fixture_init(&fixture);
    static const uint8_t pending[] = {1, 2, 3};
    bool queued = motor_command_mailbox_exchange_queue(&fixture.exchange, pending, sizeof(pending));
    assert(queued);
    uint8_t request[USB_FEATURE_UPLOAD_PACKET_SIZE] = {6, 0x30, 1, 9, 1, 1};

    UsbMotorVendorServiceResult result = usb_motor_vendor_service_accept_usb_mailbox(
        &fixture.service, &fixture.exchange, &fixture.transport, request, sizeof(request),
        fixture.usb_packet);
    assert((result.actions & USB_MOTOR_VENDOR_ACTION_RESTART) != 0);
    assert(command_transport_is_owner(&fixture.transport, MOTOR_COMMAND_MAILBOX_OWNER));
    assert(fixture.exchange.write_packet == 0);

    request[5] = 0;
    result = usb_motor_vendor_service_accept_usb_mailbox(&fixture.service, &fixture.exchange,
                                                         &fixture.transport, request,
                                                         sizeof(request), fixture.usb_packet);
    assert((result.actions & USB_MOTOR_VENDOR_ACTION_RELEASE) != 0);
    assert(!command_transport_is_owner(&fixture.transport, MOTOR_COMMAND_MAILBOX_OWNER));
}

static void test_retries_after_lower_layer_write_refusal(void) {
    Fixture fixture;
    fixture_init(&fixture);
    uint8_t request[USB_FEATURE_UPLOAD_PACKET_SIZE] = {6, 0x30, 0x2a, 12, 0, 0xc1, 0x12, 0x34};

    UsbMotorVendorServiceResult result = usb_motor_vendor_service_accept_usb_mailbox(
        &fixture.service, &fixture.exchange, &fixture.transport, request, sizeof(request),
        fixture.usb_packet);
    assert((result.actions & USB_MOTOR_VENDOR_ACTION_WRITE_MOTOR) != 0);
    (void)usb_motor_vendor_service_run_mailbox(&fixture.service, &fixture.exchange,
                                               &fixture.transport);
    static const uint8_t idle_control[MOTOR_COMMAND_MAILBOX_CONTROL_SIZE] = {0};
    complete_mailbox_read(&fixture, idle_control, sizeof(idle_control));
    (void)usb_motor_vendor_service_run_mailbox(&fixture.service, &fixture.exchange,
                                               &fixture.transport);
    assert(fixture.exchange.phase == MOTOR_COMMAND_MAILBOX_EXCHANGE_PAYLOAD_WRITE_WAIT);
    assert(command_transport_request_sent(&fixture.transport));
    static const uint8_t rejected[] = {0};
    command_transport_receive(&fixture.transport, rejected, sizeof(rejected));
    result = usb_motor_vendor_service_run_mailbox(&fixture.service, &fixture.exchange,
                                                  &fixture.transport);
    assert(result.mailbox_event == MOTOR_COMMAND_MAILBOX_EXCHANGE_TRANSFER_FAILED);
    assert(fixture.channel.command_pending);
    assert(!fixture.channel.command_sent);
    assert(fixture.exchange.write_packet == fixture.channel.buffers.transmit);
}

static void test_requests_retry_after_lower_layer_read_refusal(void) {
    Fixture fixture;
    fixture_init(&fixture);
    static const uint8_t control[] = {MOTOR_COMMAND_MAILBOX_CONTROL_PAYLOAD_AVAILABLE, 0, 0, 3};
    static const uint8_t outgoing[] = {0xc1, 0x12};

    command_transport_claim(&fixture.transport, MOTOR_COMMAND_MAILBOX_OWNER);
    bool payload_queued =
        motor_command_channel_queue_payload(&fixture.channel, outgoing, sizeof(outgoing));
    assert(payload_queued);
    (void)usb_motor_vendor_service_run_mailbox(&fixture.service, &fixture.exchange,
                                               &fixture.transport);
    complete_mailbox_read(&fixture, control, sizeof(control));
    (void)usb_motor_vendor_service_run_mailbox(&fixture.service, &fixture.exchange,
                                               &fixture.transport);
    assert(fixture.exchange.phase == MOTOR_COMMAND_MAILBOX_EXCHANGE_PAYLOAD_READ_WAIT);
    assert(command_transport_request_sent(&fixture.transport));
    static const uint8_t rejected[] = {0};
    command_transport_receive(&fixture.transport, rejected, sizeof(rejected));
    UsbMotorVendorServiceResult result = usb_motor_vendor_service_run_mailbox(
        &fixture.service, &fixture.exchange, &fixture.transport);
    assert(result.mailbox_event == MOTOR_COMMAND_MAILBOX_EXCHANGE_TRANSFER_FAILED);
    assert(result.actions & USB_MOTOR_VENDOR_ACTION_WRITE_MOTOR);
    assert(fixture.exchange.write_packet == fixture.channel.control_packet);
    assert(fixture.channel.control_packet[0] == 0xa0);
}

static void test_recovers_after_ten_control_sentinels(void) {
    Fixture fixture;
    fixture_init(&fixture);
    static const uint8_t sentinel[MOTOR_COMMAND_MAILBOX_CONTROL_SIZE] = {0, 0xff, 0xff, 0xff};
    UsbMotorVendorServiceResult result = {0};

    command_transport_claim(&fixture.transport, MOTOR_COMMAND_MAILBOX_OWNER);
    for (uint8_t count = 0; count < 10; count++) {
        result = usb_motor_vendor_service_run_mailbox(&fixture.service, &fixture.exchange,
                                                      &fixture.transport);
        assert(result.mailbox_event == MOTOR_COMMAND_MAILBOX_EXCHANGE_NONE);
        complete_mailbox_read(&fixture, sentinel, sizeof(sentinel));
        result = usb_motor_vendor_service_run_mailbox(&fixture.service, &fixture.exchange,
                                                      &fixture.transport);
        if (count < 9) {
            assert(result.mailbox_event == MOTOR_COMMAND_MAILBOX_EXCHANGE_NONE);
            assert(fixture.exchange.control_retry_count == count + 1);
        }
    }
    assert(result.mailbox_event == MOTOR_COMMAND_MAILBOX_EXCHANGE_RECOVERED);
    assert(fixture.exchange.phase == MOTOR_COMMAND_MAILBOX_EXCHANGE_CONTROL_QUEUE);
    assert(!fixture.channel.command_pending);
    assert(!command_transport_is_owner(&fixture.transport, MOTOR_COMMAND_MAILBOX_OWNER));
}

static void test_scheduler_requeues_and_then_recovers_live_command(void) {
    Fixture fixture;
    fixture_init(&fixture);
    static const uint8_t payload[] = {0xc1, 0x12};

    command_transport_claim(&fixture.transport, MOTOR_COMMAND_MAILBOX_OWNER);
    bool payload_queued = motor_command_channel_queue_payload(&fixture.channel, payload,
                                                              sizeof(payload));
    assert(payload_queued);
    motor_command_channel_mark_written(&fixture.channel, fixture.motor_transmit);
    fixture.channel.scheduler.timeout_ticks = 0;
    MotorCommandChannelMailboxEvent event =
        motor_command_channel_mailbox_run(&fixture.channel, &fixture.exchange, &fixture.transport);
    assert(event.mailbox_event == MOTOR_COMMAND_MAILBOX_EXCHANGE_NONE);
    assert(fixture.channel.command_pending);
    assert(!fixture.channel.command_sent);
    assert(fixture.exchange.write_packet == fixture.channel.buffers.transmit);

    fixture.channel.command_sent = true;
    fixture.channel.scheduler.timeout_ticks = 0;
    fixture.channel.scheduler.retry_count = 2;
    fixture.exchange.phase = MOTOR_COMMAND_MAILBOX_EXCHANGE_CONTROL_QUEUE;
    fixture.exchange.write_packet = 0;
    fixture.transport.phase = COMMAND_TRANSPORT_IDLE;
    event =
        motor_command_channel_mailbox_run(&fixture.channel, &fixture.exchange, &fixture.transport);
    assert(event.mailbox_event == MOTOR_COMMAND_MAILBOX_EXCHANGE_NONE);
    assert(fixture.channel.command_pending);
    assert(!fixture.channel.command_sent);
    assert(fixture.channel.pending_payload_length == 1);
    assert(fixture.channel.buffers.pending_payload[0] == 0xfe);
    assert(fixture.channel.buffers.transmit[4] == 0xfe);
    assert(!fixture.channel.reset_pending);
    assert(fixture.exchange.write_packet == fixture.channel.buffers.transmit);
}

static void test_motor_channel_rejects_invalid_storage_and_requests(void) {
    Fixture fixture;
    fixture_init(&fixture);
    MotorCommandChannel channel;
    MotorCommandChannelBuffers buffers = fixture.channel.buffers;
    uint8_t payload[129] = {0};

    bool initialized = motor_command_channel_init(NULL, &buffers);
    assert(!initialized);
    initialized = motor_command_channel_init(&channel, NULL);
    assert(!initialized);
    buffers.receive_assembly = NULL;
    initialized = motor_command_channel_init(&channel, &buffers);
    assert(!initialized);
    buffers = fixture.channel.buffers;
    buffers.receive_assembly_capacity = 0;
    initialized = motor_command_channel_init(&channel, &buffers);
    assert(!initialized);
    buffers = fixture.channel.buffers;
    buffers.transmit = NULL;
    initialized = motor_command_channel_init(&channel, &buffers);
    assert(!initialized);
    buffers = fixture.channel.buffers;
    buffers.transmit_capacity = MOTOR_COMMAND_PACKET_CONTROL_PACKET_SIZE - 1;
    initialized = motor_command_channel_init(&channel, &buffers);
    assert(!initialized);
    buffers = fixture.channel.buffers;
    buffers.pending_payload = NULL;
    initialized = motor_command_channel_init(&channel, &buffers);
    assert(!initialized);
    buffers = fixture.channel.buffers;
    buffers.pending_payload_capacity = 0;
    initialized = motor_command_channel_init(&channel, &buffers);
    assert(!initialized);

    bool queued = motor_command_channel_queue_payload(NULL, payload, 1);
    assert(!queued);
    queued = motor_command_channel_queue_payload(&fixture.channel, NULL, 1);
    assert(!queued);
    fixture.channel.command_pending = true;
    queued = motor_command_channel_queue_payload(&fixture.channel, payload, 1);
    assert(!queued);
    fixture.channel.command_pending = false;
    queued = motor_command_channel_queue_payload(&fixture.channel, payload, sizeof(payload));
    assert(!queued);
    fixture.channel.buffers.transmit_capacity = MOTOR_COMMAND_PACKET_ENCODING_OVERHEAD;
    queued = motor_command_channel_queue_payload(&fixture.channel, payload, 1);
    assert(!queued);
    fixture.channel.buffers.transmit_capacity = sizeof(fixture.motor_transmit);

    bool reset_queued = motor_command_channel_queue_sequence_reset(NULL);
    assert(!reset_queued);
    fixture.channel.command_pending = true;
    reset_queued = motor_command_channel_queue_sequence_reset(&fixture.channel);
    assert(!reset_queued);
    fixture.channel.command_pending = false;
    fixture.channel.buffers.transmit_capacity = MOTOR_COMMAND_PACKET_CONTROL_PACKET_SIZE - 1;
    reset_queued = motor_command_channel_queue_sequence_reset(&fixture.channel);
    assert(!reset_queued);
    fixture.channel.buffers.transmit_capacity = sizeof(fixture.motor_transmit);
    reset_queued = motor_command_channel_queue_sequence_reset(&fixture.channel);
    assert(reset_queued);
    motor_command_channel_mark_written(&fixture.channel, fixture.channel.buffers.transmit);
    bool information_queued = motor_command_channel_queue_information_request(&fixture.channel, 2);
    assert(!information_queued);
    information_queued = motor_command_channel_queue_information_request(&fixture.channel, 5);
    assert(!information_queued);
    information_queued = motor_command_channel_queue_information_request(&fixture.channel, 3);
    assert(information_queued);
    information_queued = motor_command_channel_queue_information_request(&fixture.channel, 4);
    assert(!information_queued);
    assert(motor_command_channel_application(NULL) == NULL);
    assert(motor_command_channel_accept(NULL, payload, 1).actions ==
           MOTOR_COMMAND_CHANNEL_ACTION_NONE);
}

int main(void) {
    test_bridges_compact_command_and_response();
    test_acknowledges_segmented_upload_progress();
    test_preserves_active_response_storage();
    test_rejects_shared_response_storage();
    test_retries_completed_segmented_upload_after_channel_space();
    test_maps_restart_release_and_retry();
    test_runs_usb_channel_through_mailbox();
    test_applies_mailbox_restart_and_release();
    test_retries_after_lower_layer_write_refusal();
    test_requests_retry_after_lower_layer_read_refusal();
    test_recovers_after_ten_control_sentinels();
    test_scheduler_requeues_and_then_recovers_live_command();
    test_motor_channel_rejects_invalid_storage_and_requests();
    return 0;
}
