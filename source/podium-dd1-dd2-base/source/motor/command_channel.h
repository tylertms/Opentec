#ifndef OPENTEC_BASE_MOTOR_COMMAND_CHANNEL_H
#define OPENTEC_BASE_MOTOR_COMMAND_CHANNEL_H

#include <stdbool.h>
#include <stdint.h>

#include "motor/command_application.h"
#include "motor/command_message.h"
#include "motor/command_receiver.h"
#include "motor/command_scheduler.h"

/** @brief Actions requested by a motor-command channel event. */
typedef enum {
    MOTOR_COMMAND_CHANNEL_ACTION_NONE = 0, /**< No packet must be written. */
    MOTOR_COMMAND_CHANNEL_ACTION_WRITE =
        1 << 0, /**< Write the packet referenced by the event to the motor. */
} MotorCommandChannelAction;

/** @brief Caller-owned storage used by a motor-command channel. */
typedef struct {
    uint8_t *receive_assembly;          /**< Buffer used to assemble fragmented received packets. */
    uint16_t receive_assembly_capacity; /**< Capacity of receive_assembly in bytes. */
    uint8_t *transmit;                  /**< Buffer used for the current encoded transmit packet. */
    uint16_t transmit_capacity;         /**< Capacity of transmit in bytes. */
    uint8_t *pending_payload; /**< Buffer retaining the application payload for retransmission. */
    uint16_t pending_payload_capacity; /**< Capacity of pending_payload in bytes. */
} MotorCommandChannelBuffers;

/** @brief Reports the result of accepting one motor-command packet. */
typedef struct {
    MotorCommandChannelAction actions;        /**< Packet-channel actions required by the caller. */
    MotorCommandReceiveResult receive_result; /**< Receive-layer result for the accepted packet. */
    MotorCommandApplicationEvent
        application; /**< Application-layer result for a complete message, when applicable. */
    const uint8_t
        *packet; /**< Encoded control or retransmission packet to write when WRITE is set. */
    uint16_t packet_length; /**< Number of bytes available at packet. */
} MotorCommandChannelEvent;

/** @brief Maintains motor-command receive, transmit, and application state. */
typedef struct {
    MotorCommandReceiver
        receiver; /**< Sequence and fragment state for received and sent packets. */
    MotorCommandApplication application; /**< Accumulated decoded application state. */
    MotorCommandMessage message;         /**< View of the most recently decoded complete message. */
    MotorCommandChannelBuffers buffers;  /**< Caller-owned buffers attached to this channel. */
    MotorCommandScheduler scheduler;     /**< Timeout state for the active outbound command. */
    uint16_t transmit_length; /**< Length of the encoded packet currently in buffers.transmit. */
    uint16_t pending_payload_length; /**< Length of the retained application payload. */
    uint8_t control_packet[5];       /**< Retained control packet awaiting transmission. */
    uint8_t retry_count;             /**< Number of peer reset requests for the active command. */
    bool command_pending; /**< Whether a queued application payload awaits acknowledgement. */
    bool command_sent;    /**< True after the active command has reached the physical link. */
    bool reset_pending;   /**< True while the sequence-reset packet awaits transmission. */
} MotorCommandChannel;

/**
 * @brief Initializes a motor-command protocol channel.
 *
 * Attaches caller-owned receive, transmit, and retransmission buffers, clears application state,
 * and resets sequence and pending-command progress.
 *
 * @param[out] channel Channel state to initialize.
 * @param[in] buffers Caller-owned storage and capacities for the channel.
 * @return true when all required buffers are present and usable; otherwise false.
 */
bool motor_command_channel_init(MotorCommandChannel *channel,
                                const MotorCommandChannelBuffers *buffers);

/**
 * @brief Resets motor-command protocol progress.
 *
 * Restarts receive assembly and sequence tracking and discards a retained request while keeping
 * attached buffers and accumulated application state.
 *
 * @param[in,out] channel Channel state to reset.
 */
void motor_command_channel_reset(MotorCommandChannel *channel);

/**
 * @brief Queues an application payload for transmission.
 *
 * Copies the payload into retained storage, encodes the initial packet, and advances the transmit
 * sequence so the request can be rebuilt if the peer asks for a resend.
 *
 * @param[in,out] channel Channel receiving the request.
 * @param[in] payload Application payload bytes to copy.
 * @param[in] payload_length Number of payload bytes.
 * @return true when the payload was queued; otherwise false when the channel is busy or storage is
 *         insufficient.
 */
