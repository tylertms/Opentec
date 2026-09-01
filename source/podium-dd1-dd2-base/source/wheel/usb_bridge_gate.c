#include "wheel/usb_bridge_gate.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/** @brief USB bridge protocol timeout. */
enum {
    WHEEL_USB_BRIDGE_PROTOCOL_TIMEOUT_MS =
        2000 /**< Timeout after first exchange in milliseconds. */
};

/**
 * @brief Initializes USB bridge protocol gating.
 *
 * Waits for the first completed command-two exchange before starting the timeout window.
 *
 * @param[out] gate USB bridge gate to initialize.
 */
void wheel_usb_bridge_gate_init(WheelUsbBridgeGate *gate) {
    if (gate != NULL) {
        *gate = (WheelUsbBridgeGate){0};
    }
}

/**
 * @brief Advances USB bridge protocol gating after a completed exchange.
 *
 * Starts a 2,000 millisecond window on the first completion, keeps acknowledgement clear through
 * the deadline, raises it on the first later completion, and releases the bridge on a subsequent
 * completion.
 *
 * @param[in,out] gate USB bridge gate to advance.
 * @param[in] now_ms Current monotonic time in milliseconds.
 * @param[in] protocol_exchange_completed True after one command-two response completed.
 * @return Required acknowledgement update or bridge release action.
 */
WheelUsbBridgeGateResult wheel_usb_bridge_gate_step(WheelUsbBridgeGate *gate, uint32_t now_ms,
                                                    bool protocol_exchange_completed) {
    if (gate == NULL || !protocol_exchange_completed) {
        return WHEEL_USB_BRIDGE_GATE_NONE;
    }
    switch (gate->phase) {
    case WHEEL_USB_BRIDGE_GATE_WAIT_FIRST_EXCHANGE:
        gate->deadline_ms = now_ms + WHEEL_USB_BRIDGE_PROTOCOL_TIMEOUT_MS;
        gate->phase = WHEEL_USB_BRIDGE_GATE_WAIT_TIMEOUT;
        return WHEEL_USB_BRIDGE_GATE_CLEAR_ACKNOWLEDGEMENT;
    case WHEEL_USB_BRIDGE_GATE_WAIT_TIMEOUT:
        if (now_ms > gate->deadline_ms) {
            gate->phase = WHEEL_USB_BRIDGE_GATE_READY;
            return WHEEL_USB_BRIDGE_GATE_SET_ACKNOWLEDGEMENT;
        }
        return WHEEL_USB_BRIDGE_GATE_CLEAR_ACKNOWLEDGEMENT;
    case WHEEL_USB_BRIDGE_GATE_READY:
        return WHEEL_USB_BRIDGE_GATE_RELEASE;
    default:
        return WHEEL_USB_BRIDGE_GATE_NONE;
    }
}
