#ifndef OPENTEC_BASE_PROFILE_TUNING_INTERACTION_H
#define OPENTEC_BASE_PROFILE_TUNING_INTERACTION_H

#include <stdbool.h>
#include <stdint.h>

typedef enum {
    TUNING_INTERACTION_CLOSED,
    TUNING_INTERACTION_MENU_HELD,
    TUNING_INTERACTION_ENTRY_OPEN,
} TuningInteractionPhase;

/** @brief Attached-wheel inputs that affect tuning-menu pedal-adjustment shortcuts. */
typedef struct {
    uint8_t wheel_mode;
    uint16_t primary_buttons;
    uint16_t secondary_buttons;
    bool adapter_profile_shortcut;
    bool available;
} TuningInteractionInput;

/** @brief Logical tuning-menu phase needed to distinguish profile and entry shortcuts. */
typedef struct {
    TuningInteractionPhase phase;
    bool closing;
} TuningInteraction;

void tuning_interaction_init(TuningInteraction *interaction);
bool tuning_interaction_requests_pedal_adjustment(TuningInteraction *interaction,
                                                  const TuningInteractionInput *input);

#endif
