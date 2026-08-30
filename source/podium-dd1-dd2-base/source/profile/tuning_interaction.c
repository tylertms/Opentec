#include "profile/tuning_interaction.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

enum {
    WHEEL_MODE_LEGACY = 0x0e,
    WHEEL_MODE_LEGACY_ALTERNATE = 0x0f,
    WHEEL_MODE_STANDARD = 0x10,
    WHEEL_MODE_LEGACY_COMPATIBILITY = 0x17,
    WHEEL_MODE_EXTENDED = 0x1c,
    TUNING_PRIMARY_LEGACY_CENTER = 0x4000,
    TUNING_PRIMARY_INCREASE = 0x0100,
    TUNING_PRIMARY_PREVIOUS = 0x0200,
    TUNING_PRIMARY_NEXT = 0x0400,
    TUNING_PRIMARY_DECREASE = 0x0800,
    TUNING_SECONDARY_LEGACY_CENTER = 0x0010,
    TUNING_SECONDARY_ADJUSTMENT = 0x0040,
    TUNING_SECONDARY_STANDARD_CENTER = 0x0080,
    TUNING_SECONDARY_PROFILE_SHORTCUT = 0x0100,
    TUNING_SECONDARY_TOGGLE_VIEW = 0x0200,
    TUNING_SECONDARY_MENU = 0x2000,
    TUNING_SECONDARY_LEGACY_ADJUSTMENT = 0x0140,
};

/**
 * @brief Reports whether an earlier profile shortcut owns the held menu input.
 *
 * Applies the attached-wheel shortcut priority that precedes the pedal-adjustment button.
 *
 * @param[in] input Current attached-wheel and adapter inputs.
 * @return True when a higher-priority shortcut suppresses pedal adjustment.
 */
static bool profile_shortcut_precedes_adjustment(const TuningInteractionInput *input) {
    bool legacy_profile_shortcut =
        (input->wheel_mode == WHEEL_MODE_LEGACY_ALTERNATE ||
         input->wheel_mode == WHEEL_MODE_LEGACY_COMPATIBILITY) &&
        (input->secondary_buttons & TUNING_SECONDARY_PROFILE_SHORTCUT) != 0;
    return legacy_profile_shortcut || input->adapter_profile_shortcut ||
           (input->secondary_buttons & TUNING_SECONDARY_STANDARD_CENTER) != 0;
}

/**
 * @brief Reports whether the legacy center chord owns the open tuning entry.
 *
 * Matches the center-capture priority that precedes the legacy pedal-adjustment shortcut.
 *
 * @param[in] input Current attached-wheel inputs.
 * @return True when the complete legacy center chord is held.
 */
static bool legacy_center_chord_held(const TuningInteractionInput *input) {
    return (input->secondary_buttons & TUNING_SECONDARY_LEGACY_CENTER) != 0 &&
           (input->primary_buttons & TUNING_PRIMARY_LEGACY_CENTER) != 0;
}

/**
 * @brief Initializes tuning-menu interaction state.
 *
 * Starts with the profile selector and tuning entries closed.
 *
 * @param[out] interaction Interaction state to initialize.
 */
void tuning_interaction_init(TuningInteraction *interaction) {
    *interaction = (TuningInteraction){0};
}

/**
 * @brief Decodes one attached-wheel tuning navigation sample.
 *
 * Applies the firmware input priority from increase through menu, preserves the signed analog
 * scale only for analog navigation, suppresses repeated actions, and allows menu to repeat while
 * held. Unavailable input resets the edge latch.
 *
 * @param[in,out] interaction Tuning interaction and navigation edge latch.
 * @param[in] input Current attached-wheel input sample.
 * @return Decoded navigation action, or no action when unchanged or unavailable.
 */
