#include <assert.h>
#include <stdint.h>

#include "wheel/pulse_gate.h"

static void test_applies_independent_interface_deadlines(void) {
    WheelPulseGate gate;
    wheel_pulse_gate_init(&gate);

    assert(wheel_pulse_gate_ready(&gate, 0, 0, 0x10));
    assert(!wheel_pulse_gate_ready(&gate, 6, 0, 0x10));
    assert(wheel_pulse_gate_ready(&gate, 6, 1, 0x10));
    assert(!wheel_pulse_gate_ready(&gate, 6, 91, 0x10));
    assert(wheel_pulse_gate_ready(&gate, 6, 92, 0x10));
    assert(wheel_pulse_gate_ready(&gate, 7, 1, 0x10));
    assert(!wheel_pulse_gate_ready(&gate, 7, 16, 0x10));
    assert(wheel_pulse_gate_ready(&gate, 7, 17, 0x10));
    assert(!wheel_pulse_gate_ready(&gate, 10, 17, 0x10));
    assert(wheel_pulse_gate_ready(&gate, 10, 33, 0x10));
}

static void test_zero_flags_do_not_extend_a_deadline(void) {
    WheelPulseGate gate;
    wheel_pulse_gate_init(&gate);

    assert(wheel_pulse_gate_ready(&gate, 6, 1, 0));
    assert(wheel_pulse_gate_ready(&gate, 6, 2, 0x10));
    assert(!wheel_pulse_gate_ready(&gate, 6, 92, 0));
    assert(wheel_pulse_gate_ready(&gate, 6, 93, 0));
}

int main(void) {
    test_applies_independent_interface_deadlines();
    test_zero_flags_do_not_extend_a_deadline();
    return 0;
}
