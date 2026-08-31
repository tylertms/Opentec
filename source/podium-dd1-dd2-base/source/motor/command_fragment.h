#ifndef OPENTEC_BASE_MOTOR_COMMAND_FRAGMENT_H
#define OPENTEC_BASE_MOTOR_COMMAND_FRAGMENT_H

#include <stdint.h>

/** @brief Limits for motor-command fragment assembly. */
enum {
    MOTOR_COMMAND_FRAGMENT_MAX_SIZE = 1009, /**< Maximum packet size defined by the motor command protocol. */
};

/** @brief Reports the result of accepting a motor-command fragment. */
typedef enum {
    MOTOR_COMMAND_FRAGMENT_INVALID, /**< The packet or fragment sequence is invalid. */
    MOTOR_COMMAND_FRAGMENT_WAITING, /**< More fragments are required before a complete message is available. */
    MOTOR_COMMAND_FRAGMENT_COMPLETE, /**< The final fragment completed the assembled message. */
} MotorCommandFragmentResult;

/** @brief Stores state for assembling a fragmented motor-command packet. */
typedef struct {
    uint8_t *data; /**< Caller-owned assembly buffer containing the packet without its final checksum. */
    uint16_t capacity; /**< Capacity of data in bytes. */
    uint16_t length; /**< Number of assembled bytes currently stored in data. */
    uint16_t content_length; /**< Assembled body length, excluding the three-byte packet envelope. */
} MotorCommandFragment;

/**
 * @brief Initializes a motor-command fragment assembler.
 *
 * Attaches the caller-owned assembly buffer and clears the assembled and completed lengths.
 *
 * @param[out] fragment Fragment assembler state to initialize.
 * @param[in] data Caller-owned storage for assembled packet bytes.
 * @param[in] capacity Capacity of data in bytes.
 */
void motor_command_fragment_init(MotorCommandFragment *fragment, uint8_t *data, uint16_t capacity);

/**
 * @brief Accepts one motor-command packet fragment.
 *
 * Validates the packet checksum and fragment envelope, starts or extends the assembly, and reports
 * when the final fragment completes the assembled packet in data.
 *
 * @param[in,out] fragment Fragment assembler and assembled packet storage.
 * @param[in] packet Candidate fragment packet.
 * @param[in] length Number of bytes in packet.
 * @return Fragment progress or invalid status.
 */
MotorCommandFragmentResult motor_command_fragment_accept(MotorCommandFragment *fragment,
                                                         const uint8_t *packet, uint16_t length);

#endif