TuningNavigationEvent tuning_interaction_read_navigation(TuningInteraction *interaction,
                                                         const TuningInteractionInput *input) {
    TuningNavigationEvent event = {0};
    if (interaction == NULL || input == NULL || !input->available) {
        if (interaction != NULL) {
            interaction->last_navigation = TUNING_NAVIGATION_NONE;
        }
        return event;
    }

    if ((input->primary_buttons & TUNING_PRIMARY_INCREASE) != 0) {
        event.mode = TUNING_NAVIGATION_INCREASE;
    }
    if ((input->primary_buttons & TUNING_PRIMARY_DECREASE) != 0) {
        event.mode = TUNING_NAVIGATION_DECREASE;
    }
    if ((input->primary_buttons & TUNING_PRIMARY_PREVIOUS) != 0) {
        event.mode = TUNING_NAVIGATION_PREVIOUS;
    }
    if ((input->primary_buttons & TUNING_PRIMARY_NEXT) != 0) {
        event.mode = TUNING_NAVIGATION_NEXT;
    }
    if ((input->secondary_buttons & TUNING_SECONDARY_TOGGLE_VIEW) != 0) {
        event.mode = TUNING_NAVIGATION_TOGGLE_VIEW;
    }
    if (input->analog_scale != 0) {
        event.mode = TUNING_NAVIGATION_ANALOG;
        event.scale = input->analog_scale;
    }
    if ((input->secondary_buttons & TUNING_SECONDARY_MENU) != 0) {
        event.mode = TUNING_NAVIGATION_MENU;
        event.scale = 0;
    }

    const TuningNavigationMode sampled_mode = event.mode;
    if (sampled_mode == interaction->last_navigation && event.mode != TUNING_NAVIGATION_MENU) {
        event = (TuningNavigationEvent){0};
    }
    interaction->last_navigation = sampled_mode;
    return event;
}

/**
 * @brief Takes the navigation event produced by the latest interaction update.
 *
 * Returns the retained event once and clears it so entry navigation and value adjustment cannot
 * process the same input twice.
 *
 * @param[in,out] interaction Tuning interaction retaining the event.
 * @return Latest navigation event, or no action when none is pending.
 */
TuningNavigationEvent tuning_interaction_take_navigation(TuningInteraction *interaction) {
    if (interaction == NULL) {
        return (TuningNavigationEvent){0};
    }
    TuningNavigationEvent navigation = interaction->navigation;
    interaction->navigation = (TuningNavigationEvent){0};
    return navigation;
}

/**
 * @brief Clears the active tuning-profile hold timer.
 *
 * Returns the hold lifecycle to its idle state without changing menu navigation.
 *
 * @param[in,out] interaction Tuning interaction to update.
 */
static void clear_profile_hold(TuningInteraction *interaction) {
    interaction->profile_hold_started_ms = 0;
    interaction->profile_hold_active = false;
    interaction->profile_mode_toggled = false;
}

/**
 * @brief Reports whether another profile-selection input owns the menu hold.
 *
 * Excludes navigation, analog selection, profile selectors, and earlier wheel or adapter
 * shortcuts from the timed profile-mode and reset actions.
 *
 * @param[in] input Current attached-wheel and adapter inputs.
 * @return True when a higher-priority profile interaction is active.
 */
static bool profile_hold_blocked(const TuningInteractionInput *input) {
    bool profile_selector_active =
        (input->wheel_mode == WHEEL_MODE_STANDARD || input->wheel_mode == WHEEL_MODE_EXTENDED) &&
        input->profile_selector_active;
    return (input->primary_buttons & 0x0f00u) != 0 || input->analog_scale != 0 ||
           profile_selector_active || profile_shortcut_precedes_adjustment(input);
}

/**
 * @brief Advances the timed profile-mode and reset hold.
 *
 * Emits the mode action once after two seconds and the reset action at ten seconds. A pedal
 * adjustment or higher-priority profile input cancels the timer.
 *
 * @param[in,out] interaction Active menu-hold state.
 * @param[in] input Current attached-wheel and adapter inputs.
 * @param[in] now_ms Current monotonic time in milliseconds.
 * @return Action produced by the held profile selector.
 */
