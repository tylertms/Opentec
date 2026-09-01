#include "force_feedback/script_tick.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "force_feedback/script_service.h"

/**
 * @brief Runtime indexes and scale used by script tick processing.
 *
 * The indexes identify timing and motion values, and the clock constant converts engine ticks to
 * seconds.
 */
enum {
    SCRIPT_TICK_DELTA = 8,     /**< Variable index containing elapsed tick time. */
    SCRIPT_AVERAGE_TICK = 9,   /**< Variable index containing average tick time. */
    SCRIPT_TICK_SNAPSHOT = 11, /**< Variable index containing the current tick snapshot. */
    MOTION_SELECTOR = 0,       /**< Motion selector value index. */
    MOTION_FORCE = 2,          /**< Secondary motion force value index. */
    FORCE_FEEDBACK_TICKS_PER_SECOND = 10000, /**< Engine-clock frequency in ticks per second. */
};

/** @brief Engine-clock value above which tick processing resets the engine counter. */
static const uint32_t FORCE_FEEDBACK_TICK_RESET_THRESHOLD = UINT32_C(0x337f9800);

/**
 * @brief Provides numeric and raw-bit views of a script tick value.
 *
 * Tick processing preserves script value bits while storing calculated timing values as
 * floating-point representations.
 */
typedef union {
    float number;  /**< Single-precision numeric view. */
    uint32_t bits; /**< Raw 32-bit representation. */
} TickValue;

/**
 * @brief Returns the bit representation of a script floating-point value.
 *
 * Preserves the 32-bit value without numeric conversion for script-visible timing fields.
 *
 * @param[in] value Floating-point value to represent.
 * @return The unchanged 32-bit representation.
 */
static uint32_t float_bits(float value) { return (TickValue){.number = value}.bits; }

/**
 * @brief Updates script-engine timing fields for one scheduled tick.
 *
 * Records elapsed and average timing values, normalizes an aged engine counter, advances the
 * sample count, and retains the schedule-specific tick snapshot.
 *
 * @param[in,out] system Script system whose timing fields are updated.
 * @param[in] schedule Host or idle schedule that selected this tick.
 */
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
    uint32_t sample_count =
        system->values.variables[FORCE_FEEDBACK_SCRIPT_SAMPLE_COUNT_VARIABLE] + 1u;
    system->values.variables[FORCE_FEEDBACK_SCRIPT_SAMPLE_COUNT_VARIABLE] = sample_count;
    system->values.variables[SCRIPT_TICK_SNAPSHOT] = snapshot;
    system->values.variables[SCRIPT_AVERAGE_TICK] = float_bits(
        ((float)snapshot / (float)sample_count) / (float)FORCE_FEEDBACK_TICKS_PER_SECOND);
}

ForceFeedbackScriptTickDecision force_feedback_script_tick(ForceFeedbackScriptSystem *system,
                                                           uint32_t now, int32_t wheel_position,
                                                           uint32_t half_travel) {
    if (system == NULL) {
        return (ForceFeedbackScriptTickDecision){0};
    }

    ForceFeedbackScriptSchedule schedule = force_feedback_script_scheduler_step(
        &system->scheduler, &system->inputs,
        system->values.variables[FORCE_FEEDBACK_SCRIPT_SAMPLE_COUNT_VARIABLE], now);
    if (schedule == FORCE_FEEDBACK_SCRIPT_SCHEDULE_NONE) {
        return (ForceFeedbackScriptTickDecision){0};
    }
    if (schedule == FORCE_FEEDBACK_SCRIPT_SCHEDULE_EXPIRED) {
        return (ForceFeedbackScriptTickDecision){
            .output_policy = FORCE_FEEDBACK_SCRIPT_OUTPUT_ZERO,
            .immediate_zero = true,
        };
    }

    system->values.motion[MOTION_FORCE] = 0;
    system->values.motion[MOTION_SELECTOR] = 0;
    bool host_tick = schedule == FORCE_FEEDBACK_SCRIPT_SCHEDULE_HOST;
    if (system->mode == FORCE_FEEDBACK_RUNTIME_POSITION_ONLY) {
        force_feedback_script_motion_update(&system->values, &system->inputs, &system->motion,
                                            system->clock.motion_ticks, wheel_position, half_travel,
                                            host_tick);
        return (ForceFeedbackScriptTickDecision){
            .output_policy = FORCE_FEEDBACK_SCRIPT_OUTPUT_ZERO,
        };
    }
    if (system->mode != FORCE_FEEDBACK_RUNTIME_ACTIVE &&
        system->mode != FORCE_FEEDBACK_RUNTIME_ZERO_OUTPUT) {
        return (ForceFeedbackScriptTickDecision){0};
    }

    update_engine_timing(system, schedule);
    bool slot_faulted =
        force_feedback_script_service_run(&system->values, &system->store, &system->clock);
    force_feedback_script_motion_update(&system->values, &system->inputs, &system->motion,
                                        system->clock.motion_ticks, wheel_position, half_travel,
                                        host_tick);

    if (system->mode == FORCE_FEEDBACK_RUNTIME_ZERO_OUTPUT ||
        (host_tick && system->inputs.status == FORCE_FEEDBACK_SCRIPT_INPUT_ACTIVE)) {
        return (ForceFeedbackScriptTickDecision){
            .output_policy = FORCE_FEEDBACK_SCRIPT_OUTPUT_ZERO,
            .slot_faulted = slot_faulted,
        };
    }
    return (ForceFeedbackScriptTickDecision){
        .output_policy = system->values.motion[MOTION_SELECTOR] == 0
                             ? FORCE_FEEDBACK_SCRIPT_OUTPUT_MOTION
                             : FORCE_FEEDBACK_SCRIPT_OUTPUT_NONE,
        .slot_faulted = slot_faulted,
    };
}

ForceFeedbackScriptTickResult force_feedback_script_tick_output(
    ForceFeedbackScriptSystem *system, ForceFeedbackScriptOutputState *output_state, uint32_t now,
    int32_t wheel_position, uint32_t half_travel, const ForceFeedbackScriptOutputConfig *config,
    ForceOutputReport *report) {
    if (system->store.position_request_pending) {
        bool outside_travel = force_feedback_script_position_output_apply(
            output_state, wheel_position, half_travel, now, config, report);
        return (ForceFeedbackScriptTickResult){
            .wrote_output = true,
            .outside_travel = outside_travel,
        };
    }
    ForceFeedbackScriptTickDecision decision =
        force_feedback_script_tick(system, now, wheel_position, half_travel);
    if (decision.output_policy == FORCE_FEEDBACK_SCRIPT_OUTPUT_NONE) {
        return (ForceFeedbackScriptTickResult){.slot_faulted = decision.slot_faulted};
    }
    if (decision.immediate_zero) {
        report->primary_magnitude = 0;
        return (ForceFeedbackScriptTickResult){
            .wrote_output = true,
            .slot_faulted = decision.slot_faulted,
        };
    }

    uint32_t motion = decision.output_policy == FORCE_FEEDBACK_SCRIPT_OUTPUT_ZERO
                          ? 0
                          : system->values.motion[MOTION_FORCE];
    bool outside_travel = force_feedback_script_output_apply(output_state, motion, wheel_position,
                                                             now, config, report);
    return (ForceFeedbackScriptTickResult){
        .wrote_output = true,
        .outside_travel = outside_travel,
        .slot_faulted = decision.slot_faulted,
    };
}
