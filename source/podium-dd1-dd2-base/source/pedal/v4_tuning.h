#ifndef PODIUM_DD1_DD2_BASE_PEDAL_V4_TUNING_H
#define PODIUM_DD1_DD2_BASE_PEDAL_V4_TUNING_H

#include <stdbool.h>
#include <stdint.h>

enum {
    PEDAL_V4_TUNING_REQUEST_SIZE = 23,
};

typedef enum {
    PEDAL_V4_TUNING_THROTTLE_CURVE = 1,
    PEDAL_V4_TUNING_BRAKE_CURVE = 2,
    PEDAL_V4_TUNING_CLUTCH_CURVE = 3,
    PEDAL_V4_TUNING_BRAKE_FORCE = 4,
} PedalV4TuningSetting;

typedef struct {
    uint8_t brake_force;
    uint8_t clutch_curve;
    uint8_t brake_curve;
    uint8_t throttle_curve;
} PedalV4Tuning;

bool pedal_v4_tuning_request(PedalV4TuningSetting setting, uint8_t value,
                             uint8_t output[PEDAL_V4_TUNING_REQUEST_SIZE]);

#endif
