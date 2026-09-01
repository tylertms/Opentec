#ifndef OPENTEC_BASE_MOTOR_IDENTITY_H
#define OPENTEC_BASE_MOTOR_IDENTITY_H

#include <stdbool.h>
#include <stdint.h>

/**
 * @brief Motor-controller protocol family decoded from initial status.
 */
typedef enum {
    MOTOR_PROTOCOL_LEGACY,   /**< Legacy controller without standard command registers. */
    MOTOR_PROTOCOL_STANDARD, /**< Standard controller with the base command protocol. */
    MOTOR_PROTOCOL_POSITION, /**< Position-capable controller with extended parameters. */
} MotorProtocol;

/**
 * @brief Decoded motor-controller identity.
 */
typedef struct {
    MotorProtocol protocol; /**< Decoded protocol family; valid only after successful decoding. */
    uint8_t version[4];     /**< Four-byte version response copied from the controller. */
    uint8_t model;          /**< Five-bit model identifier from the initial status byte. */
    uint8_t transfer_code;  /**< Six-bit transfer identifier from version byte zero. */
    uint8_t initial_status; /**< Initial status byte used for protocol and model decoding. */
} MotorIdentity;

/**
 * @brief Decodes a motor-controller identity response.
 *
 * Copies the status and four-byte version response, extracts model and transfer fields, and
 * classifies the controller. A clear status high bit selects legacy; otherwise status bits zero
 * and one select standard for zero, position for one or two, and an invalid encoding for three.
 * The output protocol is valid only when the function returns true.
 *
 * @param[in] status Initial controller status byte.
 * @param[in] version Four-byte controller version response.
 * @param[out] identity Identity storage to populate.
 * @return True when status encodes legacy, standard, or position protocol; false for a reserved
 * protocol encoding.
 */
bool motor_identity_decode(uint8_t status, const uint8_t version[4], MotorIdentity *identity);

/**
 * @brief Tests whether an identity supports extended motor parameters.
 *
 * Reports true only for the position-capable protocol, which exposes the extended parameter
 * exchange used by telemetry and status services. The identity pointer must be non-null.
 *
 * @param[in] identity Decoded motor-controller identity.
 * @return True for the position-capable protocol; otherwise false.
 */
bool motor_identity_has_extended_parameters(const MotorIdentity *identity);

/**
 * @brief Selects the motor transfer identifier for input reports.
 *
 * Returns the decoded six-bit transfer code for standard and position-capable controllers. Legacy
 * controllers and unavailable identities map to zero. A null identity is accepted.
 *
 * @param[in] identity Decoded motor-controller identity, or null when unavailable.
 * @return Input-report transfer identifier.
 */
uint8_t motor_identity_input_transfer_code(const MotorIdentity *identity);

/**
 * @brief Selects the runtime state code for a motor-controller identity.
 *
 * Maps an unavailable identity to zero and maps legacy, standard, and position protocols to one,
 * two, and three respectively. A null identity is accepted.
 *
 * @param[in] identity Decoded motor-controller identity, or null when unavailable.
 * @return Runtime motor-controller state code.
 */
uint8_t motor_identity_runtime_state(const MotorIdentity *identity);

/**
 * @brief Selects the signed-position modulus for a motor controller.
 *
 * Uses modulus 0x5c7f for position-capable models with model bit one clear. Position-capable
 * models with that bit set, standard and legacy controllers, and unavailable identities use
 * modulus 0x5d2b. A null identity is accepted.
 *
 * @param[in] identity Decoded motor-controller identity, or null when unavailable.
 * @return Modulus for signed wheel-position normalization.
 */
uint32_t motor_identity_position_modulus(const MotorIdentity *identity);

#endif
