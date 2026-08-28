#ifndef OPENTEC_BASE_PEDAL_PROTOCOL_H
#define OPENTEC_BASE_PEDAL_PROTOCOL_H

#include <stdbool.h>
#include <stdint.h>

#include "pedal/input.h"

typedef enum {
    PEDAL_DEVICE_NONE,
    PEDAL_DEVICE_V3 = 0x1a,
    PEDAL_DEVICE_V4 = 0x2a,
    PEDAL_DEVICE_INVALID = 0xff,
} PedalDevice;

typedef enum {
    PEDAL_PROTOCOL_REDISCOVER,
    PEDAL_PROTOCOL_LEGACY,
    PEDAL_PROTOCOL_V3,
    PEDAL_PROTOCOL_V4,
} PedalProtocol;

typedef enum {
    PEDAL_LEGACY_AXIS_1,
    PEDAL_LEGACY_AXIS_2,
    PEDAL_LEGACY_AXIS_3,
    PEDAL_LEGACY_AUXILIARY,
    PEDAL_LEGACY_CHANNEL_COUNT,
} PedalLegacyChannel;

PedalProtocol pedal_protocol_select(uint8_t device, uint8_t response);
uint8_t pedal_legacy_request(PedalLegacyChannel channel, uint8_t protocol_first,
                             uint8_t protocol_second);
void pedal_legacy_apply_response(PedalLegacyChannel channel, uint8_t response,
                                 bool auxiliary_locked, PedalInput *input);

#endif
