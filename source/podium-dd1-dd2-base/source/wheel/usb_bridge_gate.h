#ifndef OPENTEC_BASE_WHEEL_USB_BRIDGE_GATE_H
#define OPENTEC_BASE_WHEEL_USB_BRIDGE_GATE_H

#include <stdbool.h>
#include <stdint.h>

/** @brief Protocol timeout stages required before USB bridge entry. */
typedef enum {
    WHEEL_USB_BRIDGE_GATE_WAIT_FIRST_EXCHANGE, /**< Wait for the first completed protocol exchange.
                                                */
    WHEEL_USB_BRIDGE_GATE_WAIT_TIMEOUT, /**< Wait for the protocol timeout after first exchange. */
    WHEEL_USB_BRIDGE_GATE_READY,        /**< A later completed exchange may release the bridge. */
} WheelUsbBridgeGatePhase;

/** @brief Response flag or transition operation produced by one gate step. */
typedef enum {
    WHEEL_USB_BRIDGE_GATE_NONE,                  /**< No gate action is required. */
    WHEEL_USB_BRIDGE_GATE_CLEAR_ACKNOWLEDGEMENT, /**< Clear the protocol acknowledgement flag. */
    WHEEL_USB_BRIDGE_GATE_SET_ACKNOWLEDGEMENT,   /**< Set the protocol acknowledgement flag. */
    WHEEL_USB_BRIDGE_GATE_RELEASE,               /**< Release the USB bridge to the updater path. */
} WheelUsbBridgeGateResult;

/** @brief USB bridge protocol gate phase and timeout deadline. */
typedef struct {
    uint32_t deadline_ms;          /**< Monotonic timeout deadline after the first exchange. */
    WheelUsbBridgeGatePhase phase; /**< Current USB bridge gate phase. */
} WheelUsbBridgeGate;

/**
 * @brief Initializes USB bridge protocol gating.
 *
 * Clears the phase and timeout deadline so the next completed exchange starts the gate sequence.
 *
 * @param[out] gate USB bridge gate to initialize; null is ignored.
 */
void wheel_usb_bridge_gate_init(WheelUsbBridgeGate *gate);

/**
 * @brief Advances USB bridge protocol gating.
 *
 * Processes one completed command-two exchange and returns the acknowledgement or release action
 * for the current gate phase.
 *
 * @param[in,out] gate USB bridge gate to advance.
 * @param[in] now_ms Current monotonic time in milliseconds.
 * @param[in] protocol_exchange_completed True when a command-two exchange completed this iteration.
 * @return Gate action for the completed exchange, or WHEEL_USB_BRIDGE_GATE_NONE when no exchange
 * completed or gate is null.
 */
WheelUsbBridgeGateResult wheel_usb_bridge_gate_step(WheelUsbBridgeGate *gate, uint32_t now_ms,
                                                    bool protocol_exchange_completed);

#endif
