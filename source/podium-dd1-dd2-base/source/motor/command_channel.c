#include "motor/command_channel.h"

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "motor/command_application.h"
#include "motor/command_message.h"
#include "motor/command_packet.h"
#include "motor/command_receiver.h"
#include "motor/command_sequence.h"

enum {
    MOTOR_COMMAND_CHANNEL_REQUEST_SIZE = 5,
    MOTOR_COMMAND_CHANNEL_DIGEST_COMMAND = 7,
    MOTOR_COMMAND_CHANNEL_INFORMATION_COMMAND = 5,
    MOTOR_COMMAND_CHANNEL_DIGEST_LENGTH = 20,
    MOTOR_COMMAND_CHANNEL_INFORMATION_WORD_LENGTH = 2,
};

/**
 * @brief Publishes the channel's current transmit packet.
 *
 * Returns a write event that refers to the caller-owned transmit storage attached during channel
 * initialization.
 *
 * @param[in] channel Active motor-command channel.
 * @return Motor write event for the current packet.
 */
static MotorCommandChannelEvent write_event(const MotorCommandChannel *channel) {
    return (MotorCommandChannelEvent){
        .actions = MOTOR_COMMAND_CHANNEL_ACTION_WRITE,
        .packet = channel->buffers.transmit,
        .packet_length = channel->transmit_length,
    };
}

/**
 * @brief Rebuilds the retained application request.
 *
 * Frames the pending payload with the current transmit and adjacent receive sequences without
 * advancing either sequence.
 *
 * @param[in,out] channel Active motor-command channel.
 * @return True when a retained request was encoded.
 */
static bool rebuild_payload(MotorCommandChannel *channel) {
    return channel->command_pending &&
           motor_command_packet_payload_encode(
               0, channel->receiver.sequence.transmit, channel->receiver.sequence.receive_previous,
               channel->buffers.pending_payload, channel->pending_payload_length,
               channel->buffers.transmit, channel->buffers.transmit_capacity,
               &channel->transmit_length);
}

/**
 * @brief Builds an acknowledgement or retry control packet.
 *
 * Selects the accepted previous receive sequence for acknowledgement and the expected next receive
 * sequence for retry.
 *
 * @param[in,out] channel Active motor-command channel.
 * @param[in] retry Selects retry control when true.
 * @return True when the transmit storage can hold the control packet.
 */
static bool build_control(MotorCommandChannel *channel, bool retry) {
    if (channel->buffers.transmit_capacity < MOTOR_COMMAND_PACKET_CONTROL_PACKET_SIZE) {
        return false;
    }
    if (retry) {
        motor_command_packet_retry_encode(channel->receiver.sequence.receive_next,
                                          channel->buffers.transmit);
    } else {
        motor_command_packet_acknowledgement_encode(channel->receiver.sequence.receive_previous,
                                                    channel->buffers.transmit);
    }
    channel->transmit_length = MOTOR_COMMAND_PACKET_CONTROL_PACKET_SIZE;
    return true;
}

/**
 * @brief Initializes a motor-command protocol channel.
 *
 * Attaches receive assembly, transmit, and retained-request storage, clears application state, and
 * starts both sequence counters at zero.
 *
 * @param[out] channel Channel to initialize.
 * @param[in] buffers Caller-owned protocol storage.
 * @return True when all required storage is present and large enough for control packets.
 */
bool motor_command_channel_init(MotorCommandChannel *channel,
                                const MotorCommandChannelBuffers *buffers) {
    if (channel == 0 || buffers == 0 || buffers->receive_assembly == 0 ||
        buffers->receive_assembly_capacity == 0 || buffers->transmit == 0 ||
        buffers->transmit_capacity < MOTOR_COMMAND_PACKET_CONTROL_PACKET_SIZE ||
        buffers->pending_payload == 0 || buffers->pending_payload_capacity == 0) {
        return false;
    }
    *channel = (MotorCommandChannel){.buffers = *buffers};
    motor_command_application_init(&channel->application);
    motor_command_channel_reset(channel);
    return true;
}

/**
 * @brief Resets motor-command protocol progress.
 *
 * Restarts receive assembly and sequence tracking and discards any retained request while keeping
 * accumulated application information and attached storage.
 *
 * @param[in,out] channel Channel to reset.
 */
void motor_command_channel_reset(MotorCommandChannel *channel) {
    motor_command_receiver_init(&channel->receiver, channel->buffers.receive_assembly,
                                channel->buffers.receive_assembly_capacity);
    channel->transmit_length = 0;
    channel->pending_payload_length = 0;
    channel->command_pending = false;
}

/**
 * @brief Queues an application request on the motor-command channel.
 *
 * Retains the payload for resend handling, advances the transmit sequence, and frames the request
 * against the most recently accepted receive sequence.
 *
 * @param[in,out] channel Active motor-command channel.
 * @param[in] payload Application request bytes.
 * @param[in] payload_length Application request byte count.
 * @return True when no request is pending and the payload fits the attached storage.
 */
bool motor_command_channel_queue_payload(MotorCommandChannel *channel, const uint8_t *payload,
                                         uint16_t payload_length) {
    if (channel == 0 || payload == 0 || channel->command_pending ||
        payload_length > channel->buffers.pending_payload_capacity ||
        payload_length + MOTOR_COMMAND_PACKET_ENCODING_OVERHEAD >
            channel->buffers.transmit_capacity) {
        return false;
    }
    memmove(channel->buffers.pending_payload, payload, payload_length);
    motor_command_sequence_advance(&channel->receiver.sequence);
    if (!motor_command_packet_payload_encode(
            0, channel->receiver.sequence.transmit, channel->receiver.sequence.receive_previous,
            channel->buffers.pending_payload, payload_length, channel->buffers.transmit,
            channel->buffers.transmit_capacity, &channel->transmit_length)) {
        return false;
    }
    channel->pending_payload_length = payload_length;
    channel->command_pending = true;
    return true;
}

