#include <assert.h>
#include <stdint.h>
#include <string.h>

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
        .pending_payload = fixture->application_data,
        .pending_payload_capacity = sizeof(fixture->application_data),
    };
    UsbMotorVendorServiceBuffers buffers = {
        .upload_assembly = fixture->upload_assembly,
        .upload_assembly_capacity = sizeof(fixture->upload_assembly),
        .application_data = fixture->application_data,
        .application_data_capacity = sizeof(fixture->application_data),
    };
    assert(motor_command_channel_init(&fixture->channel, &channel_buffers));
    assert(usb_motor_vendor_service_init(&fixture->service, &fixture->channel, &buffers));
    assert(motor_command_mailbox_exchange_init(&fixture->exchange, fixture->mailbox_receive,
                                               sizeof(fixture->mailbox_receive)));
    command_transport_init(&fixture->transport);
}

static void complete_mailbox_read(Fixture *fixture, const uint8_t *data, uint16_t length) {
    uint8_t response[MEMORY_TRANSFER_MAX_READ_SIZE + 2] = {1, 0};
    memcpy(response + 2, data, length);
    assert(command_transport_request_sent(&fixture->transport));
    command_transport_receive(&fixture->transport, response, length + 2);
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
    assert(fixture.motor_transmit[0] == 3);
    assert(memcmp(fixture.motor_transmit + 4, request + 5, 3) == 0);
    assert(motor_command_packet_checksum_valid(fixture.motor_transmit, result.motor_packet_length));

    static const uint8_t motor_payload[] = {0xc1, 0xaa, 0xbb};
    uint8_t motor_response[16];
    uint16_t motor_response_length;
    assert(motor_command_packet_payload_encode(0, 0, 0, motor_payload, sizeof(motor_payload),
                                               motor_response, sizeof(motor_response),
                                               &motor_response_length));
    result = usb_motor_vendor_service_accept_motor(&fixture.service, motor_response,
                                                   motor_response_length);
    assert((result.actions & USB_MOTOR_VENDOR_ACTION_WRITE_MOTOR) != 0);
    assert((result.actions & USB_MOTOR_VENDOR_ACTION_RESPONSE_READY) != 0);
    assert(result.motor_packet_length == MOTOR_COMMAND_PACKET_CONTROL_PACKET_SIZE);
    assert(fixture.motor_transmit[0] == 0x80);

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
    assert(usb_motor_vendor_service_acknowledge_response(&fixture.service, acknowledgement));
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
    assert(fixture.motor_transmit[0] == 0xa0);
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

    static const uint8_t motor_payload[] = {0xc1, 0xaa, 0xbb};
    uint8_t motor_response[16];
    uint16_t motor_response_length;
    assert(motor_command_packet_payload_encode(0, 0, 0, motor_payload, sizeof(motor_payload),
                                               motor_response, sizeof(motor_response),
                                               &motor_response_length));
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
    assert(fixture.exchange.write_packet == fixture.motor_transmit);
    assert(fixture.exchange.write_length == MOTOR_COMMAND_PACKET_CONTROL_PACKET_SIZE);
}

static void test_applies_mailbox_restart_and_release(void) {
    Fixture fixture;
    fixture_init(&fixture);
    static const uint8_t pending[] = {1, 2, 3};
    assert(motor_command_mailbox_exchange_queue(&fixture.exchange, pending, sizeof(pending)));
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

static void test_motor_channel_rejects_invalid_storage_and_requests(void) {
    Fixture fixture;
    fixture_init(&fixture);
    MotorCommandChannel channel;
    MotorCommandChannelBuffers buffers = fixture.channel.buffers;
    uint8_t payload[129] = {0};

    assert(!motor_command_channel_init(NULL, &buffers));
    assert(!motor_command_channel_init(&channel, NULL));
    buffers.receive_assembly = NULL;
    assert(!motor_command_channel_init(&channel, &buffers));
    buffers = fixture.channel.buffers;
    buffers.receive_assembly_capacity = 0;
    assert(!motor_command_channel_init(&channel, &buffers));
    buffers = fixture.channel.buffers;
    buffers.transmit = NULL;
    assert(!motor_command_channel_init(&channel, &buffers));
    buffers = fixture.channel.buffers;
    buffers.transmit_capacity = MOTOR_COMMAND_PACKET_CONTROL_PACKET_SIZE - 1;
    assert(!motor_command_channel_init(&channel, &buffers));
    buffers = fixture.channel.buffers;
    buffers.pending_payload = NULL;
    assert(!motor_command_channel_init(&channel, &buffers));
    buffers = fixture.channel.buffers;
    buffers.pending_payload_capacity = 0;
    assert(!motor_command_channel_init(&channel, &buffers));

    assert(!motor_command_channel_queue_payload(NULL, payload, 1));
    assert(!motor_command_channel_queue_payload(&fixture.channel, NULL, 1));
    fixture.channel.command_pending = true;
    assert(!motor_command_channel_queue_payload(&fixture.channel, payload, 1));
    fixture.channel.command_pending = false;
    assert(!motor_command_channel_queue_payload(&fixture.channel, payload, sizeof(payload)));
    fixture.channel.buffers.transmit_capacity = MOTOR_COMMAND_PACKET_ENCODING_OVERHEAD;
    assert(!motor_command_channel_queue_payload(&fixture.channel, payload, 1));
    fixture.channel.buffers.transmit_capacity = sizeof(fixture.motor_transmit);

    assert(!motor_command_channel_queue_sequence_reset(NULL));
    fixture.channel.command_pending = true;
    assert(!motor_command_channel_queue_sequence_reset(&fixture.channel));
    fixture.channel.command_pending = false;
    fixture.channel.buffers.transmit_capacity = MOTOR_COMMAND_PACKET_CONTROL_PACKET_SIZE - 1;
    assert(!motor_command_channel_queue_sequence_reset(&fixture.channel));
    fixture.channel.buffers.transmit_capacity = sizeof(fixture.motor_transmit);
    assert(motor_command_channel_queue_sequence_reset(&fixture.channel));
    assert(!motor_command_channel_queue_information_request(&fixture.channel, 2));
    assert(!motor_command_channel_queue_information_request(&fixture.channel, 5));
    assert(motor_command_channel_queue_information_request(&fixture.channel, 3));
    assert(!motor_command_channel_queue_information_request(&fixture.channel, 4));
    assert(motor_command_channel_application(NULL) == NULL);
    assert(motor_command_channel_accept(NULL, payload, 1).actions ==
           MOTOR_COMMAND_CHANNEL_ACTION_NONE);
}

int main(void) {
    test_bridges_compact_command_and_response();
    test_acknowledges_segmented_upload_progress();
    test_maps_restart_release_and_retry();
    test_runs_usb_channel_through_mailbox();
    test_applies_mailbox_restart_and_release();
    test_motor_channel_rejects_invalid_storage_and_requests();
    return 0;
}