static TuningInteractionAction update_profile_hold(TuningInteraction *interaction,
                                                   const TuningInteractionInput *input,
                                                   uint32_t now_ms) {
    bool adjustment = (input->secondary_buttons & TUNING_SECONDARY_ADJUSTMENT) != 0 &&
                      !profile_shortcut_precedes_adjustment(input);
    if (adjustment) {
        clear_profile_hold(interaction);
        if (!interaction->pedal_adjustment_requested) {
            interaction->pedal_adjustment_requested = true;
            return TUNING_INTERACTION_ACTION_PEDAL_ADJUSTMENT;
        }
        return TUNING_INTERACTION_ACTION_NONE;
    }
    interaction->pedal_adjustment_requested = false;

    if (profile_hold_blocked(input)) {
        clear_profile_hold(interaction);
        return TUNING_INTERACTION_ACTION_NONE;
    }
    if (!interaction->profile_hold_active) {
        interaction->profile_hold_started_ms = now_ms;
        interaction->profile_hold_active = true;
        return TUNING_INTERACTION_ACTION_NONE;
    }

    uint32_t elapsed_ms = now_ms - interaction->profile_hold_started_ms;
    if (elapsed_ms >= TUNING_PROFILE_RESET_HOLD_MS) {
        clear_profile_hold(interaction);
        interaction->closing = true;
        return TUNING_INTERACTION_ACTION_RESET_PROFILES;
    }
    if (elapsed_ms >= TUNING_PROFILE_MODE_HOLD_MS && !interaction->profile_mode_toggled) {
        interaction->profile_mode_toggled = true;
        return TUNING_INTERACTION_ACTION_TOGGLE_PROFILE_MODE;
    }
    return TUNING_INTERACTION_ACTION_NONE;
}

/**
 * @brief Advances one local tuning-menu interaction sample.
 *
 * A first menu press opens profile interaction. Releasing it exposes the tuning entries, and the
 * next menu press closes them. While profile interaction is held, the update emits pedal,
 * two-second profile-mode, and ten-second reset actions. Legacy entries also accept their pedal
 * adjustment chord unless the wheel-center chord owns the input.
 *
 * @param[in,out] interaction Current logical tuning-menu state.
 * @param[in] input Current attached-wheel and adapter inputs.
 * @param[in] now_ms Current monotonic time in milliseconds.
 * @return Actions produced by the current sample.
 */
TuningInteractionAction tuning_interaction_update(TuningInteraction *interaction,
                                                  const TuningInteractionInput *input,
                                                  uint32_t now_ms) {
    if (interaction == NULL || input == NULL) {
        return TUNING_INTERACTION_ACTION_NONE;
    }
    if (!input->available) {
        tuning_interaction_init(interaction);
        return TUNING_INTERACTION_ACTION_NONE;
    }

    TuningNavigationEvent navigation = tuning_interaction_read_navigation(interaction, input);
    interaction->navigation = navigation;
    bool menu_held = navigation.mode == TUNING_NAVIGATION_MENU;
    if (interaction->phase == TUNING_INTERACTION_CLOSED) {
        if (menu_held) {
            interaction->phase = TUNING_INTERACTION_MENU_HELD;
            interaction->closing = false;
            clear_profile_hold(interaction);
        }
        return TUNING_INTERACTION_ACTION_NONE;
    }

    if (interaction->phase == TUNING_INTERACTION_MENU_HELD) {
        if (menu_held) {
            return update_profile_hold(interaction, input, now_ms);
        }
        interaction->phase =
            interaction->closing ? TUNING_INTERACTION_CLOSED : TUNING_INTERACTION_ENTRY_OPEN;
        interaction->closing = false;
        interaction->pedal_adjustment_requested = false;
        clear_profile_hold(interaction);
        return TUNING_INTERACTION_ACTION_NONE;
    }

    if (menu_held) {
        interaction->phase = TUNING_INTERACTION_MENU_HELD;
        interaction->closing = true;
        interaction->pedal_adjustment_requested = false;
        clear_profile_hold(interaction);
        return TUNING_INTERACTION_ACTION_NONE;
    }

    bool adjustment = input->wheel_mode == WHEEL_MODE_LEGACY &&
                      (input->secondary_buttons & TUNING_SECONDARY_LEGACY_ADJUSTMENT) ==
                          TUNING_SECONDARY_LEGACY_ADJUSTMENT &&
                      !legacy_center_chord_held(input);
    if (!adjustment) {
        interaction->pedal_adjustment_requested = false;
        return TUNING_INTERACTION_ACTION_NONE;
    }
    if (interaction->pedal_adjustment_requested) {
        return TUNING_INTERACTION_ACTION_NONE;
    }
    interaction->pedal_adjustment_requested = true;
    return TUNING_INTERACTION_ACTION_PEDAL_ADJUSTMENT;
}
