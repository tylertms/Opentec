#ifndef OPENTEC_BASE_PROFILE_TUNING_INTERACTION_H
#define OPENTEC_BASE_PROFILE_TUNING_INTERACTION_H

#include <stdbool.h>
#include <stdint.h>

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

/** @brief Attached-wheel inputs used by local tuning interaction. */
typedef struct {
    uint8_t wheel_mode;
    uint16_t primary_buttons;
    uint16_t secondary_buttons;
    int8_t analog_scale;
    bool adapter_profile_shortcut;
    bool available;
} TuningInteractionInput;

/** @brief Logical tuning-menu phase needed to distinguish profile and entry shortcuts. */
typedef struct {
    TuningInteractionPhase phase;
    TuningNavigationMode last_navigation;
    bool closing;
} TuningInteraction;

void tuning_interaction_init(TuningInteraction *interaction);
TuningNavigationEvent tuning_interaction_read_navigation(TuningInteraction *interaction,
                                                         const TuningInteractionInput *input);
bool tuning_interaction_requests_pedal_adjustment(TuningInteraction *interaction,
                                                  const TuningInteractionInput *input);

#endif
