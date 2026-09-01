#ifndef OPENTEC_BASE_SYSTEM_RUNTIME_BRIDGE_H
#define OPENTEC_BASE_SYSTEM_RUNTIME_BRIDGE_H

#include <stdbool.h>
#include <stdint.h>

#include "usb/operating_mode_command.h"

/**
 * @brief Wheel-transfer handshake status consumed by runtime bridge transitions.
 *
 * The status distinguishes an idle transfer, an in-progress transfer, and its terminal outcomes.
 */
typedef enum {
    RUNTIME_BRIDGE_TRANSFER_IDLE = 0,     /**< No transfer result is available. */
    RUNTIME_BRIDGE_TRANSFER_PENDING = 1,  /**< A transfer is still in progress. */
    RUNTIME_BRIDGE_TRANSFER_COMPLETE = 2, /**< The transfer completed successfully. */
    RUNTIME_BRIDGE_TRANSFER_FAILED = 3,   /**< The transfer failed. */
} RuntimeBridgeTransferStatus;

/**
 * @brief Independent operations emitted to bridge service owners.
 *
 * Values are bit flags and may be combined in one return value from a bridge transition step.
 */
typedef enum {
    RUNTIME_BRIDGE_ACTION_NONE = 0, /**< No bridge operation is requested. */
    RUNTIME_BRIDGE_ACTION_REQUEST_AUXILIARY_HANDSHAKE = 1u
                                                        << 0, /**< Request auxiliary handshake. */
    RUNTIME_BRIDGE_ACTION_MARK_WHEEL_STATUS = 1u << 1, /**< Mark the next wheel status response. */
    RUNTIME_BRIDGE_ACTION_PREPARE_USB = 1u << 2,       /**< Prepare the USB controller. */
    RUNTIME_BRIDGE_ACTION_ENABLE_TRANSFER_TIMER = 1u << 3,  /**< Enable transfer timing. */
    RUNTIME_BRIDGE_ACTION_DISABLE_TRANSFER_TIMER = 1u << 4, /**< Disable transfer timing. */
    RUNTIME_BRIDGE_ACTION_START_TRANSFER = 1u << 5, /**< Start the wheel-transfer handshake. */
    RUNTIME_BRIDGE_ACTION_INITIALIZE_DIRECT_TRANSFER = 1u << 6, /**< Initialize direct transfer. */
    RUNTIME_BRIDGE_ACTION_REQUEST_PROTOCOL_COMMAND = 1u << 7, /**< Request the protocol command. */
    RUNTIME_BRIDGE_ACTION_ACTIVATE_UPDATER_USB = 1u << 8,     /**< Activate updater USB mode. */
    RUNTIME_BRIDGE_ACTION_SERVICE_UPDATER = 1u << 9,          /**< Service the active updater. */
    RUNTIME_BRIDGE_ACTION_RESTORE_NORMAL_USB = 1u << 10,      /**< Restore normal USB mode. */
} RuntimeBridgeAction;

/**
 * @brief Current prerequisite and handshake signals for one bridge step.
 *
 * The owning firmware layer fills these fields from USB, wheel, and transfer services before each
 * transition step.
 */
typedef struct {
    uint32_t now_ms;                             /**< Current monotonic time in milliseconds. */
    RuntimeBridgeTransferStatus transfer_status; /**< Current wheel-transfer handshake status. */
    bool auxiliary_handshake_complete;           /**< Whether the auxiliary handshake completed. */
    bool usb_bridge_ready;                       /**< Whether USB bridge preparation completed. */
    bool marked_wheel_status_received;  /**< Whether the marked wheel status was received. */
    bool protocol_command_acknowledged; /**< Whether the requested protocol command was accepted. */
} RuntimeBridgeInput;

/**
 * @brief High-level phase shared by all updater bridge entry paths.
 *
 * The phase identifies the prerequisite or delay that must complete before the next bridge action
 * can be emitted.
 */
