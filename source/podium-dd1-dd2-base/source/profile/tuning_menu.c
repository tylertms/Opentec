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

void tuning_menu_reset(TuningMenu *menu) {
    if (menu != NULL) {
        *menu = (TuningMenu){.selected_entry = TUNING_ENTRY_COUNT};
    }
}

/**
 * @brief Selects the visible side of a newly chosen tuning entry.
 *
 * Shows every newly selected entry in its label presentation.
 *
 * @param[in,out] menu Tuning menu receiving the entry presentation.
 */
static void show_selected_entry(TuningMenu *menu) {
    menu->view = TUNING_MENU_VIEW_LABEL;
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
        tuning_menu_reset(menu);
        return update;
    }
    if (phase != TUNING_INTERACTION_ENTRY_OPEN) {
        return update;
    }

    bool selected_available = menu->selected_entry < TUNING_ENTRY_COUNT &&
                              tuning_entry_available(menu->selected_entry, bank, availability);
    if (!selected_available) {
        TuningEntry origin = menu->selected_entry;
        bool directional_navigation = navigation.mode == TUNING_NAVIGATION_PREVIOUS ||
                                      navigation.mode == TUNING_NAVIGATION_NEXT;
        TuningNavigationMode direction = navigation.mode == TUNING_NAVIGATION_PREVIOUS
                                             ? TUNING_NAVIGATION_PREVIOUS
                                             : TUNING_NAVIGATION_NEXT;
        TuningEntry start = directional_navigation ? origin : TUNING_ENTRY_COUNT;
        menu->selected_entry = tuning_entry_navigate(start, direction, bank, availability);
        update.entry_changed = menu->selected_entry != origin;
        show_selected_entry(menu);
        if (directional_navigation) {
            return update;
        }
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
        bool value_view_visible = menu->view == TUNING_MENU_VIEW_VALUE;
        menu->view = TUNING_MENU_VIEW_VALUE;
        if (value_view_visible && menu->selected_entry < TUNING_ENTRY_COUNT) {
            update.adjustment_requested = true;
            update.adjusted_entry = menu->selected_entry;
            update.value_changed =
                tuning_entry_adjust(bank, menu->selected_entry, navigation, adjustment);
        }
    } else if (navigation.mode == TUNING_NAVIGATION_TOGGLE_VIEW) {
        menu->view =
            menu->view == TUNING_MENU_VIEW_LABEL ? TUNING_MENU_VIEW_VALUE : TUNING_MENU_VIEW_LABEL;
    }
    return update;
}
