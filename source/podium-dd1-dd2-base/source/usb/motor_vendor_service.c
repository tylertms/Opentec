#include "usb/motor_vendor_service.h"

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "motor/command_application.h"
#include "motor/command_mailbox.h"
#include "motor/command_message.h"
#include "motor/command_packet.h"
#include "motor/command_receiver.h"
#include "motor/command_sequence.h"
#include "usb/feature_upload_acknowledgement.h"
#include "usb/motor_command_upload.h"
#include "usb/motor_response_download.h"

/**
 * @brief Resets the motor-command protocol state.
 *
 * Restores sequence, fragment, and pending-command state while retaining application data and any
 * active USB response transfer.
 *
 * @param[in,out] service Motor vendor service to reset.
 */
static void reset_protocol(UsbMotorVendorService *service) {
    motor_command_receiver_init(&service->receiver, service->buffers.receive_assembly,
                                service->buffers.receive_assembly_capacity);
    service->motor_transmit_length = 0;
    service->pending_payload_length = 0;
    service->command_pending = false;
}

/**
 * @brief Builds a motor packet for a newly uploaded vendor command.
 *
 * Retains the logical command for protocol retries, advances the two-bit transmit sequence, and
 * frames the command in normal mode against the previously received sequence.
 *
 * @param[in,out] service Active motor vendor service.
 * @param[in] payload Uploaded motor application command.
 * @param[in] payload_length Command byte count.
 * @return True when the command fits both retained application and motor transmit storage.
 */
static bool build_command(UsbMotorVendorService *service, const uint8_t *payload,
                          uint16_t payload_length) {
    if (payload == 0 || payload_length > service->buffers.application_data_capacity ||
        payload_length + MOTOR_COMMAND_PACKET_ENCODING_OVERHEAD >
            service->buffers.motor_transmit_capacity) {
        return false;
    }
    memmove(service->buffers.application_data, payload, payload_length);
    motor_command_sequence_advance(&service->receiver.sequence);
    if (!motor_command_packet_payload_encode(
            0, service->receiver.sequence.transmit, service->receiver.sequence.receive_previous,
            service->buffers.application_data, payload_length, service->buffers.motor_transmit,
            service->buffers.motor_transmit_capacity, &service->motor_transmit_length)) {
        return false;
    }
    service->pending_payload_length = payload_length;
    service->command_pending = true;
    return true;
}

/**
 * @brief Rebuilds the pending vendor command for the selected transmit sequence.
 *
 * Frames the retained logical command without advancing sequence state so retry and resend
 * requests can select the sequence already applied by the receiver.
 *
 * @param[in,out] service Active motor vendor service.
 * @return True when a retained command is pending and can be framed.
 */
static bool rebuild_command(UsbMotorVendorService *service) {
    return service->command_pending &&
           motor_command_packet_payload_encode(
               0, service->receiver.sequence.transmit, service->receiver.sequence.receive_previous,
               service->buffers.application_data, service->pending_payload_length,
               service->buffers.motor_transmit, service->buffers.motor_transmit_capacity,
               &service->motor_transmit_length);
}

/**
 * @brief Builds a motor-link control response.
 *
 * Encodes acknowledgement or retry control with the receiver's accepted previous or expected next
 * sequence and publishes the resulting five-byte motor packet.
 *
 * @param[in,out] service Active motor vendor service.
 * @param[in] retry Selects retry instead of acknowledgement control.
 * @return True when the control packet fits the motor transmit buffer.
 */
static bool build_control(UsbMotorVendorService *service, bool retry) {
    if (service->buffers.motor_transmit_capacity < MOTOR_COMMAND_PACKET_CONTROL_PACKET_SIZE) {
        return false;
    }
    if (retry) {
        motor_command_packet_retry_encode(service->receiver.sequence.receive_next,
                                          service->buffers.motor_transmit);
    } else {
        motor_command_packet_acknowledgement_encode(service->receiver.sequence.receive_previous,
                                                    service->buffers.motor_transmit);
    }
    service->motor_transmit_length = MOTOR_COMMAND_PACKET_CONTROL_PACKET_SIZE;
    return true;
}

