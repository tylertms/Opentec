#include <assert.h>
#include <stdbool.h>
#include <stdint.h>

#include "force_feedback/safety.h"

static ForceSafetyInputs ready_inputs(void) {
    return (ForceSafetyInputs){
        .motor_ready = true,
        .wheel_ready = true,
        .protocol_ready = true,
        .command_fresh = true,
        .temperature_safe = true,
        .output_allowed = true,
    };
}

static void test_starts_fail_closed(void) {
    ForceSafety safety;
    force_safety_init(&safety);

    assert(!force_safety_permitted(&safety));
    assert(!force_safety_arm(&safety));
    assert(force_safety_blockers(&safety) != 0);
}

static void test_requires_explicit_arm(void) {
    ForceSafety safety;
    ForceSafetyInputs inputs = ready_inputs();
    force_safety_init(&safety);

    force_safety_update(&safety, &inputs);

    assert(force_safety_blockers(&safety) == 0);
    assert(!force_safety_permitted(&safety));
    assert(force_safety_arm(&safety));
    assert(force_safety_permitted(&safety));
}

static void test_any_blocker_revokes_permission(void) {
    ForceSafety safety;
    ForceSafetyInputs inputs = ready_inputs();
    force_safety_init(&safety);
    force_safety_update(&safety, &inputs);
    assert(force_safety_arm(&safety));

    inputs.command_fresh = false;
    force_safety_update(&safety, &inputs);

    assert(!force_safety_permitted(&safety));
    assert(force_safety_blockers(&safety) == FORCE_SAFETY_COMMAND_EXPIRED);
}

static void test_recovery_requires_another_arm(void) {
    ForceSafety safety;
    ForceSafetyInputs inputs = ready_inputs();
    force_safety_init(&safety);
    force_safety_update(&safety, &inputs);
    assert(force_safety_arm(&safety));

    inputs.wheel_ready = false;
    force_safety_update(&safety, &inputs);
    inputs.wheel_ready = true;
    force_safety_update(&safety, &inputs);

    assert(force_safety_blockers(&safety) == 0);
    assert(!force_safety_permitted(&safety));
    assert(force_safety_arm(&safety));
}

static void test_reports_all_active_blockers(void) {
    ForceSafety safety;
    ForceSafetyInputs inputs = ready_inputs();
    force_safety_init(&safety);
    inputs.motor_ready = false;
    inputs.temperature_safe = false;
    inputs.output_allowed = false;

    force_safety_update(&safety, &inputs);

    uint16_t expected = FORCE_SAFETY_MOTOR_UNAVAILABLE | FORCE_SAFETY_TEMPERATURE_UNSAFE |
                        FORCE_SAFETY_OUTPUT_INHIBITED;
    assert(force_safety_blockers(&safety) == expected);
}

static void test_manual_disarm_requires_another_arm(void) {
    ForceSafety safety;
    ForceSafetyInputs inputs = ready_inputs();
    force_safety_init(&safety);
    force_safety_update(&safety, &inputs);
    assert(force_safety_arm(&safety));

    force_safety_disarm(&safety);

    assert(!force_safety_permitted(&safety));
    assert(force_safety_arm(&safety));
}

int main(void) {
    test_starts_fail_closed();
    test_requires_explicit_arm();
    test_any_blocker_revokes_permission();
    test_recovery_requires_another_arm();
    test_reports_all_active_blockers();
    test_manual_disarm_requires_another_arm();
    return 0;
}
