#ifndef OPENTEC_BASE_MOTOR_COMMAND_SEQUENCE_H
#define OPENTEC_BASE_MOTOR_COMMAND_SEQUENCE_H

#include <stdint.h>

/**
 * @brief Classification of a received motor-command header.
 */
typedef enum {
    MOTOR_COMMAND_SEQUENCE_INVALID, /**< Header is malformed or has an unexpected adjacent sequence.
                                     */
    MOTOR_COMMAND_SEQUENCE_ACKNOWLEDGED, /**< Header acknowledges the current transmitted sequence.
                                          */
    MOTOR_COMMAND_SEQUENCE_PAYLOAD,      /**< Header carries the expected peer payload sequence. */
    MOTOR_COMMAND_SEQUENCE_RESEND, /**< Mismatched acknowledgement requests the expected sequence.
                                    */
    MOTOR_COMMAND_SEQUENCE_RESET,  /**< Header requests a complete sequence reset. */
    MOTOR_COMMAND_SEQUENCE_RETRY,  /**< Header requests the selected transmitted sequence again. */
} MotorCommandSequenceEvent;

/**
 * @brief Two-bit motor-command transmit and receive sequence state.
 */
typedef struct {
    uint8_t transmit;         /**< Sequence number used by the next locally transmitted payload. */
    uint8_t receive_previous; /**< Most recently accepted peer payload sequence number. */
    uint8_t receive_next;     /**< Sequence number expected in the next peer payload. */
} MotorCommandSequence;

/**
 * @brief Initializes motor-command sequence state.
 *
 * Sets the local transmit sequence to zero, the previous peer sequence to three, and the next
 * expected peer sequence to zero. The sequence pointer must be non-null.
 *
 * @param[out] sequence Sequence state to initialize.
 */
void motor_command_sequence_init(MotorCommandSequence *sequence);

/**
 * @brief Advances the local motor-command transmit sequence.
 *
 * Increments the two-bit sequence and wraps from three to zero for the next outbound payload. The
 * sequence pointer must be non-null.
 *
 * @param[in,out] sequence Sequence state to update.
 */
void motor_command_sequence_advance(MotorCommandSequence *sequence);

/**
 * @brief Classifies one received motor-command header.
 *
 * Compares the header's adjacent sequence with the sequence preceding the current transmission.
 * Retry and mismatched-acknowledgement headers update the local transmit sequence, while reset
 * headers reinitialize all sequence state. The sequence pointer must be non-null.
 *
 * @param[in,out] sequence Sequence state updated by retry, resend, or reset headers.
 * @param[in] header Received packet header byte.
 * @return Header classification.
 */
MotorCommandSequenceEvent motor_command_sequence_receive_header(MotorCommandSequence *sequence,
                                                                uint8_t header);

/**
 * @brief Accepts a peer payload sequence.
 *
 * Stores the peer sequence carried in header bits two and three and derives its wrapped successor
 * for the next expected peer payload. The sequence pointer must be non-null.
 *
 * @param[in,out] sequence Sequence state to update.
 * @param[in] header Header from the accepted peer payload packet.
 */
void motor_command_sequence_accept_payload(MotorCommandSequence *sequence, uint8_t header);

#endif
