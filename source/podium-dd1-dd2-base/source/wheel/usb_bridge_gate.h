#ifndef OPENTEC_BASE_WHEEL_USB_BRIDGE_GATE_H
#define OPENTEC_BASE_WHEEL_USB_BRIDGE_GATE_H

#include <stdbool.h>
#include <stdint.h>

/** @brief Protocol timeout stages required before USB bridge entry. */
typedef enum {
    WHEEL_USB_BRIDGE_GATE_WAIT_FIRST_EXCHANGE,
    WHEEL_USB_BRIDGE_GATE_WAIT_TIMEOUT,
    WHEEL_USB_BRIDGE_GATE_READY,
} WheelUsbBridgeGatePhase;

/** @brief Response flag or transition operation produced by one gate step. */
typedef enum {
    WHEEL_USB_BRIDGE_GATE_NONE,
    WHEEL_USB_BRIDGE_GATE_CLEAR_ACKNOWLEDGEMENT,
    WHEEL_USB_BRIDGE_GATE_SET_ACKNOWLEDGEMENT,
    WHEEL_USB_BRIDGE_GATE_RELEASE,
} WheelUsbBridgeGateResult;

/** @brief USB bridge protocol gate phase and timeout deadline. */
typedef struct {
    uint32_t deadline_ms;
    WheelUsbBridgeGatePhase phase;
} WheelUsbBridgeGate;

void wheel_usb_bridge_gate_init(WheelUsbBridgeGate *gate);
WheelUsbBridgeGateResult wheel_usb_bridge_gate_step(WheelUsbBridgeGate *gate, uint32_t now_ms,
                                                    bool protocol_exchange_completed);

#endif
