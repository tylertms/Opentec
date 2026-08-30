#include <assert.h>
#include <stdbool.h>
#include <stdint.h>

#include "motor/rotation_guard.h"

static void ignores_ordinary_steering_positions(void) {
    MotorRotationGuard guard;
    motor_rotation_guard_init(&guard);

    assert(!motor_rotation_guard_update(&guard, 1, 100, 0));
    assert(!guard.monitoring);
    assert(!motor_rotation_guard_update(&guard, 32768, 100, 5000));
    assert(!guard.monitoring);
}

static void warns_after_an_unchanged_strict_hold(void) {
    MotorRotationGuard guard;
    motor_rotation_guard_init(&guard);

    assert(!motor_rotation_guard_update(&guard, 0, 100, 1000));
    assert(guard.monitoring);
    assert(guard.deadline_ms == 5500);
    assert(!motor_rotation_guard_update(&guard, 0, 100, 5500));
    assert(motor_rotation_guard_update(&guard, 0, 100, 5501));
    assert(guard.triggered);
    assert(!motor_rotation_guard_update(&guard, 0, 100, 10000));
}

static void waits_for_a_nonzero_motor_runtime(void) {
    MotorRotationGuard guard;
    motor_rotation_guard_init(&guard);

    assert(!motor_rotation_guard_update(&guard, 0, 0, 1000));
    assert(!guard.monitoring);
    assert(!motor_rotation_guard_update(&guard, 0, 1, 1001));
    assert(guard.deadline_ms == 5501);
}

static void restarts_after_motor_runtime_progress(void) {
    MotorRotationGuard guard;
    motor_rotation_guard_init(&guard);

    assert(!motor_rotation_guard_update(&guard, -1, 100, 1000));
    assert(!motor_rotation_guard_update(&guard, -1, 101, 5501));
    assert(!guard.monitoring);
    assert(!motor_rotation_guard_update(&guard, -1, 101, 5502));
    assert(guard.deadline_ms == 10002);
    assert(motor_rotation_guard_update(&guard, -1, 101, 10003));
}

static void preserves_deadlines_across_timer_wrap(void) {
    MotorRotationGuard guard;
    motor_rotation_guard_init(&guard);

    assert(!motor_rotation_guard_update(&guard, 0, 7, UINT32_MAX - 1000));
    assert(guard.deadline_ms == 3499);
    assert(!motor_rotation_guard_update(&guard, 0, 7, 3499));
    assert(motor_rotation_guard_update(&guard, 0, 7, 3500));
}

int main(void) {
    ignores_ordinary_steering_positions();
    warns_after_an_unchanged_strict_hold();
    waits_for_a_nonzero_motor_runtime();
    restarts_after_motor_runtime_progress();
    preserves_deadlines_across_timer_wrap();
    return 0;
}