/**
 * @brief Initializes the USB motor vendor bridge.
 *
 * Attaches caller-owned USB assembly, motor assembly, transmit, and application buffers and resets
 * upload, motor protocol, application, and response state.
 *
 * @param[out] service Motor vendor service to initialize.
 * @param[in] buffers Caller-owned storage used by the service.
 * @return True when every required buffer is present and nonempty.
 */
bool usb_motor_vendor_service_init(UsbMotorVendorService *service,
                                   const UsbMotorVendorServiceBuffers *buffers) {
    if (service == 0 || buffers == 0 || buffers->upload_assembly == 0 ||
        buffers->upload_assembly_capacity == 0 || buffers->receive_assembly == 0 ||
        buffers->receive_assembly_capacity == 0 || buffers->motor_transmit == 0 ||
        buffers->motor_transmit_capacity < MOTOR_COMMAND_PACKET_CONTROL_PACKET_SIZE ||
        buffers->application_data == 0 || buffers->application_data_capacity == 0) {
        return false;
    }
    *service = (UsbMotorVendorService){.buffers = *buffers};
    if (!usb_motor_command_upload_init(&service->upload, buffers->upload_assembly,
                                       buffers->upload_assembly_capacity)) {
        return false;
    }
    motor_command_application_init(&service->application);
    memset(&service->download, 0, sizeof(service->download));
    reset_protocol(service);
    return true;
}

/**
 * @brief Accepts one USB vendor request for the motor command channel.
 *
 * Claims report 6 traffic, maps restart and release controls, emits compact or segmented upload
 * acknowledgements, and frames completed application commands for the motor link.
 *
 * @param[in,out] service Active motor vendor service.
 * @param[in] request Sixty-four-byte USB feature request.
 * @param[in] length Received request byte count.
 * @param[out] usb_packet Destination for an upload acknowledgement.
 * @return Transport ownership, control, USB write, and motor write actions for the request.
 */
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
        reset_protocol(service);
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

    if (event.acknowledgement_report_id == USB_MOTOR_COMMAND_COMPACT_ACKNOWLEDGEMENT_REPORT_ID &&
        usb_packet != 0 &&
        usb_feature_upload_acknowledgement_compact_encode(event.sequence, request, usb_packet)) {
        result.actions = (UsbMotorVendorAction)(result.actions | USB_MOTOR_VENDOR_ACTION_WRITE_USB);
        result.usb_packet_length = USB_FEATURE_UPLOAD_ACKNOWLEDGEMENT_SIZE;
    }
    if (build_command(service, event.payload, event.payload_length)) {
        result.actions =
            (UsbMotorVendorAction)(result.actions | USB_MOTOR_VENDOR_ACTION_WRITE_MOTOR);
        result.motor_packet = service->buffers.motor_transmit;
        result.motor_packet_length = service->motor_transmit_length;
    }
    return result;
}

/**
 * @brief Accepts a USB motor request through the command mailbox.
 *
 * Applies report 6 ownership, resets both packet and mailbox state for restart requests, releases
 * the channel when requested, and queues completed motor packets for owner 0x20 transport.
 *
 * @param[in,out] service Active motor vendor service.
 * @param[in,out] exchange Motor-command mailbox exchange.
 * @param[in,out] transport Shared command transport.
 * @param[in] request Sixty-four-byte USB feature request.
 * @param[in] length Received request byte count.
 * @param[out] usb_packet Destination for an upload acknowledgement.
 * @return USB, ownership, motor-write, and mailbox actions produced by the request.
 */
