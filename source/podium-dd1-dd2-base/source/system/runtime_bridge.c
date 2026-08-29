#include "system/runtime_bridge.h"

#include <stdbool.h>
#include <stdint.h>

#include "usb/operating_mode_command.h"

enum {
    RUNTIME_BRIDGE_USB_START_DELAY_MS = 10,
    RUNTIME_BRIDGE_STANDARD_SETTLE_MS = 100,
    RUNTIME_BRIDGE_USB_SETTLE_MS = 300,
    RUNTIME_BRIDGE_PROTOCOL_WAIT_MS = 1000,
    RUNTIME_BRIDGE_PROTOCOL_START_DELAY_MS = 500,
};

/**
 * @brief Tests whether a runtime transition deadline has passed.
 *
 * Preserves the transition controller's strict greater-than comparison across timer wraparound.
 *
 * @param[in] now_ms Current monotonic time in milliseconds.
 * @param[in] deadline_ms Recorded transition deadline.
 * @return True only after the deadline; otherwise false.
 */
static bool deadline_passed(uint32_t now_ms, uint32_t deadline_ms) {
    return (int32_t)(now_ms - deadline_ms) > 0;
}

/**
 * @brief Prepares the USB controller and schedules a transfer handshake.
 *
 * Requests controller preparation, enables transfer timing when the selected path requires it
 * immediately, and records the common start and settle deadlines.
 *
 * @param[in,out] bridge Runtime bridge entering the common USB preparation path.
 * @param[in] now_ms Current monotonic time in milliseconds.
 * @param[in] start_delay_ms Delay before starting the wheel-transfer handshake.
 * @param[in] settle_delay_ms Minimum delay before activating updater USB mode.
 * @return USB preparation and transfer-timer actions.
 */
static uint16_t prepare_transfer(RuntimeBridge *bridge, uint32_t now_ms, uint16_t start_delay_ms,
                                 uint16_t settle_delay_ms) {
    bridge->start_deadline_ms = now_ms + start_delay_ms;
    bridge->settle_deadline_ms = now_ms + settle_delay_ms;
    bridge->phase = RUNTIME_BRIDGE_WAIT_START_DELAY;
    return RUNTIME_BRIDGE_ACTION_PREPARE_USB | (bridge->mode == USB_RUNTIME_MODE_STATUS_BRIDGE
                                                    ? RUNTIME_BRIDGE_ACTION_NONE
                                                    : RUNTIME_BRIDGE_ACTION_ENABLE_TRANSFER_TIMER);
}

/**
 * @brief Initializes runtime bridge state.
 *
 * Selects normal runtime mode and leaves the bridge idle.
 *
 * @param[out] bridge Runtime bridge state to initialize.
 */
void runtime_bridge_init(RuntimeBridge *bridge) {
    if (bridge != NULL) {
        *bridge = (RuntimeBridge){0};
    }
}

/**
 * @brief Starts a requested runtime bridge transition.
 *
 * Selects the auxiliary, status, USB, or protocol prerequisite path. Auxiliary modes request their
 * shutdown handshake, status mode marks its next wheel response, USB mode waits for the protocol
 * timeout gate, and protocol mode starts with the wheel-transfer probe.
 *
 * @param[in,out] bridge Idle runtime bridge accepting the transition.
 * @param[in] mode Requested runtime service mode.
 * @return Initial action flags, or no actions when the request is unsupported or busy.
 */
uint16_t runtime_bridge_start(RuntimeBridge *bridge, UsbRuntimeMode mode) {
    if (bridge == NULL || bridge->phase != RUNTIME_BRIDGE_IDLE) {
        return RUNTIME_BRIDGE_ACTION_NONE;
    }

    bridge->mode = mode;
    bridge->startup_recovery = false;
    if (mode == USB_RUNTIME_MODE_AUXILIARY || mode == USB_RUNTIME_MODE_AUXILIARY_RECOVERY) {
        bridge->phase = RUNTIME_BRIDGE_WAIT_AUXILIARY;
        return RUNTIME_BRIDGE_ACTION_REQUEST_AUXILIARY_HANDSHAKE;
    }
    if (mode == USB_RUNTIME_MODE_STATUS_BRIDGE) {
        bridge->phase = RUNTIME_BRIDGE_WAIT_WHEEL_STATUS;
        return RUNTIME_BRIDGE_ACTION_MARK_WHEEL_STATUS;
    }
    if (mode == USB_RUNTIME_MODE_USB_BRIDGE) {
        bridge->phase = RUNTIME_BRIDGE_WAIT_USB_READY;
        return RUNTIME_BRIDGE_ACTION_NONE;
    }
    if (mode == USB_RUNTIME_MODE_PROTOCOL_BRIDGE) {
        bridge->phase = RUNTIME_BRIDGE_WAIT_INITIAL_TRANSFER;
        return RUNTIME_BRIDGE_ACTION_ENABLE_TRANSFER_TIMER | RUNTIME_BRIDGE_ACTION_START_TRANSFER;
    }

    bridge->mode = USB_RUNTIME_MODE_NORMAL;
    return RUNTIME_BRIDGE_ACTION_NONE;
}

