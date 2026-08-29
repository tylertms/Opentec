#ifndef OPENTEC_BASE_SYSTEM_RUNTIME_BRIDGE_H
#define OPENTEC_BASE_SYSTEM_RUNTIME_BRIDGE_H

#include <stdbool.h>
#include <stdint.h>

#include "usb/operating_mode_command.h"

/** @brief Wheel-transfer handshake status consumed by runtime bridge transitions. */
typedef enum {
    RUNTIME_BRIDGE_TRANSFER_IDLE = 0,
    RUNTIME_BRIDGE_TRANSFER_PENDING = 1,
    RUNTIME_BRIDGE_TRANSFER_COMPLETE = 2,
    RUNTIME_BRIDGE_TRANSFER_FAILED = 3,
} RuntimeBridgeTransferStatus;

/** @brief Independent operations emitted to the bridge service owners. */
typedef enum {
    RUNTIME_BRIDGE_ACTION_NONE = 0,
    RUNTIME_BRIDGE_ACTION_REQUEST_AUXILIARY_HANDSHAKE = 1u << 0,
    RUNTIME_BRIDGE_ACTION_MARK_WHEEL_STATUS = 1u << 1,
    RUNTIME_BRIDGE_ACTION_PREPARE_USB = 1u << 2,
    RUNTIME_BRIDGE_ACTION_ENABLE_TRANSFER_TIMER = 1u << 3,
    RUNTIME_BRIDGE_ACTION_DISABLE_TRANSFER_TIMER = 1u << 4,
    RUNTIME_BRIDGE_ACTION_START_TRANSFER = 1u << 5,
    RUNTIME_BRIDGE_ACTION_INITIALIZE_DIRECT_TRANSFER = 1u << 6,
    RUNTIME_BRIDGE_ACTION_REQUEST_PROTOCOL_COMMAND = 1u << 7,
    RUNTIME_BRIDGE_ACTION_ACTIVATE_UPDATER_USB = 1u << 8,
    RUNTIME_BRIDGE_ACTION_SERVICE_UPDATER = 1u << 9,
} RuntimeBridgeAction;

/** @brief Current prerequisite and handshake signals for one bridge step. */
typedef struct {
    uint32_t now_ms;
    RuntimeBridgeTransferStatus transfer_status;
    bool auxiliary_handshake_complete;
    bool usb_bridge_ready;
    bool marked_wheel_status_received;
    bool protocol_command_acknowledged;
} RuntimeBridgeInput;

/** @brief High-level phase shared by all updater bridge entry paths. */
typedef enum {
    RUNTIME_BRIDGE_IDLE,
    RUNTIME_BRIDGE_WAIT_AUXILIARY,
    RUNTIME_BRIDGE_WAIT_USB_READY,
    RUNTIME_BRIDGE_WAIT_WHEEL_STATUS,
    RUNTIME_BRIDGE_WAIT_INITIAL_TRANSFER,
    RUNTIME_BRIDGE_WAIT_PROTOCOL_COMMAND,
    RUNTIME_BRIDGE_WAIT_START_DELAY,
    RUNTIME_BRIDGE_WAIT_TRANSFER,
    RUNTIME_BRIDGE_WAIT_SETTLE,
    RUNTIME_BRIDGE_ACTIVE,
} RuntimeBridgePhase;

/** @brief Runtime bridge mode, phase, and transition deadlines. */
typedef struct {
    UsbRuntimeMode mode;
    RuntimeBridgePhase phase;
    uint32_t start_deadline_ms;
    uint32_t settle_deadline_ms;
} RuntimeBridge;

void runtime_bridge_init(RuntimeBridge *bridge);
uint16_t runtime_bridge_start(RuntimeBridge *bridge, UsbRuntimeMode mode);
uint16_t runtime_bridge_step(RuntimeBridge *bridge, const RuntimeBridgeInput *input);
bool runtime_bridge_active(const RuntimeBridge *bridge);

#endif