bool motor_command_channel_queue_payload(MotorCommandChannel *channel, const uint8_t *payload,
                                         uint16_t payload_length);

/**
 * @brief Queues a protocol sequence-reset packet.
 *
 * Encodes a five-byte reset control packet when no application request is pending.
 *
 * @param[in,out] channel Channel whose transmit buffer receives the reset packet.
 * @return true when the reset packet was queued; otherwise false.
 */
bool motor_command_channel_queue_sequence_reset(MotorCommandChannel *channel);

/**
 * @brief Builds a retry control event for the current receive sequence.
 *
 * The event is independent of the retained outbound command, so a lower-layer payload-read
 * refusal can request the same peer packet again without consuming the command retry budget.
 *
 * @param[in,out] channel Channel whose control storage receives the retry packet.
 * @return Retry write event, or an empty event when channel is null.
 */
MotorCommandChannelEvent motor_command_channel_queue_retry(MotorCommandChannel *channel);

/**
 * @brief Requeues the retained application command.
 *
 * Marks the retained command unsent and rebuilds it with its already reserved transmit sequence.
 *
 * @param[in,out] channel Channel retaining the command.
 * @return true when the retained command was rebuilt; otherwise false.
 */
bool motor_command_channel_requeue_pending(MotorCommandChannel *channel);

/**
 * @brief Queues the protocol recovery control packet.
 *
 * Sends the official five-byte sequence-reset control packet for the 0xFE recovery action while
 * retaining the outstanding application payload, sequence ownership, and command-pending state.
 * The retained application command can be rebuilt after the reset packet is written.
 *
 * @param[in,out] channel Channel retaining the stalled command.
 * @return true when the recovery frame was queued; otherwise false.
 */
bool motor_command_channel_queue_recovery_command(MotorCommandChannel *channel);

/**
 * @brief Records that a channel packet was written to the physical transport.
 *
 * Completes a sequence-reset control write before rebuilding any retained application request.
 * This keeps the reset packet stable through its transport exchange and makes the retained request
 * available to the next mailbox exchange.
 *
 * @param[in,out] channel Motor command channel retaining acknowledgement state.
 * @param[in] packet Packet accepted by the physical transport.
 */
void motor_command_channel_mark_written(MotorCommandChannel *channel, const uint8_t *packet);

/**
 * @brief Queues a calibration-digest request.
 *
 * Builds the command-7 request and retains it through the normal application request path for
 * acknowledgement and retransmission handling.
 *
 * @param[in,out] channel Channel receiving the digest request.
 * @return true when the request was queued; otherwise false.
 */
bool motor_command_channel_queue_digest_request(MotorCommandChannel *channel);

/**
 * @brief Queues an information-selector request.
 *
 * Builds a command-5 request for a two-byte response from selector 3 or 4 and retains it through
 * the normal application request path.
 *
 * @param[in,out] channel Channel receiving the information request.
 * @param[in] selector Information selector; only 3 and 4 are supported by this helper.
 * @return true when the selector is supported and the request was queued; otherwise false.
 */
bool motor_command_channel_queue_information_request(MotorCommandChannel *channel,
                                                     uint8_t selector);

/**
 * @brief Accepts one motor-command protocol packet.
 *
 * Applies sequence and fragment handling, updates application state for complete valid messages,
 * and prepares acknowledgements, retries, or retained-request retransmissions for the caller.
 *
 * @param[in,out] channel Channel state to advance.
 * @param[in] packet Received packet bytes.
 * @param[in] length Number of received packet bytes.
 * @return Receive and application results plus an optional packet-write action.
 */
MotorCommandChannelEvent motor_command_channel_accept(MotorCommandChannel *channel,
                                                      const uint8_t *packet, uint16_t length);

/**
 * @brief Returns accumulated motor-command application state.
 *
 * Provides read-only access to information-selector values and the calibration digest accumulated
 * by the channel.
 *
 * @param[in] channel Channel state to inspect.
 * @return Pointer to the channel's application state, or null when channel is null.
 */
const MotorCommandApplication *
motor_command_channel_application(const MotorCommandChannel *channel);

#endif
