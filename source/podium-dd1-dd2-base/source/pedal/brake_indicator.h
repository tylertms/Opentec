#ifndef OPENTEC_BASE_PEDAL_BRAKE_INDICATOR_H
#define OPENTEC_BASE_PEDAL_BRAKE_INDICATOR_H

#include <stdbool.h>
#include <stdint.h>

enum {
    PEDAL_BRAKE_INDICATOR_DISABLED = 101,
    PEDAL_BRAKE_INDICATOR_NO_UPDATE = 0x66,
};

typedef struct {
    uint8_t selector;
} PedalBrakeIndicator;

void pedal_brake_indicator_init(PedalBrakeIndicator *indicator);
uint8_t pedal_brake_indicator_update(PedalBrakeIndicator *indicator, uint8_t level,
                                     uint16_t brake_position, bool legacy_transport);

#endif
