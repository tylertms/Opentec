#ifndef OPENTEC_BASE_PROFILE_TUNING_INTERACTION_H
#define OPENTEC_BASE_PROFILE_TUNING_INTERACTION_H

#include <stdbool.h>
#include <stdint.h>

enum {
    TUNING_PROFILE_MODE_HOLD_MS = 2000,
    TUNING_PROFILE_RESET_HOLD_MS = 10000,
};

typedef enum {
    TUNING_INTERACTION_CLOSED,
    TUNING_INTERACTION_MENU_HELD,
    TUNING_INTERACTION_ENTRY_OPEN,
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
} TuningInteractionAction;

/** @brief Attached-wheel inputs used by local tuning interaction. */
typedef struct {
    uint8_t wheel_mode;
    uint16_t primary_buttons;
    uint16_t secondary_buttons;
    int8_t analog_scale;
    bool adapter_profile_shortcut;
    bool profile_selector_active;
    bool available;
} TuningInteractionInput;

/** @brief Logical tuning-menu phase needed to distinguish profile and entry shortcuts. */
typedef struct {
    uint32_t profile_hold_started_ms;
    TuningInteractionPhase phase;
    TuningNavigationMode last_navigation;
    TuningNavigationEvent navigation;
    bool closing;
    bool profile_hold_active;
    bool profile_mode_toggled;
    bool pedal_adjustment_requested;
} TuningInteraction;

void tuning_interaction_init(TuningInteraction *interaction);
TuningNavigationEvent tuning_interaction_read_navigation(TuningInteraction *interaction,
                                                         const TuningInteractionInput *input);
TuningNavigationEvent tuning_interaction_take_navigation(TuningInteraction *interaction);
TuningInteractionAction tuning_interaction_update(TuningInteraction *interaction,
                                                  const TuningInteractionInput *input,
                                                  uint32_t now_ms);

#endif