/**
 * @brief Queues a motor-command sequence reset.
 *
 * Builds the five-byte reset control packet without retaining an application request.
 *
 * @param[in,out] channel Active motor-command channel.
 * @return True when the channel is idle and its transmit storage holds a control packet.
 */
bool motor_command_channel_queue_sequence_reset(MotorCommandChannel *channel) {
    if (channel == 0 || channel->command_pending ||
        channel->buffers.transmit_capacity < MOTOR_COMMAND_PACKET_CONTROL_PACKET_SIZE) {
        return false;
    }
    motor_command_packet_sequence_reset_encode(channel->buffers.transmit);
    channel->transmit_length = MOTOR_COMMAND_PACKET_CONTROL_PACKET_SIZE;
    return true;
}

/**
 * @brief Queues the motor calibration digest request.
 *
 * Requests command 7 with its fixed twenty-byte calibration response and uses the channel's normal
 * application-request sequencing and resend retention.
 *
 * @param[in,out] channel Active motor-command channel.
 * @return True when the request was queued.
 */
bool motor_command_channel_queue_digest_request(MotorCommandChannel *channel) {
    const uint8_t payload[MOTOR_COMMAND_CHANNEL_REQUEST_SIZE] = {
        MOTOR_COMMAND_CHANNEL_DIGEST_COMMAND, 0, 0, 0, MOTOR_COMMAND_CHANNEL_DIGEST_LENGTH,
    };
    return motor_command_channel_queue_payload(channel, payload, sizeof(payload));
}

/**
 * @brief Queues a two-byte motor information request.
 *
 * Requests command 5 for selector 3 or 4 and uses the channel's normal application-request
 * sequencing and resend retention.
 *
 * @param[in,out] channel Active motor-command channel.
 * @param[in] selector Information selector 3 or 4.
 * @return True when the selector is supported and the request was queued.
 */
bool motor_command_channel_queue_information_request(MotorCommandChannel *channel,
                                                     uint8_t selector) {
    if (selector != 3 && selector != 4) {
        return false;
    }
    const uint8_t payload[MOTOR_COMMAND_CHANNEL_REQUEST_SIZE] = {
        MOTOR_COMMAND_CHANNEL_INFORMATION_COMMAND,     0, selector, 0,
        MOTOR_COMMAND_CHANNEL_INFORMATION_WORD_LENGTH,
    };
    return motor_command_channel_queue_payload(channel, payload, sizeof(payload));
}

/**
 * @brief Accepts one motor-command protocol packet.
 *
 * Applies sequence and fragment handling, rebuilds retained requests for resend or retry, emits
 * acknowledgement or retry control, and routes complete application messages into accumulated
 * information and digest state.
 *
 * @param[in,out] channel Active motor-command channel.
 * @param[in] packet Received motor-command packet.
 * @param[in] length Received packet byte count.
 * @return Receive result, optional application event, and any packet that must be written.
 */
MotorCommandChannelEvent motor_command_channel_accept(MotorCommandChannel *channel,
                                                      const uint8_t *packet, uint16_t length) {
    MotorCommandChannelEvent event = {0};
    if (channel == 0) {
        return event;
    }

    MotorCommandReceiveEvent receive =
        motor_command_receiver_accept(&channel->receiver, packet, length);
    event.receive_result = receive.result;
    if (receive.result == MOTOR_COMMAND_RECEIVE_ACKNOWLEDGED) {
        channel->command_pending = false;
        channel->pending_payload_length = 0;
        return event;
    }
    if (receive.result == MOTOR_COMMAND_RECEIVE_RESEND ||
        receive.result == MOTOR_COMMAND_RECEIVE_RETRY) {
        return rebuild_payload(channel) ? write_event(channel) : event;
    }
    if (receive.result == MOTOR_COMMAND_RECEIVE_RESET) {
        motor_command_channel_reset(channel);
        event.receive_result = MOTOR_COMMAND_RECEIVE_RESET;
        return event;
    }
    if (receive.result == MOTOR_COMMAND_RECEIVE_INVALID) {
        if (packet != 0 && length >= MOTOR_COMMAND_PACKET_CONTROL_PACKET_SIZE &&
            build_control(channel, true)) {
            event = write_event(channel);
            event.receive_result = MOTOR_COMMAND_RECEIVE_INVALID;
        }
        return event;
    }
    if (receive.result != MOTOR_COMMAND_RECEIVE_FRAGMENT_WAITING &&
        receive.result != MOTOR_COMMAND_RECEIVE_MESSAGE &&
        receive.result != MOTOR_COMMAND_RECEIVE_IGNORED) {
        return event;
    }

    channel->command_pending = false;
    channel->pending_payload_length = 0;
    if (build_control(channel, false)) {
        event = write_event(channel);
        event.receive_result = receive.result;
    }
    if (receive.result == MOTOR_COMMAND_RECEIVE_MESSAGE &&
        motor_command_message_decode(receive.payload, receive.payload_length, &channel->message)) {
        event.application =
            motor_command_application_apply(&channel->application, &channel->message);
    }
    return event;
}

/**
 * @brief Returns accumulated motor-command application state.
 *
 * Exposes the current information-selector values and derived calibration digest without changing
 * channel progress.
 *
 * @param[in] channel Active motor-command channel.
 * @return Application state, or null when the channel is absent.
 */
const MotorCommandApplication *
motor_command_channel_application(const MotorCommandChannel *channel) {
    return channel == 0 ? 0 : &channel->application;
}
