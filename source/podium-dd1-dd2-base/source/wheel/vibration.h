#ifndef OPENTEC_BASE_WHEEL_VIBRATION_H
#define OPENTEC_BASE_WHEEL_VIBRATION_H

#include <stdbool.h>
#include <stdint.h>

enum {
    WHEEL_VIBRATION_CHANNEL_COUNT = 2,
};

typedef struct {
    uint8_t channels[WHEEL_VIBRATION_CHANNEL_COUNT];
} WheelVibrationOutput;

void wheel_vibration_from_brake(WheelVibrationOutput *output, uint16_t brake_position,
                                uint8_t strength, uint8_t wheel_mode, bool active);

#endif
