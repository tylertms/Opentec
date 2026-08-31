#ifndef OPENTEC_BASE_MOTOR_COMMAND_MESSAGE_H
#define OPENTEC_BASE_MOTOR_COMMAND_MESSAGE_H

#include <stdbool.h>
#include <stdint.h>

/** @brief Identifies the application-level kind of a decoded motor command. */
typedef enum {
    MOTOR_COMMAND_MESSAGE_INFORMATION, /**< Information-selector response. */
    MOTOR_COMMAND_MESSAGE_CALIBRATION, /**< Calibration response. */
    MOTOR_COMMAND_MESSAGE_VENDOR_CONTINUATION, /**< Vendor response continuation. */
    MOTOR_COMMAND_MESSAGE_VENDOR_FINAL, /**< Final vendor response. */
} MotorCommandMessageKind;

/** @brief Provides views into one decoded motor-command application message. */
typedef struct {
    MotorCommandMessageKind kind; /**< Application-level message classification. */
    uint8_t command; /**< Command byte from the application payload. */
    uint8_t selector; /**< Information selector, when kind is information. */
    const uint8_t *payload; /**< Complete application payload view. */
    uint16_t payload_length; /**< Number of bytes in payload. */
    const uint8_t *data; /**< Command data view used by the application handler. */
    uint16_t data_length; /**< Number of bytes in data. */
} MotorCommandMessage;

/**
 * @brief Decodes one motor-command application payload.
 *
 * Classifies supported information, calibration, and vendor command bytes and records their
 * selector and data views without copying the payload.
 *
 * @param[in] payload Complete application payload to decode.
 * @param[in] length Number of bytes in payload.
 * @param[out] message Decoded message view to populate.
 * @return true when payload contains a supported complete message; otherwise false.
 */
bool motor_command_message_decode(const uint8_t *payload, uint16_t length,
                                  MotorCommandMessage *message);

#endif