UsbMotorVendorServiceResult usb_motor_vendor_service_accept_usb_mailbox(
    UsbMotorVendorService *service, MotorCommandMailboxExchange *exchange,
    CommandTransport *transport, const uint8_t request[USB_FEATURE_UPLOAD_PACKET_SIZE],
    uint8_t length, uint8_t usb_packet[USB_FEATURE_UPLOAD_PACKET_SIZE]) {
    UsbMotorVendorServiceResult result =
        usb_motor_vendor_service_accept_usb(service, request, length, usb_packet);
    if (exchange == 0 || transport == 0) {
        result.mailbox_event = MOTOR_COMMAND_MAILBOX_EXCHANGE_FAILED;
        return result;
    }
    if ((result.actions & USB_MOTOR_VENDOR_ACTION_CLAIM) != 0) {
        command_transport_claim(transport, MOTOR_COMMAND_MAILBOX_OWNER);
    }
    if ((result.actions & USB_MOTOR_VENDOR_ACTION_RESTART) != 0) {
        motor_command_mailbox_exchange_reset(exchange);
    }
    if ((result.actions & USB_MOTOR_VENDOR_ACTION_RELEASE) != 0) {
        motor_command_mailbox_exchange_reset(exchange);
        command_transport_release(transport, MOTOR_COMMAND_MAILBOX_OWNER);
        return result;
    }
    if ((result.actions & USB_MOTOR_VENDOR_ACTION_WRITE_MOTOR) != 0 &&
        !motor_command_mailbox_exchange_queue(exchange, result.motor_packet,
                                              result.motor_packet_length)) {
        result.mailbox_event = MOTOR_COMMAND_MAILBOX_EXCHANGE_FAILED;
    }
    return result;
}

/**
 * @brief Accepts one packet from the motor command channel.
 *
 * Applies sequence and fragment handling, acknowledges accepted payloads, retries invalid packets,
 * rebuilds retained commands for peer retry requests, applies local responses, and publishes
 * forwardable motor responses for USB download.
 *
 * @param[in,out] service Active motor vendor service.
 * @param[in] packet Received motor command packet.
 * @param[in] length Received packet byte count.
 * @return Motor write and USB response-ready actions produced by the packet.
 */
