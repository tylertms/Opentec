#ifndef OPENTEC_BASE_PROFILE_TUNING_MENU_H
#define OPENTEC_BASE_PROFILE_TUNING_MENU_H

#include <stdbool.h>

#include "profile/bank.h"
#include "profile/tuning_entry.h"
#include "profile/tuning_interaction.h"

/** @brief Visible side of a local tuning entry. */
typedef enum {
    TUNING_MENU_VIEW_LABEL,
    TUNING_MENU_VIEW_VALUE,
} TuningMenuView;

/** @brief Current local tuning-menu selection and presentation. */
typedef struct {
    TuningEntry selected_entry;
    TuningMenuView view;
} TuningMenu;

/** @brief Changes produced by one local tuning-menu update. */
typedef struct {
    bool entry_changed;
    bool value_changed;
} TuningMenuUpdate;

void tuning_menu_init(TuningMenu *menu);
TuningMenuUpdate tuning_menu_update(TuningMenu *menu, TuningInteractionPhase phase,
                                    TuningNavigationEvent navigation, TuningProfileBank *bank,
                                    const TuningEntryAvailabilityContext *availability,
                                    const TuningEntryAdjustmentContext *adjustment);

#endif
