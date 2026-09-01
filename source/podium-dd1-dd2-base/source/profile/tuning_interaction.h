#ifndef OPENTEC_BASE_PROFILE_TUNING_INTERACTION_H
#define OPENTEC_BASE_PROFILE_TUNING_INTERACTION_H

#include <stdbool.h>
#include <stdint.h>

/** @brief Timing intervals used by local tuning interaction. */
enum {
    TUNING_PROFILE_MODE_HOLD_MS = 2000,    /**< Hold time before toggling profile mode. */
    TUNING_PROFILE_RESET_HOLD_MS = 10000,  /**< Hold time before resetting profiles. */
    TUNING_PROFILE_RESET_RESULT_MS = 2000, /**< Duration of the reset result phase. */
    TUNING_RECONNECT_GRACE_MS = 1500,      /**< Grace period for an extended-wheel reconnect. */
    TUNING_PEDAL_RESULT_MS = 2000,         /**< Duration of a pedal-operation result phase. */
};

/** @brief Logical phase of local tuning interaction. */
typedef enum {
    TUNING_INTERACTION_CLOSED,         /**< Menu is closed. */
    TUNING_INTERACTION_ENTRY_OPEN,     /**< Entry selection is open. */
    TUNING_INTERACTION_CENTER_CAPTURE, /**< Waiting for center-capture chord release. */
    TUNING_INTERACTION_MENU_HELD,      /**< Profile selector is held. */
    TUNING_INTERACTION_RESET_RESULT,   /**< Showing the profile-reset result. */
    TUNING_INTERACTION_CLOSING,        /**< Completing menu close cleanup. */
    TUNING_INTERACTION_PEDAL_UP,       /**< Waiting for pedal-up operation release or completion. */
    TUNING_INTERACTION_PEDAL_DOWN, /**< Waiting for pedal-down operation release or completion. */
    TUNING_INTERACTION_PEDAL_AUTOMATIC, /**< Waiting for automatic pedal operation completion. */
} TuningInteractionPhase;

/** @brief Semantic attached-wheel tuning navigation actions. */
typedef enum {
    TUNING_NAVIGATION_NONE,        /**< No navigation action. */
    TUNING_NAVIGATION_INCREASE,    /**< Increase the selected value. */
    TUNING_NAVIGATION_DECREASE,    /**< Decrease the selected value. */
    TUNING_NAVIGATION_PREVIOUS,    /**< Select the previous entry. */
    TUNING_NAVIGATION_NEXT,        /**< Select the next entry. */
    TUNING_NAVIGATION_ANALOG,      /**< Adjust using signed analogue scale. */
    TUNING_NAVIGATION_MENU,        /**< Profile-menu selector is held. */
    TUNING_NAVIGATION_TOGGLE_VIEW, /**< Toggle entry label and value views. */
} TuningNavigationMode;

/** @brief One decoded tuning navigation action and its signed analog scale. */
typedef struct {
    TuningNavigationMode mode; /**< Decoded navigation mode. */
    int8_t scale;              /**< Signed analogue adjustment scale. */
} TuningNavigationEvent;

/** @brief Actions produced by one local tuning interaction update. */
typedef enum {
    TUNING_INTERACTION_ACTION_NONE = 0,                  /**< No interaction action. */
    TUNING_INTERACTION_ACTION_PEDAL_ADJUSTMENT = 1 << 0, /**< Request pedal adjustment. */
    TUNING_INTERACTION_ACTION_TOGGLE_PROFILE_MODE = 1
                                                    << 1, /**< Toggle Standard or Advanced mode. */
    TUNING_INTERACTION_ACTION_RESET_PROFILES = 1 << 2,    /**< Request profile reset. */
    TUNING_INTERACTION_ACTION_SHOW_CENTER_CAPTURE = 1 << 3, /**< Show center-capture prompt. */
    TUNING_INTERACTION_ACTION_CAPTURE_CENTER = 1 << 4,      /**< Capture the wheel center. */
    TUNING_INTERACTION_ACTION_PEDAL_UP = 1 << 5,            /**< Start pedal-up operation. */
    TUNING_INTERACTION_ACTION_PEDAL_DOWN = 1 << 6,          /**< Start pedal-down operation. */
    TUNING_INTERACTION_ACTION_PEDAL_AUTOMATIC = 1 << 7,     /**< Start automatic pedal operation. */
    TUNING_INTERACTION_ACTION_SHOW_SHIFTER = 1 << 8,        /**< Show shifter controls. */
    TUNING_INTERACTION_ACTION_SHOW_EXTENDED_SHIFTER = 1
                                                      << 9,  /**< Show extended shifter controls. */
    TUNING_INTERACTION_ACTION_PEDAL_UP_COMPLETE = 1 << 10,   /**< Complete pedal-up operation. */
    TUNING_INTERACTION_ACTION_PEDAL_DOWN_COMPLETE = 1 << 11, /**< Complete pedal-down operation. */
    TUNING_INTERACTION_ACTION_PEDAL_AUTOMATIC_COMPLETE =
        1 << 12, /**< Complete automatic pedal operation. */
} TuningInteractionAction;

