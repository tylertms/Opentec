#include "profile/tuning_menu.h"

#include <stddef.h>

#include "profile/bank.h"
#include "profile/tuning_entry.h"
#include "profile/tuning_interaction.h"

/**
 * @brief Initializes local tuning-menu selection.
 *
 * Starts without a selected entry so opening the menu resolves the first currently available
 * setting.
 *
 * @param[out] menu Tuning menu to initialize.
 */
void tuning_menu_init(TuningMenu *menu) {
    if (menu != NULL) {
        menu->selected_entry = TUNING_ENTRY_COUNT;
    }
}

/**
 * @brief Advances local tuning entry selection and adjustment.
 *
 * Selects the first available entry when the menu opens, repairs a selection that becomes
 * unavailable, applies previous or next navigation with wraparound, and routes value movement to
 * the selected entry. Closing the menu clears the selection.
 *
 * @param[in,out] menu Current local tuning-menu selection.
 * @param[in] phase Current tuning interaction phase.
 * @param[in] navigation Navigation event produced by the interaction layer.
 * @param[in,out] bank Tuning profile bank to navigate or adjust.
 * @param[in] availability Current interface and attached-device capabilities.
 * @param[in] adjustment Current adjustment restrictions and dynamic limits.
 * @return Entry and value change indications for the caller's display and persistence actions.
 */
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
    }
    if (navigation.mode == TUNING_NAVIGATION_PREVIOUS ||
        navigation.mode == TUNING_NAVIGATION_NEXT) {
        TuningEntry selected =
            tuning_entry_navigate(menu->selected_entry, navigation.mode, bank, availability);
        update.entry_changed |= selected != menu->selected_entry;
        menu->selected_entry = selected;
    } else if (navigation.mode == TUNING_NAVIGATION_INCREASE ||
               navigation.mode == TUNING_NAVIGATION_DECREASE ||
               navigation.mode == TUNING_NAVIGATION_ANALOG) {
        update.value_changed =
            tuning_entry_adjust(bank, menu->selected_entry, navigation, adjustment);
    }
    return update;
}
