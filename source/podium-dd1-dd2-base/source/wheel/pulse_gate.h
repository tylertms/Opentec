#ifndef OPENTEC_BASE_WHEEL_PULSE_GATE_H
#define OPENTEC_BASE_WHEEL_PULSE_GATE_H

#include <stdbool.h>
#include <stdint.h>

/** @brief Independent Xbox and PlayStation attached-wheel pulse deadlines. */
typedef struct {
    uint32_t deadlines_ms[2];
} WheelPulseGate;

void wheel_pulse_gate_init(WheelPulseGate *gate);
bool wheel_pulse_gate_ready(WheelPulseGate *gate, uint8_t interface_mode, uint32_t now_ms,
                            uint8_t pulse_flags);

#endif
