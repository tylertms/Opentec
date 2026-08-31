#ifndef OPENTEC_BASE_PROFILE_TUNING_INTERACTION_H
#define OPENTEC_BASE_PROFILE_TUNING_INTERACTION_H

#include <stdbool.h>
#include <stdint.h>

enum {
    TUNING_PROFILE_MODE_HOLD_MS = 2000,
    TUNING_PROFILE_RESET_HOLD_MS = 10000,
    TUNING_PROFILE_RESET_RESULT_MS = 2000,
    TUNING_RECONNECT_GRACE_MS = 1500,
    TUNING_PEDAL_RESULT_MS = 2000,
};

typedef enum {
    TUNING_INTERACTION_CLOSED,
    TUNING_INTERACTION_ENTRY_OPEN,
    TUNING_INTERACTION_CENTER_CAPTURE,
    TUNING_INTERACTION_MENU_HELD,
    TUNING_INTERACTION_RESET_RESULT,
    TUNING_INTERACTION_CLOSING,
    TUNING_INTERACTION_PEDAL_UP,
    TUNING_INTERACTION_PEDAL_DOWN,
    TUNING_INTERACTION_PEDAL_AUTOMATIC,
} TuningInteractionPhase;

/** @brief Semantic attached-wheel tuning navigation actions. */
typedef enum {
    TUNING_NAVIGATION_NONE,
    TUNING_NAVIGATION_INCREASE,
    TUNING_NAVIGATION_DECREASE,
    TUNING_NAVIGATION_PREVIOUS,
    TUNING_NAVIGATION_NEXT,
    TUNING_NAVIGATION_ANALOG,
    TUNING_NAVIGATION_MENU,
    TUNING_NAVIGATION_TOGGLE_VIEW,
} TuningNavigationMode;

/** @brief One decoded tuning navigation action and its signed analog scale. */
typedef struct {
    TuningNavigationMode mode;
    int8_t scale;
} TuningNavigationEvent;

/** @brief Actions produced by one local tuning interaction update. */
typedef enum {
    TUNING_INTERACTION_ACTION_NONE = 0,
    TUNING_INTERACTION_ACTION_PEDAL_ADJUSTMENT = 1 << 0,
    TUNING_INTERACTION_ACTION_TOGGLE_PROFILE_MODE = 1 << 1,
    TUNING_INTERACTION_ACTION_RESET_PROFILES = 1 << 2,
    TUNING_INTERACTION_ACTION_SHOW_CENTER_CAPTURE = 1 << 3,
    TUNING_INTERACTION_ACTION_CAPTURE_CENTER = 1 << 4,
    TUNING_INTERACTION_ACTION_PEDAL_UP = 1 << 5,
    TUNING_INTERACTION_ACTION_PEDAL_DOWN = 1 << 6,
    TUNING_INTERACTION_ACTION_PEDAL_AUTOMATIC = 1 << 7,
    TUNING_INTERACTION_ACTION_SHOW_SHIFTER = 1 << 8,
    TUNING_INTERACTION_ACTION_SHOW_EXTENDED_SHIFTER = 1 << 9,
    TUNING_INTERACTION_ACTION_PEDAL_UP_COMPLETE = 1 << 10,
    TUNING_INTERACTION_ACTION_PEDAL_DOWN_COMPLETE = 1 << 11,
    TUNING_INTERACTION_ACTION_PEDAL_AUTOMATIC_COMPLETE = 1 << 12,
} TuningInteractionAction;

/** @brief Attached-wheel inputs used by local tuning interaction. */
typedef struct {
    uint8_t wheel_mode;
    uint16_t primary_buttons;
    uint16_t secondary_buttons;
    int8_t analog_scale;
    uint8_t auxiliary_report[3];
    uint8_t adapter_buttons[3];
    uint8_t adapter_mode;
    bool adapter_profile_shortcut;
    bool adapter_connected;
    bool profile_selector_active;
    bool entry_showing_label;
    bool legacy_pedal_calibration_available;
    bool pedal_operation_pending;
    bool available;
} TuningInteractionInput;

/** @brief Logical tuning-menu phase needed to distinguish profile and entry shortcuts. */
typedef struct {
    uint32_t profile_hold_started_ms;
    uint32_t result_deadline_ms;
    uint32_t reconnect_started_ms;
    TuningInteractionPhase phase;
    TuningNavigationMode last_navigation;
    TuningNavigationEvent navigation;
    bool closing;
    bool profile_hold_active;
    bool profile_mode_toggled;
    bool pedal_adjustment_requested;
    bool pedal_operation_sent;
    bool reconnect_active;
} TuningInteraction;

void tuning_interaction_init(TuningInteraction *interaction);
void tuning_interaction_request_close(TuningInteraction *interaction);
TuningNavigationEvent tuning_interaction_read_navigation(TuningInteraction *interaction,
                                                         const TuningInteractionInput *input);
TuningNavigationEvent tuning_interaction_take_navigation(TuningInteraction *interaction);
TuningInteractionAction tuning_interaction_update(TuningInteraction *interaction,
                                                  const TuningInteractionInput *input,
                                                  uint32_t now_ms);
bool tuning_interaction_suppresses_host_input(const TuningInteraction *interaction);
bool tuning_interaction_suppresses_system_button(const TuningInteraction *interaction);
bool tuning_interaction_blocks_adapter_synchronization(const TuningInteraction *interaction);

#endif
