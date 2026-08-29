#include <assert.h>
#include <stdint.h>

#include "wheel/motion.h"

int main(void) {
    WheelMotion motion;
    wheel_motion_init(&motion);

    wheel_motion_accumulate_primary(&motion, 12);
    wheel_motion_accumulate_primary(&motion, 1);
    wheel_motion_accumulate_primary(&motion, -7);
    assert(wheel_motion_primary_direction(&motion) == 1);
    assert(wheel_motion_take_primary(&motion) == 1);
    assert(wheel_motion_primary_direction(&motion) == 0);
    assert(wheel_motion_take_primary(&motion) == 0);

    wheel_motion_accumulate_primary(&motion, -1);
    wheel_motion_accumulate_primary(&motion, -100);
    assert(wheel_motion_primary_direction(&motion) == -1);
    assert(wheel_motion_take_primary(&motion) == -1);
    assert(wheel_motion_take_primary(&motion) == -1);
    assert(wheel_motion_take_primary(&motion) == 0);

    wheel_motion_accumulate_axis(&motion, 0, 1);
    wheel_motion_accumulate_axis(&motion, 1, -1);
    wheel_motion_accumulate_axis(&motion, 2, 1);
    wheel_motion_accumulate_axis(&motion, 3, 1);
    assert(wheel_motion_take_axis(&motion, 0) == 1);
    assert(wheel_motion_take_axis(&motion, 1) == -1);
    assert(wheel_motion_take_axis(&motion, 2) == 1);
    assert(wheel_motion_take_axis(&motion, 3) == 0);

    return 0;
}
