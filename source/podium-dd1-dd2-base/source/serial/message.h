#ifndef OPENTEC_BASE_SERIAL_MESSAGE_H
#define OPENTEC_BASE_SERIAL_MESSAGE_H

#include <stdbool.h>
#include <stdint.h>

#include "serial/packet.h"

/** @brief Logical serial message limits and type range. */
enum {
    SERIAL_MESSAGE_MAX_SIZE = 512, /**< Maximum logical message size in bytes. */
    SERIAL_MESSAGE_FIRST_TYPE = 2, /**< First logical message type accepted by the transport. */
    SERIAL_MESSAGE_LAST_TYPE = 5,  /**< Last logical message type accepted by the transport. */
};

/** @brief Result of adding one packet to a logical message assembly. */
typedef enum {
    SERIAL_MESSAGE_INVALID_PACKET, /**< The packet cannot be accepted into the assembly. */
    SERIAL_MESSAGE_ACKNOWLEDGE,    /**< The packet was accepted and requires an acknowledgement. */
    SERIAL_MESSAGE_COMPLETE,       /**< The packet completed the logical message. */
    SERIAL_MESSAGE_OVERFLOW,       /**< The packet would exceed the logical message capacity. */
} SerialMessageResult;

/** @brief Logical serial message assembled from one or more packets. */
typedef struct {
    uint8_t data[SERIAL_MESSAGE_MAX_SIZE]; /**< Assembled logical message bytes. */
    uint16_t length;                       /**< Number of valid bytes in data. */
    uint8_t type;                          /**< Logical message type. */
} SerialMessageAssembly;

/**
 * @brief Resets a logical message assembly.
 *
 * Discards the accumulated length and type so assembly can accept a new message.
 *
 * @param[out] assembly Assembly state to reset.
 */
void serial_message_assembly_reset(SerialMessageAssembly *assembly);

/**
 * @brief Encodes one logical-message fragment.
 *
 * Selects the appropriate fragment flag, encodes the packet, and optionally reports the next
 * message offset and whether the peer must acknowledge the fragment.
 *
 * @param[in] type Logical message type from SERIAL_MESSAGE_FIRST_TYPE through
 * SERIAL_MESSAGE_LAST_TYPE.
 * @param[in] sequence Transport sequence number.
 * @param[in] message Complete logical message bytes.
 * @param[in] message_length Logical message length from one through
 * SERIAL_MESSAGE_MAX_SIZE bytes.
 * @param[in] offset Offset of the fragment within message.
 * @param[out] output Destination for the encoded packet.
 * @param[out] next_offset Destination for the offset after this fragment, or null when unused.
 * @param[out] acknowledgement_required Destination for whether this fragment requires an
 * acknowledgement, or null when unused.
 * @return True when the message, offset, and output arguments are valid; otherwise false.
 */
bool serial_message_fragment_encode(uint8_t type, uint8_t sequence, const uint8_t *message,
                                    uint16_t message_length, uint16_t offset,
                                    uint8_t output[SERIAL_PACKET_SIZE], uint16_t *next_offset,
                                    bool *acknowledgement_required);

/**
 * @brief Adds one decoded packet to a logical message assembly.
 *
 * Validates the logical type, appends payload bytes, and reports whether an acknowledgement or
 * completion follows. An over-capacity packet clears the entire assembly before returning
 * SERIAL_MESSAGE_OVERFLOW.
 *
 * @param[in,out] assembly Assembly state to update.
 * @param[in] packet Decoded packet to append.
 * @return Invalid, acknowledgement, complete, or overflow result.
 */
SerialMessageResult serial_message_accept(SerialMessageAssembly *assembly,
                                          const SerialPacket *packet);

/**
 * @brief Encodes a fragment acknowledgement packet.
 *
 * Builds a type-one packet whose payload identifies the acknowledged logical type.
 *
 * @param[in] sequence Transport sequence number.
 * @param[in] current_type Logical type being acknowledged.
 * @param[out] output Destination for the encoded packet.
 * @return True when output is non-null and the packet is encoded; otherwise false.
 */
bool serial_message_acknowledgement_encode(uint8_t sequence, uint8_t current_type,
                                           uint8_t output[SERIAL_PACKET_SIZE]);

/**
 * @brief Encodes a resynchronization packet.
 *
 * Builds a type-zero packet whose payload identifies the current logical transport type.
 *
 * @param[in] sequence Transport sequence number.
 * @param[in] current_type Current logical transport type.
 * @param[out] output Destination for the encoded packet.
 * @return True when output is non-null and the packet is encoded; otherwise false.
 */
bool serial_message_resynchronization_encode(uint8_t sequence, uint8_t current_type,
                                             uint8_t output[SERIAL_PACKET_SIZE]);

#endif