typedef enum {
    RUNTIME_BRIDGE_IDLE,                  /**< No runtime transition is active. */
    RUNTIME_BRIDGE_WAIT_AUXILIARY,        /**< Waiting for the auxiliary handshake. */
    RUNTIME_BRIDGE_WAIT_USB_READY,        /**< Waiting for USB bridge preparation. */
    RUNTIME_BRIDGE_WAIT_WHEEL_STATUS,     /**< Waiting for the marked wheel status. */
    RUNTIME_BRIDGE_WAIT_INITIAL_TRANSFER, /**< Waiting for the initial protocol transfer. */
    RUNTIME_BRIDGE_WAIT_PROTOCOL_COMMAND, /**< Waiting for the protocol command or timeout. */
    RUNTIME_BRIDGE_WAIT_START_DELAY,      /**< Waiting before starting the transfer. */
    RUNTIME_BRIDGE_WAIT_TRANSFER,         /**< Waiting for the wheel-transfer result. */
    RUNTIME_BRIDGE_WAIT_SETTLE, /**< Waiting for USB settling before updater activation. */
    RUNTIME_BRIDGE_ACTIVE,      /**< Updater USB mode is active. */
} RuntimeBridgePhase;

/**
 * @brief Runtime bridge mode, phase, deadlines, and startup fallback state.
 *
 * This state is shared by all runtime transition entry paths and is advanced by
 * runtime_bridge_step().
 */
typedef struct {
    UsbRuntimeMode mode;         /**< Requested USB runtime mode. */
    RuntimeBridgePhase phase;    /**< Current transition phase. */
    uint32_t start_deadline_ms;  /**< Earliest time to start the transfer. */
    uint32_t settle_deadline_ms; /**< Earliest time to activate updater USB mode. */
    bool startup_recovery;       /**< Whether failure returns directly to normal USB mode. */
} RuntimeBridge;

/**
 * @brief Initializes runtime bridge state.
 *
 * Clears transition state and leaves the bridge idle in normal runtime mode.
 *
 * @param[out] bridge Runtime bridge state to initialize.
 */
void runtime_bridge_init(RuntimeBridge *bridge);

/**
 * @brief Starts a requested runtime bridge transition.
 *
 * Selects the prerequisite path for auxiliary, status, USB, or protocol bridge modes and emits
 * the initial operation flags when the bridge is idle.
 *
 * @param[in,out] bridge Idle runtime bridge accepting the transition.
 * @param[in] mode Requested runtime service mode.
 * @return Initial action flags, or no actions when the request is unsupported or busy.
 */
uint16_t runtime_bridge_start(RuntimeBridge *bridge, UsbRuntimeMode mode);

/**
 * @brief Starts startup auxiliary-updater recovery.
 *
 * Enters the common USB preparation path without the normal-controller shutdown handshake.
 *
 * @param[in,out] bridge Idle runtime bridge accepting the recovery path.
 * @param[in] now_ms Current monotonic time in milliseconds.
 * @return USB preparation and transfer-timer actions, or no actions when the bridge is busy.
 */
uint16_t runtime_bridge_start_auxiliary_recovery(RuntimeBridge *bridge, uint32_t now_ms);

/**
 * @brief Starts the startup wheel-status recovery bridge.
 *
 * Enters active status-bridge mode immediately because the failed startup status transaction has
 * already satisfied the route-selection prerequisite.
 *
 * @param[in,out] bridge Idle runtime bridge accepting the recovery path.
 * @return Direct-transfer initialization and updater-USB activation actions, or no actions when
 * the bridge is unavailable.
 */
uint16_t runtime_bridge_start_status_recovery(RuntimeBridge *bridge);

/**
 * @brief Advances a runtime bridge transition.
 *
 * Resolves the selected prerequisite, enforces transition deadlines, handles fallback paths, and
 * emits updater activation or service actions when due.
 *
 * @param[in,out] bridge Runtime bridge state to advance.
 * @param[in] input Current time and prerequisite completion signals.
 * @return Action flags for the owning USB, wheel, and transfer services.
 */
uint16_t runtime_bridge_step(RuntimeBridge *bridge, const RuntimeBridgeInput *input);

/**
 * @brief Reports whether updater bridge service is active.
 *
 * Returns true only after the transition reaches the active phase that requests updater service.
 *
 * @param[in] bridge Runtime bridge state to inspect.
 * @return True when updater USB service is active; otherwise false.
 */
bool runtime_bridge_active(const RuntimeBridge *bridge);

#endif