/**
 * @brief Starts the startup auxiliary-updater recovery path.
 *
 * Skips the normal-controller shutdown handshake after the startup discovery window found no
 * controller, then applies the common ten-millisecond probe delay and 100-millisecond USB settling
 * interval.
 *
 * @param[in,out] bridge Idle runtime bridge accepting the recovery path.
 * @param[in] now_ms Current monotonic time in milliseconds.
 * @return USB preparation and transfer-timer actions, or no actions when the bridge is busy.
 */
uint16_t runtime_bridge_start_auxiliary_recovery(RuntimeBridge *bridge, uint32_t now_ms) {
    if (bridge == NULL || bridge->phase != RUNTIME_BRIDGE_IDLE) {
        return RUNTIME_BRIDGE_ACTION_NONE;
    }
    bridge->mode = USB_RUNTIME_MODE_AUXILIARY_RECOVERY;
    bridge->startup_recovery = true;
    return prepare_transfer(bridge, now_ms, RUNTIME_BRIDGE_USB_START_DELAY_MS,
                            RUNTIME_BRIDGE_STANDARD_SETTLE_MS);
}

/**
 * @brief Advances a runtime bridge transition.
 *
 * Resolves the selected prerequisite, enforces the 10, 100, 300, 500, and 1000 millisecond
 * transition deadlines, handles the protocol fallback and USB retry paths, and emits updater
 * activation or service actions when due.
 *
 * @param[in,out] bridge Runtime bridge state to advance.
 * @param[in] input Current time and prerequisite completion signals.
 * @return Action flags for the owning USB, wheel, and transfer services.
 */
