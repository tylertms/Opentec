#ifndef OPENTEC_BASE_SYSTEM_BASE_MODE_CONTROLLER_H
#define OPENTEC_BASE_SYSTEM_BASE_MODE_CONTROLLER_H

#include <stdbool.h>
#include <stdint.h>

/**
 * @brief Runtime phases of the retained Xbox base-mode recovery controller.
 */
typedef enum {
    BASE_MODE_CONTROLLER_RESET = 0,             /**< Controller is inactive. */
    BASE_MODE_CONTROLLER_MEMORY_STARTUP = 1,    /**< Motor-memory startup is running. */
    BASE_MODE_CONTROLLER_STATUS_USB_DELAY = 2,  /**< Waiting 100 ms before USB enable. */
    BASE_MODE_CONTROLLER_STATUS_USB_ENABLE = 3, /**< Enabling USB after the delay. */
    BASE_MODE_CONTROLLER_STATUS_WAIT = 4,       /**< Waiting for bounded status transport. */
    BASE_MODE_CONTROLLER_STATUS_ACTIVE = 5,     /**< Status transport is active. */
    BASE_MODE_CONTROLLER_MEMORY_TIMEOUT = 15,   /**< Waiting through the second timeout window. */
} BaseModeControllerPhase;

/**
 * @brief Actions emitted while advancing retained Xbox recovery.
 */
typedef enum {
    BASE_MODE_CONTROLLER_ACTION_NONE = 0, /**< No runtime side effect is requested. */
    BASE_MODE_CONTROLLER_ACTION_DISPLAY_ERROR = 1u << 0, /**< Show the Xbox error page. */
    BASE_MODE_CONTROLLER_ACTION_RESET_MEMORY = 1u << 1,  /**< Reset motor-memory transaction. */
    BASE_MODE_CONTROLLER_ACTION_ENABLE_USB = 1u << 2,    /**< Enable the prepared USB interface. */
    BASE_MODE_CONTROLLER_ACTION_FALLBACK_NATIVE = 1u << 3, /**< Return to native mode. */
} BaseModeControllerAction;

/**
 * @brief Result of one bounded motor-memory startup step.
 */
typedef enum {
    BASE_MODE_CONTROLLER_MEMORY_RUNNING,  /**< Exchange remains in progress. */
    BASE_MODE_CONTROLLER_MEMORY_COMPLETE, /**< Exchange completed successfully. */
    BASE_MODE_CONTROLLER_MEMORY_FAILED,   /**< Exchange failed and may be retried. */
} BaseModeControllerMemoryResult;

/**
 * @brief State and deadlines for retained Xbox recovery.
 */
typedef struct {
    BaseModeControllerPhase phase;   /**< Current controller phase. */
    uint32_t memory_deadline_ms;     /**< Deadline for the first memory exchange. */
    uint32_t usb_enable_deadline_ms; /**< Strict delay before USB enable. */
    uint32_t transport_deadline_ms;  /**< Deadline for status wait or second timeout. */
} BaseModeController;

/**
 * @brief Initializes an inactive retained Xbox recovery controller.
 *
 * @param[out] controller Controller state to initialize.
 */
void base_mode_controller_init(BaseModeController *controller);

/**
 * @brief Starts bounded motor-memory recovery.
 *
 * @param[in,out] controller Inactive controller to start.
 * @param[in] now_ms Current monotonic time in milliseconds.
 * @return True when recovery entered memory startup; otherwise false.
 */
bool base_mode_controller_start(BaseModeController *controller, uint32_t now_ms);

/**
 * @brief Advances one retained Xbox recovery phase.
 *
 * Memory startup may run until its two-second deadline. A successful exchange enters the strict
 * 100-millisecond USB delay and then the two-second status wait. A failed or invalid active path
 * returns the native fallback action; a memory timeout receives a second two-second bounded window
 * before the same fallback.
 *
 * @param[in,out] controller Controller state to advance.
 * @param[in] now_ms Current monotonic time in milliseconds.
 * @param[in] memory_result Current motor-memory startup result.
 * @param[in] mode_valid True when the retained Xbox mode remains valid.
 * @param[in] protocol_active True when the attached-wheel protocol remains active.
 * @return Actions for display, transaction reset, USB enable, or native fallback.
 */
uint8_t base_mode_controller_step(BaseModeController *controller, uint32_t now_ms,
                                  BaseModeControllerMemoryResult memory_result, bool mode_valid,
                                  bool protocol_active);

/**
 * @brief Reports whether recovery currently owns the motor-memory startup path.
 *
 * @param[in] controller Controller state to inspect.
 * @return True during memory startup or its bounded timeout window; otherwise false.
 */
bool base_mode_controller_memory_active(const BaseModeController *controller);

/**
 * @brief Reports the current recovery phase.
 *
 * @param[in] controller Controller state to inspect.
 * @return Current phase, or reset for a null controller.
 */
BaseModeControllerPhase base_mode_controller_phase(const BaseModeController *controller);

#endif
