#ifndef OPENTEC_BASE_WHEEL_PULSE_GATE_H
#define OPENTEC_BASE_WHEEL_PULSE_GATE_H

#include <stdbool.h>
#include <stdint.h>

/** @brief Independent Xbox and PlayStation attached-wheel pulse deadlines. */
typedef struct {
    uint32_t deadlines_ms[2]; /**< Next allowed deadline for each gated interface. */
} WheelPulseGate;

/**
 * @brief Clears attached-wheel interface pulse deadlines.
 *
 * Returns both independent interface gates to their initial zero deadlines.
 *
 * @param[out] gate Pulse timing state to initialize.
 */
void wheel_pulse_gate_init(WheelPulseGate *gate);

/**
 * @brief Applies attached-wheel interface pulse timing.
 *
 * Direct interfaces pass immediately. Xbox, PlayStation, and auxiliary-pulse interfaces pass only
 * after their hold interval and restart that interval when a nonzero pulse is accepted.
 *
 * @param[in,out] gate Interface deadlines to update.
 * @param[in] interface_mode Active wheel interface mode.
 * @param[in] now_ms Current monotonic time in milliseconds.
 * @param[in] pulse_flags Positive and negative pulse flags.
 * @return True when the pulse flags may update motion counters; otherwise false.
 */
bool wheel_pulse_gate_ready(WheelPulseGate *gate, uint8_t interface_mode, uint32_t now_ms,
                            uint8_t pulse_flags);

#endif
