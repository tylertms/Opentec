#ifndef OPENTEC_BASE_BOARD_POWER_H
#define OPENTEC_BASE_BOARD_POWER_H

#include <stdbool.h>
#include <stdint.h>

/**
 * @brief States of the wheel-base power-button controller.
 *
 * The controller progresses from startup through button handling and shutdown while retaining
 * its current deadline and torque-disable request.
 */
typedef enum {
    POWER_PHASE_WAITING_FOR_START, /**< Waiting for the first pressed-button sample. */
    POWER_PHASE_READY,             /**< Waiting for an unlocked pressed-button sample. */
    POWER_PHASE_BUTTON_HELD,       /**< A qualifying button hold is in progress. */
    POWER_PHASE_COMPLETE,          /**< Shutdown side effects completed; finalization is active. */
    POWER_PHASE_OFF,               /**< Retained terminal state for callers that need one. */
} PowerPhase;

/**
 * @brief Actions emitted by the power-button controller.
 *
 * Each update returns one action for the caller to apply to the power latch, torque request, or
 * shutdown sequence.
 */
typedef enum {
    POWER_ACTION_NONE,                   /**< No power-controller transition occurred. */
    POWER_ACTION_ENABLE_LATCH,           /**< Enable the hardware power latch. */
    POWER_ACTION_TORQUE_REQUEST_CHANGED, /**< The short-press torque request changed. */
    POWER_ACTION_BEGIN_SHUTDOWN,         /**< Begin the shutdown sequence. */
    POWER_ACTION_FINISH_SHUTDOWN,        /**< Complete the shutdown sequence. */
} PowerAction;

/**
 * @brief Stateful power-button and shutdown controller.
 *
 * Stores the current phase, the active modular deadline, and the requested torque-disable state
 * used across periodic updates.
 */
typedef struct {
    PowerPhase phase;                /**< Current power-controller phase. */
    uint32_t deadline_ms;            /**< Active hold deadline in milliseconds. */
    uint32_t completion_deadline_ms; /**< Display-finalization deadline in milliseconds. */
    bool torque_disabled;            /**< True when a short press requested torque disable. */
} PowerController;

/**
 * @brief Initializes the wheel-base power controller.
 *
 * Clears the controller into POWER_PHASE_WAITING_FOR_START with no active deadline or torque
 * disable request.
 *
 * @param[out] controller Power state to initialize.
 */
void power_controller_init(PowerController *controller);

/**
 * @brief Advances power-button and shutdown behavior.
 *
 * Enables the power latch on the first pressed-button sample, toggles torque disable on qualifying
 * short releases, and emits shutdown actions only after the strict hold deadline passes. Shutdown
 * completion is emitted on the next service sample without an additional delay.
 *
 * @param[in,out] controller Persistent power phase, deadline, and torque-disable state.
 * @param[in] button_pressed True while the active-high power button input is asserted.
 * @param[in] button_control_enabled True to allow a ready-state press to begin short-press or hold
 * handling.
 * @param[in] now_ms Current monotonic time in milliseconds.
 * @return The single transition action produced by this update.
 */
PowerAction power_controller_update(PowerController *controller, bool button_pressed,
                                    bool button_control_enabled, uint32_t now_ms);

#endif
