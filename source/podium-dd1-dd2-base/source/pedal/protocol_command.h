#ifndef OPENTEC_BASE_PEDAL_PROTOCOL_COMMAND_H
#define OPENTEC_BASE_PEDAL_PROTOCOL_COMMAND_H

#include <stdbool.h>
#include <stdint.h>

#include "usb/operating_mode_command.h"

typedef enum {
    PEDAL_PROTOCOL_COMMAND_UPDATE,
    PEDAL_PROTOCOL_COMMAND_LEGACY_SCALE,
} PedalProtocolCommandKind;

typedef struct {
    PedalProtocolCommandKind kind;
    uint8_t value;
    uint8_t first;
    uint8_t second;
} PedalProtocolCommand;

bool pedal_protocol_command_decode(const UsbOperatingModeCommand *source,
                                   PedalProtocolCommand *command);

#endif
