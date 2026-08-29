#ifndef OPENTEC_BASE_WHEEL_MOTION_H
#define OPENTEC_BASE_WHEEL_MOTION_H

#include <stdint.h>

enum { WHEEL_MOTION_AXIS_COUNT = 4 };

typedef struct {
    uint8_t primary;
    uint8_t axes[WHEEL_MOTION_AXIS_COUNT];
} WheelMotion;

void wheel_motion_init(WheelMotion *motion);
void wheel_motion_accumulate_primary(WheelMotion *motion, int8_t delta);
void wheel_motion_accumulate_axis(WheelMotion *motion, uint8_t axis, int8_t delta);
int8_t wheel_motion_primary_direction(const WheelMotion *motion);
int8_t wheel_motion_take_primary(WheelMotion *motion);
int8_t wheel_motion_take_axis(WheelMotion *motion, uint8_t axis);

#endif
