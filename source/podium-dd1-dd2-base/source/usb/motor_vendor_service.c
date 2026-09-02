#include "usb/motor_vendor_service.h"

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "motor/command_channel.h"
#include "motor/command_channel_mailbox.h"
#include "motor/command_mailbox.h"
#include "usb/feature_upload_acknowledgement.h"
#include "usb/motor_command_upload.h"
#include "usb/motor_response_download.h"

/**
 * @brief Maps one channel event into the USB vendor response service.
 *
 * Publishes motor writes and copies forwardable application responses into stable USB download
 * storage before initializing compact or segmented response framing.
 *
 * @param[in,out] service Motor vendor service receiving the event.
 * @param[in] channel_event Motor-command protocol event to apply.
 * @return Motor write and USB response-ready actions produced by the event.
 */
static UsbMotorVendorServiceResult apply_channel_event(UsbMotorVendorService *service,
                                                       MotorCommandChannelEvent channel_event) {
    UsbMotorVendorServiceResult result = {0};
    if ((channel_event.actions & MOTOR_COMMAND_CHANNEL_ACTION_WRITE) != 0) {
        result.actions = USB_MOTOR_VENDOR_ACTION_WRITE_MOTOR;
        result.motor_packet = channel_event.packet;
        result.motor_packet_length = channel_event.packet_length;
    }
    MotorCommandApplicationEvent application = channel_event.application;
    if (application.result != MOTOR_COMMAND_APPLICATION_FORWARD || application.forward_data == 0 ||
        application.forward_length > service->buffers.application_data_capacity) {
        return result;
    }
    memmove(service->buffers.application_data, application.forward_data,
            application.forward_length);
    service->response_length = application.forward_length;
    service->response_active = usb_motor_response_download_init(
        &service->download, service->usb_sequence, service->response_length);
    if (service->response_active) {
        result.actions =
            (UsbMotorVendorAction)(result.actions | USB_MOTOR_VENDOR_ACTION_RESPONSE_READY);
    }
    return result;
}

bool usb_motor_vendor_service_init(UsbMotorVendorService *service, MotorCommandChannel *channel,
                                   const UsbMotorVendorServiceBuffers *buffers) {
    if (service == 0 || channel == 0 || buffers == 0 || buffers->upload_assembly == 0 ||
        buffers->upload_assembly_capacity == 0 || buffers->application_data == 0 ||
        buffers->application_data_capacity == 0) {
        return false;
    }
    *service = (UsbMotorVendorService){.channel = channel, .buffers = *buffers};
    if (!usb_motor_command_upload_init(&service->upload, buffers->upload_assembly,
                                       buffers->upload_assembly_capacity)) {
        return false;
    }
    memset(&service->download, 0, sizeof(service->download));
    return true;
}

UsbMotorVendorServiceResult usb_motor_vendor_service_accept_usb(
    UsbMotorVendorService *service, const uint8_t request[USB_FEATURE_UPLOAD_PACKET_SIZE],
    uint8_t length, uint8_t usb_packet[USB_FEATURE_UPLOAD_PACKET_SIZE]) {
    UsbMotorVendorServiceResult result = {0};
    if (service == 0 || request == 0 || length != USB_FEATURE_UPLOAD_PACKET_SIZE ||
        request[0] != USB_MOTOR_COMMAND_REPORT_ID) {
        return result;
    }
    result.actions = USB_MOTOR_VENDOR_ACTION_CLAIM;
    UsbMotorCommandUploadEvent event =
        usb_motor_command_upload_accept(&service->upload, request, length);
    service->usb_sequence = event.sequence;

    if (event.result == USB_MOTOR_COMMAND_UPLOAD_RESTART) {
        motor_command_channel_reset(service->channel);
        result.actions = (UsbMotorVendorAction)(result.actions | USB_MOTOR_VENDOR_ACTION_RESTART);
        return result;
    }
    if (event.result == USB_MOTOR_COMMAND_UPLOAD_RELEASE) {
        result.actions = (UsbMotorVendorAction)(result.actions | USB_MOTOR_VENDOR_ACTION_RELEASE);
        return result;
    }
    if (event.result == USB_MOTOR_COMMAND_UPLOAD_ACKNOWLEDGEMENT && usb_packet != 0 &&
        usb_feature_upload_acknowledgement_segmented_encode(
            event.sequence, request, service->upload.feature.offset,
            service->upload.feature.total_length, usb_packet)) {
        result.actions = (UsbMotorVendorAction)(result.actions | USB_MOTOR_VENDOR_ACTION_WRITE_USB);
        result.usb_packet_length = USB_FEATURE_UPLOAD_ACKNOWLEDGEMENT_SIZE;
        return result;
    }
    if (event.result != USB_MOTOR_COMMAND_UPLOAD_COMMAND) {
        return result;
    }

    if (!motor_command_channel_queue_payload(service->channel, event.payload,
                                             event.payload_length)) {
        return result;
    }
    if (event.acknowledgement_report_id == USB_MOTOR_COMMAND_COMPACT_ACKNOWLEDGEMENT_REPORT_ID &&
        usb_packet != 0 &&
        usb_feature_upload_acknowledgement_compact_encode(event.sequence, request, usb_packet)) {
        result.actions = (UsbMotorVendorAction)(result.actions | USB_MOTOR_VENDOR_ACTION_WRITE_USB);
        result.usb_packet_length = USB_FEATURE_UPLOAD_ACKNOWLEDGEMENT_SIZE;
    }
    result.actions = (UsbMotorVendorAction)(result.actions | USB_MOTOR_VENDOR_ACTION_WRITE_MOTOR);
    result.motor_packet = service->channel->buffers.transmit;
    result.motor_packet_length = service->channel->transmit_length;
    return result;
}

