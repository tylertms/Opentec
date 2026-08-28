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

typedef struct {
    uint8_t value;
    uint8_t first;
    uint8_t second;
    uint8_t scale;
} PedalProtocolStatus;

typedef enum {
    PEDAL_V3_CONTROL_UP = 1u << 0,
    PEDAL_V3_CONTROL_DOWN = 1u << 1,
    PEDAL_V3_CONTROL_ENABLE = 1u << 2,
    PEDAL_V3_CONTROL_DISABLE = 1u << 3,
    PEDAL_V3_CONTROL_AUTOMATIC = 1u << 4,
} PedalV3Control;

PedalProtocol pedal_protocol_select(uint8_t device, uint8_t response);
uint8_t pedal_legacy_request(PedalLegacyChannel channel, uint8_t protocol_first,
                             uint8_t protocol_second);
void pedal_legacy_apply_response(PedalLegacyChannel channel, uint8_t response,
                                 bool auxiliary_locked, PedalInput *input);
void pedal_v3_build_handshake(bool recovering, PedalFrame *frame);
void pedal_v3_build_status(const PedalProtocolStatus *status, PedalFrame *frame);
uint8_t pedal_v3_build_control(uint8_t pending, PedalFrame *frame);
void pedal_v3_build_input_command(const uint8_t values[PEDAL_INPUT_AXIS_COUNT], PedalFrame *frame);
void pedal_v3_build_configuration(uint8_t brake_force, bool fine_scale, bool reset,
                                  PedalFrame *frame);
void pedal_v3_build_keepalive(PedalFrame *frame);

#endif
