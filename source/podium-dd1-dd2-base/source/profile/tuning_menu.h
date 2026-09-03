#ifndef OPENTEC_BASE_PROFILE_TUNING_MENU_H
#define OPENTEC_BASE_PROFILE_TUNING_MENU_H

#include <stdbool.h>

#include "profile/bank.h"
#include "profile/tuning_entry.h"
#include "profile/tuning_interaction.h"

/** @brief Visible side of a local tuning entry. */
typedef enum {
    TUNING_MENU_VIEW_LABEL, /**< Show the entry label. */
    TUNING_MENU_VIEW_VALUE, /**< Show the entry value. */
} TuningMenuView;

/** @brief Current local tuning-menu selection and presentation. */
typedef struct {
    TuningEntry selected_entry; /**< Currently selected logical entry. */
    TuningMenuView view;        /**< Visible side of the selected entry. */
} TuningMenu;

/** @brief Changes produced by one local tuning-menu update. */
typedef struct {
    bool entry_changed;         /**< True when the selected entry changed. */
    bool value_changed;         /**< True when the selected value changed. */
    bool adjustment_requested;  /**< True when a value adjustment must be applied downstream. */
    TuningEntry adjusted_entry; /**< Entry selected for the requested adjustment. */
} TuningMenuUpdate;

/**
 * @brief Initializes local tuning-menu state.
 *
 * Clears the selection and starts the menu in label view.
 *
 * @param[out] menu Menu state to initialize.
 */
void tuning_menu_init(TuningMenu *menu);

/**
 * @brief Resets local tuning-menu state after a profile or mode change.
 *
 * Clears the current entry so the next entry-open update selects the first available entry in
 * label view.
 *
 * @param[in,out] menu Menu state to reset.
 */
void tuning_menu_reset(TuningMenu *menu);

/**
 * @brief Advances local menu selection or adjustment.
 *
 * Opens the first available entry, repairs unavailable selections, navigates from the current entry,
 * and applies value changes only after the value presentation is already visible.
 *
 * @param[in,out] menu Menu state to update.
 * @param[in] phase Current tuning interaction phase.
 * @param[in] navigation Navigation event to apply.
 * @param[in,out] bank Profile bank to navigate or adjust.
 * @param[in] availability Current interface and hardware capabilities.
 * @param[in] adjustment Current adjustment restrictions.
 * @return Update flags; both flags are false when inputs are invalid or no change occurs.
 */
TuningMenuUpdate tuning_menu_update(TuningMenu *menu, TuningInteractionPhase phase,
                                    TuningNavigationEvent navigation, TuningProfileBank *bank,
                                    const TuningEntryAvailabilityContext *availability,
                                    const TuningEntryAdjustmentContext *adjustment);

#endif