/** @brief Attached-wheel inputs used by local tuning interaction. */
typedef struct {
    uint8_t wheel_mode;            /**< Attached-wheel mode identifier. */
    uint16_t primary_buttons;      /**< Primary attached-wheel button bits. */
    uint16_t secondary_buttons;    /**< Secondary attached-wheel button bits. */
    int8_t analog_scale;           /**< Signed analogue navigation scale. */
    uint8_t auxiliary_report[3];   /**< Auxiliary report bytes. */
    uint8_t adapter_buttons[3];    /**< Adapter button bytes. */
    uint8_t adapter_mode;          /**< Adapter mode identifier. */
    bool adapter_profile_shortcut; /**< True when the adapter profile shortcut is held. */
    bool adapter_connected;        /**< True when an adapter is connected. */
    bool profile_selector_active;  /**< True when the profile selector is active. */
    bool entry_showing_label;      /**< True when the current entry shows its label. */
    bool
        legacy_pedal_calibration_available; /**< True when legacy pedal calibration is available. */
    bool pedal_operation_pending;           /**< True while a pedal operation remains queued. */
    bool available; /**< True when the input sample is valid for interaction. */
} TuningInteractionInput;

/** @brief Logical tuning-menu phase needed to distinguish profile and entry shortcuts. */
typedef struct {
    uint32_t profile_hold_started_ms;     /**< Start time of the current profile hold. */
    uint32_t result_deadline_ms;          /**< Deadline for a result phase. */
    uint32_t reconnect_started_ms;        /**< Start time of reconnect grace. */
    TuningInteractionPhase phase;         /**< Current interaction phase. */
    TuningNavigationMode last_navigation; /**< Previous sampled navigation mode. */
    TuningNavigationEvent navigation;     /**< Navigation event pending for menu consumption. */
    bool closing;                         /**< True when release should close the menu. */
    bool profile_hold_active;             /**< True while profile hold timing is active. */
    bool profile_mode_toggled;            /**< True after the profile mode action was emitted. */
    bool pedal_adjustment_requested;      /**< True after pedal adjustment was requested. */
    bool pedal_operation_sent;            /**< True after a pedal operation action was emitted. */
    bool reconnect_active; /**< True while extended-wheel reconnect grace is active. */
} TuningInteraction;

/**
 * @brief Initializes local tuning interaction state.
 *
 * Clears pending events, timers, flags, and returns the phase to closed.
 *
 * @param[out] interaction Interaction state to initialize.
 */
void tuning_interaction_init(TuningInteraction *interaction);

/**
 * @brief Requests local tuning-menu closure.
 *
 * Clears pending navigation and marks the interaction to close after the current menu phase.
 *
 * @param[in,out] interaction Interaction state to update.
 */
void tuning_interaction_request_close(TuningInteraction *interaction);

/**
 * @brief Decodes one attached-wheel navigation sample.
 *
 * Converts attached-wheel button and analogue edges into one semantic navigation event.
 *
 * @param[in,out] interaction Interaction state retaining edge history.
 * @param[in] input Current attached-wheel input sample.
 * @return Decoded navigation event; no action when inputs are unavailable or unchanged.
 */
TuningNavigationEvent tuning_interaction_read_navigation(TuningInteraction *interaction,
                                                         const TuningInteractionInput *input);

/**
 * @brief Takes the pending navigation event.
 *
 * Returns the retained event and clears it so it is delivered once.
 *
 * @param[in,out] interaction Interaction state retaining the event.
 * @return Pending navigation event; no action when interaction is null or no event is pending.
 */
TuningNavigationEvent tuning_interaction_take_navigation(TuningInteraction *interaction);

/**
 * @brief Advances one local tuning interaction sample.
 *
 * Updates timed phases, release-gated operations, reconnect handling, and pending actions.
 *
 * @param[in,out] interaction Interaction state to advance.
 * @param[in] input Current attached-wheel, adapter, and pedal inputs.
 * @param[in] now_ms Current monotonic time in milliseconds.
 * @return Bitwise interaction actions produced by the sample; no action when inputs are invalid or
 * no transition is ready.
 */
TuningInteractionAction tuning_interaction_update(TuningInteraction *interaction,
                                                  const TuningInteractionInput *input,
                                                  uint32_t now_ms);

/**
 * @brief Reports whether host input is suppressed.
 *
 * Identifies phases in which local tuning owns ordinary host controls.
 *
 * @param[in] interaction Interaction state to inspect.
 * @return true while host input must be suppressed; false for null or non-owning phases.
 */
bool tuning_interaction_suppresses_host_input(const TuningInteraction *interaction);

/**
 * @brief Reports whether the system button is suppressed.
 *
 * Identifies entry and center-capture phases that hide the console system button.
 *
 * @param[in] interaction Interaction state to inspect.
 * @return true while the system button must be suppressed; false for null or other phases.
 */
bool tuning_interaction_suppresses_system_button(const TuningInteraction *interaction);

/**
 * @brief Reports whether adapter synchronization is blocked.
 *
 * Identifies active entry, held-profile, and closing phases that block adapter-active updates.
 *
 * @param[in] interaction Interaction state to inspect.
 * @return true when synchronization is blocked; false for null or other phases.
 */
bool tuning_interaction_blocks_adapter_synchronization(const TuningInteraction *interaction);

#endif
