#include "force_feedback/script_tick.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "force_feedback/script_service.h"

enum {
    SCRIPT_TICK_DELTA = 8,
    SCRIPT_AVERAGE_TICK = 9,
    SCRIPT_SAMPLE_COUNT = 10,
    SCRIPT_TICK_SNAPSHOT = 11,
    MOTION_SELECTOR = 0,
    MOTION_FORCE = 2,
    FORCE_FEEDBACK_TICKS_PER_SECOND = 10000,
};

static const uint32_t FORCE_FEEDBACK_TICK_RESET_THRESHOLD = UINT32_C(0x337f9800);

typedef union {
    float number;
    uint32_t bits;
} TickValue;

static uint32_t float_bits(float value) { return (TickValue){.number = value}.bits; }

static void update_engine_timing(ForceFeedbackScriptSystem *system,
                                 ForceFeedbackScriptSchedule schedule) {
    uint32_t ticks = system->clock.ticks;
    uint32_t previous = schedule == FORCE_FEEDBACK_SCRIPT_SCHEDULE_IDLE
                            ? system->idle_tick_snapshot
                            : system->host_tick_snapshot;
    system->values.variables[SCRIPT_TICK_DELTA] =
        float_bits((float)(ticks - previous) / (float)FORCE_FEEDBACK_TICKS_PER_SECOND);
    if (ticks > FORCE_FEEDBACK_TICK_RESET_THRESHOLD) {
        system->clock.ticks = 0;
    }

    uint32_t snapshot = system->clock.ticks;
    if (schedule == FORCE_FEEDBACK_SCRIPT_SCHEDULE_IDLE) {
        system->idle_tick_snapshot = snapshot;
    } else {
        system->host_tick_snapshot = snapshot;
    }
    uint32_t sample_count = system->values.variables[SCRIPT_SAMPLE_COUNT] + 1u;
    system->values.variables[SCRIPT_SAMPLE_COUNT] = sample_count;
    system->values.variables[SCRIPT_TICK_SNAPSHOT] = snapshot;
    system->values.variables[SCRIPT_AVERAGE_TICK] = float_bits(
        ((float)snapshot / (float)sample_count) / (float)FORCE_FEEDBACK_TICKS_PER_SECOND);
}

/**
 * @brief Service one due force-feedback script tick and select its motor-output policy.
 *
 * Evaluates the local and host deadlines, clears the per-tick script selector and accumulated
 * force, updates engine timing operands, runs every active stored script, and refreshes normalized
 * motion state. Position-only and zero-output modes request a zero motor write. Active host input
 * also requests zero output, while ready or idle input permits a motion write only when scripts
 * leave the selector at zero. Expired host input requests zero output without executing scripts.
 *
 * @param[in,out] system Complete script runtime, storage, clocks, inputs, and scheduler state.
 * @param[in] now Current monotonic time in milliseconds.
 * @param[in] wheel_position Signed raw wheel-position sample.
 * @param[in] half_travel Positive raw wheel travel from center to either endpoint.
 * @return No motor write, a zero motor write, or the current script motion output.
 * @pre system points to a valid initialized runtime.
 * @pre Every active script slot has a corresponding valid storage allocation.
 * @pre half_travel is nonzero when wheel sampling is selected.
 */
