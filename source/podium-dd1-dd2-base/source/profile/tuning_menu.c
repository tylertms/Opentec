#include "profile/tuning_menu.h"

#include <stddef.h>

#include "profile/bank.h"
#include "profile/tuning_entry.h"
#include "profile/tuning_interaction.h"

void tuning_menu_init(TuningMenu *menu) {
    if (menu != NULL) {
        *menu = (TuningMenu){.selected_entry = TUNING_ENTRY_COUNT};
    }
}

/**
 * @brief Selects the visible side of a newly chosen tuning entry.
 *
 * Shows ordinary entry labels first while keeping setup selection on its value presentation.
 *
 * @param[in,out] menu Tuning menu receiving the entry presentation.
 */
static void show_selected_entry(TuningMenu *menu) {
    menu->view = menu->selected_entry == TUNING_ENTRY_SETUP ? TUNING_MENU_VIEW_VALUE
                                                            : TUNING_MENU_VIEW_LABEL;
}

TuningMenuUpdate tuning_menu_update(TuningMenu *menu, TuningInteractionPhase phase,
                                    TuningNavigationEvent navigation, TuningProfileBank *bank,
                                    const TuningEntryAvailabilityContext *availability,
                                    const TuningEntryAdjustmentContext *adjustment) {
    TuningMenuUpdate update = {0};
    if (menu == NULL || bank == NULL || availability == NULL) {
        return update;
    }
    if (phase == TUNING_INTERACTION_CLOSED) {
        update.entry_changed = menu->selected_entry != TUNING_ENTRY_COUNT;
        menu->selected_entry = TUNING_ENTRY_COUNT;
        menu->view = TUNING_MENU_VIEW_LABEL;
        return update;
    }
    if (phase != TUNING_INTERACTION_ENTRY_OPEN) {
        return update;
    }

    if (menu->selected_entry == TUNING_ENTRY_COUNT ||
        !tuning_entry_available(menu->selected_entry, bank, availability)) {
        menu->selected_entry =
            tuning_entry_navigate(TUNING_ENTRY_COUNT, TUNING_NAVIGATION_NEXT, bank, availability);
        update.entry_changed = menu->selected_entry != TUNING_ENTRY_COUNT;
        show_selected_entry(menu);
    }
    if (navigation.mode == TUNING_NAVIGATION_PREVIOUS ||
        navigation.mode == TUNING_NAVIGATION_NEXT) {
        TuningEntry selected =
            tuning_entry_navigate(menu->selected_entry, navigation.mode, bank, availability);
        update.entry_changed |= selected != menu->selected_entry;
        menu->selected_entry = selected;
        show_selected_entry(menu);
    } else if (navigation.mode == TUNING_NAVIGATION_INCREASE ||
               navigation.mode == TUNING_NAVIGATION_DECREASE ||
               navigation.mode == TUNING_NAVIGATION_ANALOG) {
        menu->view = TUNING_MENU_VIEW_VALUE;
        update.value_changed =
            tuning_entry_adjust(bank, menu->selected_entry, navigation, adjustment);
    } else if (navigation.mode == TUNING_NAVIGATION_TOGGLE_VIEW &&
               menu->selected_entry != TUNING_ENTRY_SETUP) {
        menu->view =
            menu->view == TUNING_MENU_VIEW_LABEL ? TUNING_MENU_VIEW_VALUE : TUNING_MENU_VIEW_LABEL;
    }
    if (menu->selected_entry == TUNING_ENTRY_SETUP) {
        menu->view = TUNING_MENU_VIEW_VALUE;
    }
    return update;
}