UsbMotorVendorServiceResult usb_motor_vendor_service_accept_motor(UsbMotorVendorService *service,
                                                                  const uint8_t *packet,
                                                                  uint16_t length) {
    UsbMotorVendorServiceResult result = {0};
    if (service == 0) {
        return result;
    }
    MotorCommandReceiveEvent receive =
        motor_command_receiver_accept(&service->receiver, packet, length);
    if (receive.result == MOTOR_COMMAND_RECEIVE_ACKNOWLEDGED) {
        service->command_pending = false;
        service->pending_payload_length = 0;
        return result;
    }
    if (receive.result == MOTOR_COMMAND_RECEIVE_RESEND ||
        receive.result == MOTOR_COMMAND_RECEIVE_RETRY) {
        if (rebuild_command(service)) {
            result.actions = USB_MOTOR_VENDOR_ACTION_WRITE_MOTOR;
            result.motor_packet = service->buffers.motor_transmit;
            result.motor_packet_length = service->motor_transmit_length;
        }
        return result;
    }
    if (receive.result == MOTOR_COMMAND_RECEIVE_RESET) {
        reset_protocol(service);
        return result;
    }
    if (receive.result == MOTOR_COMMAND_RECEIVE_INVALID) {
        if (packet != 0 && length >= MOTOR_COMMAND_PACKET_CONTROL_PACKET_SIZE &&
            build_control(service, true)) {
            result.actions = USB_MOTOR_VENDOR_ACTION_WRITE_MOTOR;
            result.motor_packet = service->buffers.motor_transmit;
            result.motor_packet_length = service->motor_transmit_length;
        }
        return result;
    }
    if (receive.result != MOTOR_COMMAND_RECEIVE_FRAGMENT_WAITING &&
        receive.result != MOTOR_COMMAND_RECEIVE_MESSAGE &&
        receive.result != MOTOR_COMMAND_RECEIVE_IGNORED) {
        return result;
    }

    service->command_pending = false;
    service->pending_payload_length = 0;
    if (build_control(service, false)) {
        result.actions = USB_MOTOR_VENDOR_ACTION_WRITE_MOTOR;
        result.motor_packet = service->buffers.motor_transmit;
        result.motor_packet_length = service->motor_transmit_length;
    }
    if (receive.result != MOTOR_COMMAND_RECEIVE_MESSAGE) {
        return result;
    }

    if (!motor_command_message_decode(receive.payload, receive.payload_length, &service->message)) {
        return result;
    }
    MotorCommandApplicationEvent application =
        motor_command_application_apply(&service->application, &service->message);
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

/**
 * @brief Advances the USB motor channel over the command mailbox.
 *
 * Polls only while owner 0x20 holds the transport, passes received mailbox packets through the
 * motor command protocol, and queues any acknowledgement, retry, or rebuilt command it produces.
 *
 * @param[in,out] service Active motor vendor service.
 * @param[in,out] exchange Motor-command mailbox exchange.
 * @param[in,out] transport Shared command transport.
 * @return USB, motor-write, mailbox, and status events produced by this service call.
 */
UsbMotorVendorServiceResult
usb_motor_vendor_service_run_mailbox(UsbMotorVendorService *service,
                                     MotorCommandMailboxExchange *exchange,
                                     CommandTransport *transport) {
    UsbMotorVendorServiceResult result = {0};
    if (service == 0 || exchange == 0 || transport == 0 ||
        !command_transport_is_owner(transport, MOTOR_COMMAND_MAILBOX_OWNER)) {
        return result;
    }

    MotorCommandMailboxExchangeResult mailbox =
        motor_command_mailbox_exchange_run(exchange, transport);
    if (mailbox.event == MOTOR_COMMAND_MAILBOX_EXCHANGE_PACKET_READ) {
        result =
            usb_motor_vendor_service_accept_motor(service, mailbox.packet, mailbox.packet_length);
        if ((result.actions & USB_MOTOR_VENDOR_ACTION_WRITE_MOTOR) != 0 &&
            !motor_command_mailbox_exchange_queue(exchange, result.motor_packet,
                                                  result.motor_packet_length)) {
            mailbox.event = MOTOR_COMMAND_MAILBOX_EXCHANGE_FAILED;
        }
    }
    result.mailbox_event = mailbox.event;
    result.motor_status = mailbox.status;
    return result;
}

/**
 * @brief Prepares the next USB packet without advancing response progress.
 *
 * Restores the download cursor after encoding so endpoint backpressure or a higher-priority vendor
 * response can discard the packet without changing acknowledgement progress.
 *
 * @param[in] service Active motor vendor service.
 * @param[out] packet Destination for the candidate USB response packet.
 * @return Number of response bytes prepared, or zero while waiting or when no response is active.
 */
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

/**
 * @brief Builds the next USB packet for a completed motor response.
 *
 * Advances compact or segmented report 6 framing and releases the retained application response
 * after the terminal packet is emitted.
 *
 * @param[in,out] service Active motor vendor service.
 * @param[out] packet Destination for the next USB response packet.
 * @return Number of response bytes produced, or zero while waiting or when no response is active.
 */
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

/**
 * @brief Accepts USB progress acknowledgement for the active motor response.
 *
 * Matches report, progress, and remaining length against the current report 6 download boundary.
 *
 * @param[in,out] service Active motor vendor service.
 * @param[in] acknowledgement Thirteen-byte USB transfer acknowledgement.
 * @return True when the acknowledgement advances the active response download.
 */
bool usb_motor_vendor_service_acknowledge_response(
    UsbMotorVendorService *service,
    const uint8_t acknowledgement[USB_MOTOR_RESPONSE_ACKNOWLEDGEMENT_SIZE]) {
    return service != 0 && service->response_active &&
           usb_motor_response_download_acknowledge(&service->download, acknowledgement);
}
