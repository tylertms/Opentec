#ifndef OPENTEC_BASE_MOTOR_COMMAND_RECEIVER_H
#define OPENTEC_BASE_MOTOR_COMMAND_RECEIVER_H

#include <stdint.h>

#include "motor/command_fragment.h"
#include "motor/command_sequence.h"

/**
 * @brief Result of accepting one motor-command packet.
 */
typedef enum {
    MOTOR_COMMAND_RECEIVE_INVALID,      /**< Packet failed validation or sequence handling. */
    MOTOR_COMMAND_RECEIVE_ACKNOWLEDGED, /**< Packet acknowledged the pending transmission. */
    MOTOR_COMMAND_RECEIVE_RESEND, /**< Packet requested retransmission of the expected sequence. */
    MOTOR_COMMAND_RECEIVE_RETRY,  /**< Packet requested retransmission of a selected sequence. */
    MOTOR_COMMAND_RECEIVE_RESET,  /**< Packet reset the command sequence state. */
    MOTOR_COMMAND_RECEIVE_FRAGMENT_WAITING, /**< Valid fragment was accepted but the message is
                                               incomplete. */
    MOTOR_COMMAND_RECEIVE_MESSAGE,          /**< Complete command payload is available. */
    MOTOR_COMMAND_RECEIVE_IGNORED, /**< Valid packet with a non-reserved unsupported fragment marker
                                      was ignored. */
} MotorCommandReceiveResult;

/**
 * @brief Result and payload view produced by the receiver.
 */
typedef struct {
    MotorCommandReceiveResult result; /**< Packet acceptance result. */
    const uint8_t *payload;  /**< Complete payload bytes, or null when no message completed. */
    uint16_t payload_length; /**< Number of complete payload bytes, or zero when unavailable. */
} MotorCommandReceiveEvent;

/**
 * @brief Stateful motor-command packet receiver.
 */
typedef struct {
    MotorCommandSequence sequence; /**< Transmit and receive sequence tracking. */
    MotorCommandFragment fragment; /**< Fragment assembly state and storage view. */
} MotorCommandReceiver;

/**
 * @brief Initializes a motor-command receiver.
 *
 * Resets sequence tracking and attaches the caller-owned storage used for fragmented messages. The
 * receiver must be non-null, and the assembly storage must remain valid for the receiver's
 * lifetime when fragmented messages are expected.
 *
 * @param[out] receiver Receiver state to initialize.
 * @param[in] assembly Caller-owned storage for fragmented message assembly.
 * @param[in] assembly_capacity Capacity of assembly in bytes.
 */
void motor_command_receiver_init(MotorCommandReceiver *receiver, uint8_t *assembly,
                                 uint16_t assembly_capacity);

/**
 * @brief Accepts one motor-command packet.
 *
 * Validates packet checksum and declared length, classifies control or payload sequence state, and
 * assembles supported fragments. A complete event points into the input packet for an unfragmented
 * message or into the receiver's assembly storage for a fragmented message. The receiver and
 * packet may be null; such input produces MOTOR_COMMAND_RECEIVE_INVALID.
 *
 * @param[in,out] receiver Receiver state to update.
 * @param[in] packet Received packet bytes.
 * @param[in] length Number of bytes in packet.
 * @return Acceptance result and, for a complete message, its payload view.
 */
MotorCommandReceiveEvent motor_command_receiver_accept(MotorCommandReceiver *receiver,
                                                       const uint8_t *packet, uint16_t length);

#endif
