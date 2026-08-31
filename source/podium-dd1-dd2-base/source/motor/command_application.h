#ifndef OPENTEC_BASE_MOTOR_COMMAND_APPLICATION_H
#define OPENTEC_BASE_MOTOR_COMMAND_APPLICATION_H

#include <stdint.h>

#include "motor/command_digest.h"
#include "motor/command_information.h"
#include "motor/command_message.h"

/** @brief Describes how a decoded motor-command message was applied. */
typedef enum {
    MOTOR_COMMAND_APPLICATION_INVALID, /**< The message was unsupported or malformed for application. */
    MOTOR_COMMAND_APPLICATION_INFORMATION, /**< Information-selector data was stored in application state. */
    MOTOR_COMMAND_APPLICATION_CALIBRATION, /**< A calibration response was decoded and its digest was stored. */
    MOTOR_COMMAND_APPLICATION_FORWARD, /**< Message data should be forwarded to the vendor-facing service. */
} MotorCommandApplicationResult;

/** @brief Reports the result of applying one motor-command message. */
typedef struct {
    MotorCommandApplicationResult result; /**< Disposition of the supplied message. */
    const uint8_t *forward_data; /**< View of message bytes to forward when result is MOTOR_COMMAND_APPLICATION_FORWARD. */
    uint16_t forward_length; /**< Number of bytes available at forward_data. */
} MotorCommandApplicationEvent;

/** @brief Accumulates information responses and the derived calibration digest. */
typedef struct {
    MotorCommandInformation information; /**< Values and byte arrays received for supported information selectors. */
    uint8_t digest[MOTOR_COMMAND_DIGEST_SIZE]; /**< Eight-byte digest derived from a calibration response. */
} MotorCommandApplication;

/**
 * @brief Initializes motor-command application state.
 *
 * Clears all accumulated information-selector values and the derived calibration digest. The
 * application pointer must refer to writable storage.
 *
 * @param[out] application Application state to initialize.
 */
void motor_command_application_init(MotorCommandApplication *application);

/**
 * @brief Applies one decoded motor-command application message.
 *
 * Stores supported information and calibration responses, or exposes vendor and selector-2 data
 * for forwarding while leaving the application state unchanged for invalid messages.
 *
 * @param[in,out] application Application state to update.
 * @param[in] message Decoded application message to apply.
 * @return Application disposition and, when forwarding, a view of the message bytes.
 */
MotorCommandApplicationEvent motor_command_application_apply(MotorCommandApplication *application,
                                                             const MotorCommandMessage *message);

#endif