ForceFeedbackScriptOutputPolicy force_feedback_script_tick(ForceFeedbackScriptSystem *system,
                                                           uint32_t now, int32_t wheel_position,
                                                           uint32_t half_travel) {
    if (system == NULL) {
        return FORCE_FEEDBACK_SCRIPT_OUTPUT_NONE;
    }

    ForceFeedbackScriptSchedule schedule = force_feedback_script_scheduler_step(
        &system->scheduler, &system->inputs, system->values.variables[SCRIPT_SAMPLE_COUNT], now);
    if (schedule == FORCE_FEEDBACK_SCRIPT_SCHEDULE_NONE) {
        return FORCE_FEEDBACK_SCRIPT_OUTPUT_NONE;
    }
    if (schedule == FORCE_FEEDBACK_SCRIPT_SCHEDULE_EXPIRED) {
        return FORCE_FEEDBACK_SCRIPT_OUTPUT_ZERO;
    }

    system->values.motion[MOTION_FORCE] = 0;
    system->values.motion[MOTION_SELECTOR] = 0;
    bool host_tick = schedule == FORCE_FEEDBACK_SCRIPT_SCHEDULE_HOST;
    if (system->mode == FORCE_FEEDBACK_RUNTIME_POSITION_ONLY) {
        force_feedback_script_motion_update(&system->values, &system->inputs, &system->motion,
                                            system->clock.motion_ticks, wheel_position, half_travel,
                                            host_tick);
        return FORCE_FEEDBACK_SCRIPT_OUTPUT_ZERO;
    }
    if (system->mode != FORCE_FEEDBACK_RUNTIME_ACTIVE &&
        system->mode != FORCE_FEEDBACK_RUNTIME_ZERO_OUTPUT) {
        return FORCE_FEEDBACK_SCRIPT_OUTPUT_NONE;
    }

    update_engine_timing(system, schedule);
    force_feedback_script_service_run(&system->values, &system->store, &system->clock);
    force_feedback_script_motion_update(&system->values, &system->inputs, &system->motion,
                                        system->clock.motion_ticks, wheel_position, half_travel,
                                        host_tick);

    if (system->mode == FORCE_FEEDBACK_RUNTIME_ZERO_OUTPUT ||
        (host_tick && system->inputs.status == FORCE_FEEDBACK_SCRIPT_INPUT_ACTIVE)) {
        return FORCE_FEEDBACK_SCRIPT_OUTPUT_ZERO;
    }
    return system->values.motion[MOTION_SELECTOR] == 0 ? FORCE_FEEDBACK_SCRIPT_OUTPUT_MOTION
                                                       : FORCE_FEEDBACK_SCRIPT_OUTPUT_NONE;
}

/**
 * @brief Service one force-script tick and conditionally update the motor output report.
 *
 * Leaves the existing report untouched when no motor write is selected. Zero-output policies
 * pass zero motion through the same smoothing, ramp, position-limit, clamp, and motor-scaling
 * path used for a script motion write.
 *
 * @param[in,out] system Complete script runtime and scheduler state.
 * @param[in,out] output_state Script smoothing and wheel-range end-stop state.
 * @param[in] now Current monotonic time in milliseconds.
 * @param[in] wheel_position Signed centered wheel-position sample.
 * @param[in] half_travel Positive wheel travel from center to either endpoint.
 * @param[in] config Current smoothing, strength, ramp, range, and output limits.
 * @param[in,out] report Motor output report to update when a write is selected.
 * @return Whether a motor write occurred and whether the wheel is outside its travel limit.
 */
ForceFeedbackScriptTickResult force_feedback_script_tick_output(
    ForceFeedbackScriptSystem *system, ForceFeedbackScriptOutputState *output_state, uint32_t now,
    int32_t wheel_position, uint32_t half_travel, const ForceFeedbackScriptOutputConfig *config,
    ForceOutputReport *report) {
    ForceFeedbackScriptOutputPolicy policy =
        force_feedback_script_tick(system, now, wheel_position, half_travel);
    if (policy == FORCE_FEEDBACK_SCRIPT_OUTPUT_NONE) {
        return (ForceFeedbackScriptTickResult){0};
    }

    uint32_t motion =
        policy == FORCE_FEEDBACK_SCRIPT_OUTPUT_ZERO ? 0 : system->values.motion[MOTION_FORCE];
    bool outside_travel = force_feedback_script_output_apply(output_state, motion, wheel_position,
                                                             now, config, report);
    return (ForceFeedbackScriptTickResult){
        .wrote_output = true,
        .outside_travel = outside_travel,
    };
}
