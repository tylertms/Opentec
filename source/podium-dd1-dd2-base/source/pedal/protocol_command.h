#ifndef OPENTEC_BASE_PEDAL_PROTOCOL_COMMAND_H
#define OPENTEC_BASE_PEDAL_PROTOCOL_COMMAND_H

#include <stdbool.h>
#include <stdint.h>

#include "usb/operating_mode_command.h"

/**
 * @brief Identifies a decoded pedal protocol command.
 */
typedef enum {
    PEDAL_PROTOCOL_COMMAND_UPDATE,       /**< Update protocol value and selectors. */
    PEDAL_PROTOCOL_COMMAND_LEGACY_SCALE, /**< Update the legacy-calibration scale. */
} PedalProtocolCommandKind;

/**
 * @brief Stores a decoded pedal protocol command and its values.
 */
typedef struct {
    PedalProtocolCommandKind kind; /**< Decoded command kind. */
    uint8_t value;                 /**< Protocol value or legacy-calibration scale. */
    uint8_t first;                 /**< First protocol selector. */
    uint8_t second;                /**< Second protocol selector. */
} PedalProtocolCommand;

/**
 * @brief Decodes a pedal protocol command from an operating-mode command.
 *
 * Recognizes protocol-selector updates and legacy-calibration scale updates.
 *
 * @param[in] source Decoded operating-mode command.
 * @param[out] command Destination for the decoded protocol command.
 * @return True when source identifies a supported protocol command.
 */
bool pedal_protocol_command_decode(const UsbOperatingModeCommand *source,
                                   PedalProtocolCommand *command);

#endif