uint16_t runtime_bridge_step(RuntimeBridge *bridge, const RuntimeBridgeInput *input) {
    if (bridge == NULL || input == NULL) {
        return RUNTIME_BRIDGE_ACTION_NONE;
    }
    if (bridge->phase == RUNTIME_BRIDGE_ACTIVE) {
        return RUNTIME_BRIDGE_ACTION_SERVICE_UPDATER;
    }
    if (bridge->phase == RUNTIME_BRIDGE_WAIT_AUXILIARY) {
        return input->auxiliary_handshake_complete
                   ? prepare_transfer(bridge, input->now_ms, RUNTIME_BRIDGE_USB_START_DELAY_MS,
                                      RUNTIME_BRIDGE_STANDARD_SETTLE_MS)
                   : RUNTIME_BRIDGE_ACTION_NONE;
    }
    if (bridge->phase == RUNTIME_BRIDGE_WAIT_USB_READY) {
        if (!input->usb_bridge_ready) {
            return RUNTIME_BRIDGE_ACTION_NONE;
        }
        return prepare_transfer(bridge, input->now_ms, RUNTIME_BRIDGE_USB_START_DELAY_MS,
                                RUNTIME_BRIDGE_USB_SETTLE_MS);
    }
    if (bridge->phase == RUNTIME_BRIDGE_WAIT_WHEEL_STATUS) {
        if (!input->marked_wheel_status_received) {
            return RUNTIME_BRIDGE_ACTION_NONE;
        }
        uint16_t actions =
            prepare_transfer(bridge, input->now_ms, RUNTIME_BRIDGE_USB_START_DELAY_MS,
                             RUNTIME_BRIDGE_STANDARD_SETTLE_MS);
        return actions | RUNTIME_BRIDGE_ACTION_INITIALIZE_DIRECT_TRANSFER;
    }
    if (bridge->phase == RUNTIME_BRIDGE_WAIT_INITIAL_TRANSFER) {
        if (input->transfer_status == RUNTIME_BRIDGE_TRANSFER_COMPLETE) {
            bridge->settle_deadline_ms = input->now_ms + RUNTIME_BRIDGE_STANDARD_SETTLE_MS;
            bridge->phase = RUNTIME_BRIDGE_WAIT_SETTLE;
            return RUNTIME_BRIDGE_ACTION_PREPARE_USB;
        }
        if (input->transfer_status == RUNTIME_BRIDGE_TRANSFER_FAILED) {
            bridge->settle_deadline_ms = input->now_ms + RUNTIME_BRIDGE_PROTOCOL_WAIT_MS;
            bridge->phase = RUNTIME_BRIDGE_WAIT_PROTOCOL_COMMAND;
            return RUNTIME_BRIDGE_ACTION_DISABLE_TRANSFER_TIMER |
                   RUNTIME_BRIDGE_ACTION_REQUEST_PROTOCOL_COMMAND;
        }
        return RUNTIME_BRIDGE_ACTION_NONE;
    }
    if (bridge->phase == RUNTIME_BRIDGE_WAIT_PROTOCOL_COMMAND) {
        if (!input->protocol_command_acknowledged &&
            !deadline_passed(input->now_ms, bridge->settle_deadline_ms)) {
            return RUNTIME_BRIDGE_ACTION_NONE;
        }
        return prepare_transfer(bridge, input->now_ms, RUNTIME_BRIDGE_PROTOCOL_START_DELAY_MS,
                                RUNTIME_BRIDGE_STANDARD_SETTLE_MS);
    }
    if (bridge->phase == RUNTIME_BRIDGE_WAIT_START_DELAY) {
        if (!deadline_passed(input->now_ms, bridge->start_deadline_ms)) {
            return RUNTIME_BRIDGE_ACTION_NONE;
        }
        bridge->phase = RUNTIME_BRIDGE_WAIT_TRANSFER;
        return RUNTIME_BRIDGE_ACTION_START_TRANSFER |
               (bridge->mode == USB_RUNTIME_MODE_STATUS_BRIDGE
                    ? RUNTIME_BRIDGE_ACTION_ENABLE_TRANSFER_TIMER
                    : RUNTIME_BRIDGE_ACTION_NONE);
    }
    if (bridge->phase == RUNTIME_BRIDGE_WAIT_TRANSFER) {
        if (input->transfer_status == RUNTIME_BRIDGE_TRANSFER_COMPLETE) {
            bridge->phase = RUNTIME_BRIDGE_WAIT_SETTLE;
        } else if (input->transfer_status == RUNTIME_BRIDGE_TRANSFER_FAILED &&
                   bridge->startup_recovery) {
            bridge->mode = USB_RUNTIME_MODE_NORMAL;
            bridge->phase = RUNTIME_BRIDGE_IDLE;
            bridge->startup_recovery = false;
            return RUNTIME_BRIDGE_ACTION_DISABLE_TRANSFER_TIMER |
                   RUNTIME_BRIDGE_ACTION_RESTORE_NORMAL_USB;
        } else if (bridge->mode == USB_RUNTIME_MODE_USB_BRIDGE &&
                   deadline_passed(input->now_ms, bridge->settle_deadline_ms)) {
            bridge->phase = RUNTIME_BRIDGE_WAIT_USB_READY;
        }
        return RUNTIME_BRIDGE_ACTION_NONE;
    }
    if (bridge->phase == RUNTIME_BRIDGE_WAIT_SETTLE) {
        if (!deadline_passed(input->now_ms, bridge->settle_deadline_ms)) {
            return RUNTIME_BRIDGE_ACTION_NONE;
        }
        bridge->phase = RUNTIME_BRIDGE_ACTIVE;
        return RUNTIME_BRIDGE_ACTION_ACTIVATE_UPDATER_USB;
    }
    return RUNTIME_BRIDGE_ACTION_NONE;
}

/**
 * @brief Reports whether updater bridge service is active.
 *
 * Distinguishes the completed bridge transition from its prerequisite and timing phases.
 *
 * @param[in] bridge Runtime bridge state to inspect.
 * @return True after updater USB activation; otherwise false.
 */
bool runtime_bridge_active(const RuntimeBridge *bridge) {
    return bridge != NULL && bridge->phase == RUNTIME_BRIDGE_ACTIVE;
}
