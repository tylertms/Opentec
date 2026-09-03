#ifndef OPENTEC_BASE_MOTOR_COMMAND_INFORMATION_H
#define OPENTEC_BASE_MOTOR_COMMAND_INFORMATION_H

#include <stdint.h>

#include "motor/command_message.h"

/** @brief Describes how an information response was applied. */
typedef enum {
    MOTOR_COMMAND_INFORMATION_INVALID, /**< The response could not be applied. */
    MOTOR_COMMAND_INFORMATION_STORED, /**< The response was accepted by the information handler. */
    MOTOR_COMMAND_INFORMATION_FORWARD, /**< Selector-2 data should be forwarded to the caller. */
} MotorCommandInformationResult;

/** @brief Stores information-selector responses with functional application consumers. */
typedef struct {
    uint16_t selector_1; /**< Two-byte response for information selector 1. */
    uint16_t selector_3; /**< Two-byte response for information selector 3. */
    uint16_t selector_4; /**< Two-byte response for information selector 4. */
    uint8_t selector_5; /**< One-byte response for information selector 5. */
    uint8_t selector_7[16]; /**< Sixteen-byte response for information selector 7. */
    uint8_t selector_8[4]; /**< Four-byte response for information selector 8. */
    uint8_t selector_9[50]; /**< Fifty-byte response for information selector 9. */
} MotorCommandInformation;

/**
 * @brief Applies one motor-command information response.
 *
 * Validates the message kind and selector-specific data length, stores selectors 1 and 3 through
 * 5 and 7 through 9, and reports selector 2 for forwarding when its data length is 20 bytes.
 * Selector 6 is accepted when its data length is two bytes for protocol compatibility, but it has
 * no retained application state.
 *
 * @param[in,out] state Information state receiving a retained response when applicable.
 * @param[in] message Decoded information message to apply.
 * @return MOTOR_COMMAND_INFORMATION_STORED, MOTOR_COMMAND_INFORMATION_FORWARD, or
 *         MOTOR_COMMAND_INFORMATION_INVALID.
 */
MotorCommandInformationResult motor_command_information_apply(MotorCommandInformation *state,
                                                              const MotorCommandMessage *message);

#endif
