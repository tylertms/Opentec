#include <assert.h>
#include <stdbool.h>
#include <stdint.h>

#include "wheel/usb_bridge_gate.h"

static void test_waits_for_completed_protocol_exchanges(void) {
    WheelUsbBridgeGate gate;
    wheel_usb_bridge_gate_init(&gate);

    assert(wheel_usb_bridge_gate_step(NULL, 100, true) == WHEEL_USB_BRIDGE_GATE_NONE);
    assert(wheel_usb_bridge_gate_step(&gate, 100, false) == WHEEL_USB_BRIDGE_GATE_NONE);
    assert(gate.phase == WHEEL_USB_BRIDGE_GATE_WAIT_FIRST_EXCHANGE);

    assert(wheel_usb_bridge_gate_step(&gate, 100, true) ==
           WHEEL_USB_BRIDGE_GATE_CLEAR_ACKNOWLEDGEMENT);
    assert(gate.phase == WHEEL_USB_BRIDGE_GATE_WAIT_TIMEOUT);
    assert(gate.deadline_ms == 2100);

    assert(wheel_usb_bridge_gate_step(&gate, 2099, true) ==
           WHEEL_USB_BRIDGE_GATE_CLEAR_ACKNOWLEDGEMENT);
    assert(wheel_usb_bridge_gate_step(&gate, 2100, true) ==
           WHEEL_USB_BRIDGE_GATE_CLEAR_ACKNOWLEDGEMENT);
    assert(wheel_usb_bridge_gate_step(&gate, 2101, true) ==
           WHEEL_USB_BRIDGE_GATE_SET_ACKNOWLEDGEMENT);
    assert(gate.phase == WHEEL_USB_BRIDGE_GATE_READY);
    assert(wheel_usb_bridge_gate_step(&gate, 2102, false) == WHEEL_USB_BRIDGE_GATE_NONE);
    assert(wheel_usb_bridge_gate_step(&gate, 2102, true) == WHEEL_USB_BRIDGE_GATE_RELEASE);
}

static void test_preserves_unsigned_deadline_behavior_at_wrap(void) {
    WheelUsbBridgeGate gate;
    wheel_usb_bridge_gate_init(&gate);

    assert(wheel_usb_bridge_gate_step(&gate, UINT32_MAX, true) ==
           WHEEL_USB_BRIDGE_GATE_CLEAR_ACKNOWLEDGEMENT);
    assert(gate.deadline_ms == 1999);
    assert(wheel_usb_bridge_gate_step(&gate, UINT32_MAX, true) ==
           WHEEL_USB_BRIDGE_GATE_SET_ACKNOWLEDGEMENT);
}

static void test_ignores_unknown_phase(void) {
    WheelUsbBridgeGate gate = {.phase = (WheelUsbBridgeGatePhase)UINT8_MAX};
    assert(wheel_usb_bridge_gate_step(&gate, 0, true) == WHEEL_USB_BRIDGE_GATE_NONE);
    wheel_usb_bridge_gate_init(NULL);
}

int main(void) {
    test_waits_for_completed_protocol_exchanges();
    test_preserves_unsigned_deadline_behavior_at_wrap();
    test_ignores_unknown_phase();
    return 0;
}