UsbMotorVendorServiceResult usb_motor_vendor_service_accept_usb_mailbox(
    UsbMotorVendorService *service, MotorCommandMailboxExchange *exchange,
    CommandTransport *transport, const uint8_t request[USB_FEATURE_UPLOAD_PACKET_SIZE],
    uint8_t length, uint8_t usb_packet[USB_FEATURE_UPLOAD_PACKET_SIZE]) {
    UsbMotorVendorServiceResult result = {0};
    if (exchange == 0 || transport == 0) {
        result.mailbox_event = MOTOR_COMMAND_MAILBOX_EXCHANGE_FAILED;
        return result;
    }
    if (service != 0 && request != 0 && length == USB_FEATURE_UPLOAD_PACKET_SIZE &&
        request[0] == USB_MOTOR_COMMAND_REPORT_ID) {
        command_transport_claim(transport, MOTOR_COMMAND_MAILBOX_OWNER);
        if (!command_transport_is_owner(transport, MOTOR_COMMAND_MAILBOX_OWNER)) {
            result.mailbox_event = MOTOR_COMMAND_MAILBOX_EXCHANGE_FAILED;
            return result;
        }
    }
    result = usb_motor_vendor_service_accept_usb(service, request, length, usb_packet);
    if ((result.actions & USB_MOTOR_VENDOR_ACTION_RESTART) != 0) {
        motor_command_mailbox_exchange_reset(exchange);
        command_transport_release(transport, MOTOR_COMMAND_MAILBOX_OWNER);
        command_transport_claim(transport, MOTOR_COMMAND_MAILBOX_OWNER);
    }
    if ((result.actions & USB_MOTOR_VENDOR_ACTION_RELEASE) != 0) {
        motor_command_mailbox_exchange_reset(exchange);
        motor_command_channel_reset(service->channel);
        command_transport_release(transport, MOTOR_COMMAND_MAILBOX_OWNER);
        return result;
    }
    if ((result.actions & USB_MOTOR_VENDOR_ACTION_WRITE_MOTOR) != 0 &&
        !motor_command_mailbox_exchange_queue(exchange, result.motor_packet,
                                              result.motor_packet_length)) {
        result.mailbox_event = MOTOR_COMMAND_MAILBOX_EXCHANGE_FAILED;
        result.actions =
            (UsbMotorVendorAction)(result.actions & (uint8_t)~USB_MOTOR_VENDOR_ACTION_WRITE_USB);
    }
    return result;
}

UsbMotorVendorServiceResult usb_motor_vendor_service_accept_motor(UsbMotorVendorService *service,
                                                                  const uint8_t *packet,
                                                                  uint16_t length) {
    if (service == 0) {
        return (UsbMotorVendorServiceResult){0};
    }
    MotorCommandChannelEvent event = motor_command_channel_accept(service->channel, packet, length);
    return apply_channel_event(service, event);
}

UsbMotorVendorServiceResult
usb_motor_vendor_service_run_mailbox(UsbMotorVendorService *service,
                                     MotorCommandMailboxExchange *exchange,
                                     CommandTransport *transport) {
    UsbMotorVendorServiceResult result = {0};
    if (service == 0 || exchange == 0 || transport == 0 ||
        !command_transport_is_owner(transport, MOTOR_COMMAND_MAILBOX_OWNER)) {
        return result;
    }

    MotorCommandChannelMailboxEvent event =
        motor_command_channel_mailbox_run(service->channel, exchange, transport);
    result = apply_channel_event(service, event.channel_event);
    result.mailbox_event = event.mailbox_event;
    result.motor_status = event.status;
    return result;
}

uint8_t usb_motor_vendor_service_prepare_response(UsbMotorVendorService *service,
                                                  uint8_t packet[USB_MOTOR_RESPONSE_PACKET_SIZE]) {
    if (service == 0 || !service->response_active) {
        return 0;
    }
    uint16_t offset = service->download.offset;
    uint8_t continuation_count = service->download.continuation_count;
    bool awaiting_acknowledgement = service->download.awaiting_acknowledgement;
    bool complete = service->download.complete;
    uint8_t length = usb_motor_response_download_next(&service->download,
                                                      service->buffers.application_data, packet);
    service->download.offset = offset;
    service->download.continuation_count = continuation_count;
    service->download.awaiting_acknowledgement = awaiting_acknowledgement;
    service->download.complete = complete;
    return length;
}

uint8_t usb_motor_vendor_service_next_response(UsbMotorVendorService *service,
                                               uint8_t packet[USB_MOTOR_RESPONSE_PACKET_SIZE]) {
    if (service == 0 || !service->response_active) {
        return 0;
    }
    uint8_t length = usb_motor_response_download_next(&service->download,
                                                      service->buffers.application_data, packet);
    if (service->download.complete) {
        service->response_active = false;
        service->response_length = 0;
    }
    return length;
}

bool usb_motor_vendor_service_acknowledge_response(
    UsbMotorVendorService *service,
    const uint8_t acknowledgement[USB_MOTOR_RESPONSE_ACKNOWLEDGEMENT_SIZE]) {
    return service != 0 && service->response_active &&
           usb_motor_response_download_acknowledge(&service->download, acknowledgement);
}
