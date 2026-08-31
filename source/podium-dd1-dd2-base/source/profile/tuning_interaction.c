#include "profile/tuning_interaction.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

enum {
    WHEEL_MODE_LEGACY = 0x0e,
    WHEEL_MODE_LEGACY_ALTERNATE = 0x0f,
    WHEEL_MODE_STANDARD = 0x10,
    WHEEL_MODE_STANDARD_ALTERNATE = 0x11,
    WHEEL_MODE_LEGACY_COMPATIBILITY = 0x17,
    WHEEL_MODE_PULSE_INPUT = 0x1b,
    WHEEL_MODE_EXTENDED = 0x1c,
    TUNING_PRIMARY_INCREASE = 0x0100,
    TUNING_PRIMARY_PREVIOUS = 0x0200,
    TUNING_PRIMARY_NEXT = 0x0400,
    TUNING_PRIMARY_DECREASE = 0x0800,
    TUNING_PRIMARY_STANDARD_CENTER = 0x1000,
    TUNING_PRIMARY_LEGACY_CENTER = 0x4000,
    TUNING_SECONDARY_PEDAL_CHORD = 0x0009,
    TUNING_SECONDARY_LEGACY_CENTER = 0x0010,
    TUNING_SECONDARY_PULSE_CENTER = 0x0012,
    TUNING_SECONDARY_ADJUSTMENT = 0x0040,
    TUNING_SECONDARY_STANDARD_CENTER = 0x0080,
    TUNING_SECONDARY_PROFILE_SHORTCUT = 0x0100,
    TUNING_SECONDARY_TOGGLE_VIEW = 0x0200,
    TUNING_SECONDARY_CENTER_PAIR = 0x0600,
    TUNING_SECONDARY_MENU = 0x2000,
    TUNING_SECONDARY_LEGACY_DISPLAY = 0x0110,
    TUNING_SECONDARY_LEGACY_ADJUSTMENT = 0x0140,
    TUNING_ADAPTER_EXTENDED_CENTER_PRIMARY = 0x10,
    TUNING_ADAPTER_EXTENDED_CENTER_SECONDARY = 0x04,
    TUNING_ADAPTER_STANDARD_CENTER = 0x0c,
};

/**
 * @brief Reports whether a button mask is completely asserted.
 *
 * Applies the inclusive chord tests used by attached-wheel tuning shortcuts.
 *
 * @param[in] buttons Current button word.
 * @param[in] mask Required button bits.
 * @return True when every required bit is asserted.
 */
static bool buttons_include(uint16_t buttons, uint16_t mask) { return (buttons & mask) == mask; }

/**
 * @brief Reports whether an earlier profile shortcut owns the held menu input.
 *
 * Applies the attached-wheel shortcut priority that precedes the pedal end-stop query.
 *
 * @param[in] input Current attached-wheel and adapter inputs.
 * @return True when a higher-priority shortcut suppresses the pedal query.
 */
static bool profile_shortcut_precedes_adjustment(const TuningInteractionInput *input) {
    bool legacy_profile_shortcut =
        (input->wheel_mode == WHEEL_MODE_LEGACY_ALTERNATE ||
         input->wheel_mode == WHEEL_MODE_LEGACY_COMPATIBILITY) &&
        buttons_include(input->secondary_buttons, TUNING_SECONDARY_PROFILE_SHORTCUT);
    bool adapter_profile_shortcut =
        input->adapter_profile_shortcut || (input->adapter_connected && input->adapter_mode == 1 &&
                                            (input->adapter_buttons[1] & 0x01u) != 0);
    return legacy_profile_shortcut || adapter_profile_shortcut ||
           buttons_include(input->secondary_buttons, TUNING_SECONDARY_STANDARD_CENTER);
}

/**
 * @brief Reports whether the center-release gate remains asserted.
 *
 * Preserves the reference release predicate for the standard center pair, the ordinary standard
 * chord, and the mode-0x11 legacy-layout chord.
 *
 * @param[in] input Current attached-wheel input.
 * @return True while the release gate remains asserted.
 */
static bool default_center_chord_held(const TuningInteractionInput *input) {
    return buttons_include(input->secondary_buttons, TUNING_SECONDARY_CENTER_PAIR) ||
           (buttons_include(input->secondary_buttons, TUNING_SECONDARY_STANDARD_CENTER) &&
            buttons_include(input->primary_buttons, TUNING_PRIMARY_STANDARD_CENTER)) ||
           (input->wheel_mode == WHEEL_MODE_STANDARD_ALTERNATE &&
            buttons_include(input->secondary_buttons, TUNING_SECONDARY_LEGACY_CENTER) &&
            buttons_include(input->primary_buttons, TUNING_PRIMARY_LEGACY_CENTER));
}

/**
 * @brief Reports whether an attached adapter requests wheel-center capture.
 *
 * Selects the distinct standard and extended adapter button layouts.
 *
 * @param[in] input Current attached-adapter state.
 * @return True when the active adapter layout carries its complete center chord.
 */
static bool adapter_center_requested(const TuningInteractionInput *input) {
    if (!input->adapter_connected) {
        return false;
    }
    if (input->adapter_mode == 1) {
        return (input->adapter_buttons[0] & TUNING_ADAPTER_EXTENDED_CENTER_PRIMARY) != 0 &&
               (input->adapter_buttons[1] & TUNING_ADAPTER_EXTENDED_CENTER_SECONDARY) != 0;
    }
    return (input->adapter_buttons[2] & TUNING_ADAPTER_STANDARD_CENTER) ==
           TUNING_ADAPTER_STANDARD_CENTER;
}

/**
 * @brief Starts center capture for any supported wheel or adapter chord.
 *
 * Distinguishes legacy, extended, pulse, default, and adapter layouts. Legacy layouts additionally
 * request the reference completion command when the capture state opens.
 *
 * @param[in,out] interaction Interaction state to advance.
 * @param[in] input Current attached-wheel and adapter inputs.
 * @return Center-presentation action, or no action when no center chord is active.
 */
static TuningInteractionAction start_center_capture(TuningInteraction *interaction,
                                                    const TuningInteractionInput *input) {
    bool legacy = input->wheel_mode == WHEEL_MODE_LEGACY ||
                  input->wheel_mode == WHEEL_MODE_LEGACY_ALTERNATE ||
                  input->wheel_mode == WHEEL_MODE_LEGACY_COMPATIBILITY;
    bool requested;
    if (legacy) {
        requested = buttons_include(input->secondary_buttons, TUNING_SECONDARY_LEGACY_CENTER) &&
                    buttons_include(input->primary_buttons, TUNING_PRIMARY_LEGACY_CENTER);
    } else if (input->wheel_mode == WHEEL_MODE_EXTENDED) {
        requested = buttons_include(input->secondary_buttons, TUNING_SECONDARY_TOGGLE_VIEW) &&
                    input->auxiliary_report[2] == 2;
    } else if (input->wheel_mode == WHEEL_MODE_PULSE_INPUT) {
        requested = buttons_include(input->secondary_buttons, TUNING_SECONDARY_PULSE_CENTER);
    } else {
        requested = default_center_chord_held(input);
    }
    requested |= adapter_center_requested(input);
    if (!requested) {
        return TUNING_INTERACTION_ACTION_NONE;
    }
    interaction->phase = TUNING_INTERACTION_CENTER_CAPTURE;
    interaction->navigation = (TuningNavigationEvent){0};
    return legacy ? TUNING_INTERACTION_ACTION_SHOW_CENTER_CAPTURE : TUNING_INTERACTION_ACTION_NONE;
}

/**
 * @brief Initializes tuning-menu interaction state.
 *
 * Starts with the profile selector and tuning entries closed.
 *
 * @param[out] interaction Interaction state to initialize.
 */
void tuning_interaction_init(TuningInteraction *interaction) {
    if (interaction != NULL) {
        *interaction = (TuningInteraction){0};
    }
}

/**
 * @brief Requests the local tuning menu close phase.
 *
 * Preserves the interaction state needed by the normal close service while marking the menu as
 * closing and discarding pending navigation.
 *
 * @param[in,out] interaction Local tuning interaction to close.
 */
void tuning_interaction_request_close(TuningInteraction *interaction) {
    if (interaction == NULL) {
        return;
    }
    interaction->phase = TUNING_INTERACTION_CLOSING;
    interaction->closing = true;
    interaction->navigation = (TuningNavigationEvent){0};
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

    TuningNavigationMode sampled_mode = event.mode;
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
 * @brief Applies profile-menu shortcuts in their reference priority.
 *
 * Produces the normal or extended shifter request before the pedal end-stop query and preserves a
 * single query while its chord remains held.
 *
 * @param[in,out] interaction Active profile-menu state.
 * @param[in] input Current attached-wheel and adapter inputs.
 * @return Shortcut actions produced by the current sample.
 */
static TuningInteractionAction profile_shortcut_action(TuningInteraction *interaction,
                                                       const TuningInteractionInput *input) {
    bool legacy_shifter =
        (input->wheel_mode == WHEEL_MODE_LEGACY_ALTERNATE ||
         input->wheel_mode == WHEEL_MODE_LEGACY_COMPATIBILITY) &&
        buttons_include(input->secondary_buttons, TUNING_SECONDARY_PROFILE_SHORTCUT);
    bool adapter_shifter = input->adapter_connected && input->adapter_mode == 1 &&
                           (input->adapter_buttons[1] & 0x01u) != 0;
    if (legacy_shifter || adapter_shifter) {
        interaction->pedal_adjustment_requested = false;
        return TUNING_INTERACTION_ACTION_SHOW_SHIFTER;
    }
    if (buttons_include(input->secondary_buttons, TUNING_SECONDARY_STANDARD_CENTER)) {
        interaction->pedal_adjustment_requested = false;
        return input->wheel_mode == WHEEL_MODE_EXTENDED
                   ? TUNING_INTERACTION_ACTION_SHOW_EXTENDED_SHIFTER
                   : TUNING_INTERACTION_ACTION_SHOW_SHIFTER;
    }
    bool adjustment = buttons_include(input->secondary_buttons, TUNING_SECONDARY_ADJUSTMENT);
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

/**
 * @brief Advances the timed profile-mode and reset hold.
 *
 * Emits the mode action once after two seconds and enters the two-second reset-result phase at ten
 * seconds. Higher-priority profile inputs restart the timer.
 *
 * @param[in,out] interaction Active menu-hold state.
 * @param[in] input Current attached-wheel and adapter inputs.
 * @param[in] now_ms Current monotonic time in milliseconds.
 * @return Action produced by the held profile selector.
 */
static TuningInteractionAction update_profile_hold(TuningInteraction *interaction,
                                                   const TuningInteractionInput *input,
                                                   uint32_t now_ms) {
    TuningInteractionAction shortcut = profile_shortcut_action(interaction, input);
    if (shortcut != TUNING_INTERACTION_ACTION_NONE) {
        clear_profile_hold(interaction);
        return shortcut;
    }
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
        interaction->phase = TUNING_INTERACTION_RESET_RESULT;
        interaction->result_deadline_ms = now_ms + TUNING_PROFILE_RESET_RESULT_MS;
        interaction->pedal_adjustment_requested = false;
        return TUNING_INTERACTION_ACTION_RESET_PROFILES;
    }
    if (elapsed_ms >= TUNING_PROFILE_MODE_HOLD_MS && !interaction->profile_mode_toggled) {
        interaction->profile_mode_toggled = true;
        return TUNING_INTERACTION_ACTION_TOGGLE_PROFILE_MODE;
    }
    return TUNING_INTERACTION_ACTION_NONE;
}

/**
 * @brief Starts one local V3 pedal calibration operation.
 *
 * Requires the initial tuning-entry view, a connected legacy-calibration pedal path, the complete
 * secondary chord, and one of the three primary operation buttons.
 *
 * @param[in,out] interaction Tuning interaction to advance.
 * @param[in] input Current attached-wheel and pedal capability state.
 * @return True when a V3 operation state was entered.
 */
static bool start_pedal_operation(TuningInteraction *interaction,
                                  const TuningInteractionInput *input) {
    if (!input->entry_showing_label || !input->legacy_pedal_calibration_available ||
        !buttons_include(input->secondary_buttons, TUNING_SECONDARY_PEDAL_CHORD)) {
        return false;
    }
    if ((input->primary_buttons & TUNING_PRIMARY_INCREASE) != 0) {
        interaction->phase = TUNING_INTERACTION_PEDAL_UP;
    } else if ((input->primary_buttons & TUNING_PRIMARY_DECREASE) != 0) {
        interaction->phase = TUNING_INTERACTION_PEDAL_DOWN;
    } else if ((input->primary_buttons & TUNING_PRIMARY_PREVIOUS) != 0) {
        interaction->phase = TUNING_INTERACTION_PEDAL_AUTOMATIC;
    } else {
        return false;
    }
    interaction->pedal_operation_sent = false;
    interaction->result_deadline_ms = 0;
    interaction->navigation = (TuningNavigationEvent){0};
    return true;
}

/**
 * @brief Advances a release-gated V3 pedal operation and result interval.
 *
 * Waits for the initiating chord to release, emits the selected control once, waits for its queued
 * operation to drain, then presents the result for two seconds before returning to the first entry
 * view.
 *
 * @param[in,out] interaction Active V3 pedal operation.
 * @param[in] input Current wheel chord and pedal operation state.
 * @param[in] now_ms Current monotonic time in milliseconds.
 * @return Pedal control or result action produced by this update.
 */
static TuningInteractionAction update_pedal_operation(TuningInteraction *interaction,
                                                      const TuningInteractionInput *input,
                                                      uint32_t now_ms) {
    uint16_t held_mask;
    TuningInteractionAction start_action;
    TuningInteractionAction complete_action;
    if (interaction->phase == TUNING_INTERACTION_PEDAL_UP) {
        held_mask = TUNING_PRIMARY_INCREASE;
        start_action = TUNING_INTERACTION_ACTION_PEDAL_UP;
        complete_action = TUNING_INTERACTION_ACTION_PEDAL_UP_COMPLETE;
    } else if (interaction->phase == TUNING_INTERACTION_PEDAL_DOWN) {
        held_mask = TUNING_PRIMARY_DECREASE;
        start_action = TUNING_INTERACTION_ACTION_PEDAL_DOWN;
        complete_action = TUNING_INTERACTION_ACTION_PEDAL_DOWN_COMPLETE;
    } else {
        held_mask = TUNING_PRIMARY_PREVIOUS;
        start_action = TUNING_INTERACTION_ACTION_PEDAL_AUTOMATIC;
        complete_action = TUNING_INTERACTION_ACTION_PEDAL_AUTOMATIC_COMPLETE;
    }

    if (!interaction->pedal_operation_sent) {
        bool held = buttons_include(input->secondary_buttons, TUNING_SECONDARY_PEDAL_CHORD) &&
                    (input->primary_buttons & held_mask) != 0;
        if (held) {
            return TUNING_INTERACTION_ACTION_NONE;
        }
        interaction->pedal_operation_sent = true;
        return start_action;
    }
    if (interaction->result_deadline_ms == 0) {
        if (input->pedal_operation_pending) {
            return TUNING_INTERACTION_ACTION_NONE;
        }
        interaction->result_deadline_ms = now_ms + TUNING_PEDAL_RESULT_MS;
        return complete_action;
    }
    if ((int32_t)(now_ms - interaction->result_deadline_ms) >= 0) {
        interaction->phase = TUNING_INTERACTION_ENTRY_OPEN;
        interaction->pedal_operation_sent = false;
        interaction->result_deadline_ms = 0;
    }
    return TUNING_INTERACTION_ACTION_NONE;
}

/**
 * @brief Applies entry-open legacy display and end-stop shortcuts.
 *
 * Allows the legacy display and pedal overlay chords to produce independent actions so their
 * combined chord retains both effects.
 *
 * @param[in,out] interaction Current interaction and pedal-query edge latch.
 * @param[in] input Current attached-wheel input.
 * @return Combined shortcut actions for the current sample.
 */
static TuningInteractionAction entry_shortcut_action(TuningInteraction *interaction,
                                                     const TuningInteractionInput *input) {
    if (input->wheel_mode != WHEEL_MODE_LEGACY) {
        interaction->pedal_adjustment_requested = false;
        return TUNING_INTERACTION_ACTION_NONE;
    }
    TuningInteractionAction actions = TUNING_INTERACTION_ACTION_NONE;
    if (buttons_include(input->secondary_buttons, TUNING_SECONDARY_LEGACY_DISPLAY)) {
        actions = (TuningInteractionAction)(actions | TUNING_INTERACTION_ACTION_SHOW_SHIFTER);
    }
    bool adjustment = buttons_include(input->secondary_buttons, TUNING_SECONDARY_LEGACY_ADJUSTMENT);
    if (adjustment && !interaction->pedal_adjustment_requested) {
        actions = (TuningInteractionAction)(actions | TUNING_INTERACTION_ACTION_PEDAL_ADJUSTMENT);
    }
    interaction->pedal_adjustment_requested = adjustment;
    return actions;
}

/**
 * @brief Advances one local tuning-menu interaction sample.
 *
 * Implements release-gated center capture, local V3 pedal operations, profile shortcuts, reset
 * result and cleanup phases, and extended-wheel reconnect grace while keeping the menu and its
 * consumers separated from device transport details.
 *
 * @param[in,out] interaction Current logical tuning-menu state.
 * @param[in] input Current attached-wheel, adapter, and pedal inputs.
 * @param[in] now_ms Current monotonic time in milliseconds.
 * @return Bitwise actions produced by the current sample.
 */
TuningInteractionAction tuning_interaction_update(TuningInteraction *interaction,
                                                  const TuningInteractionInput *input,
                                                  uint32_t now_ms) {
    if (interaction == NULL || input == NULL) {
        return TUNING_INTERACTION_ACTION_NONE;
    }
    if (!input->available) {
        if (interaction->phase == TUNING_INTERACTION_MENU_HELD &&
            input->wheel_mode == WHEEL_MODE_EXTENDED) {
            if (!interaction->reconnect_active) {
                interaction->reconnect_active = true;
                interaction->reconnect_started_ms = now_ms;
            }
            if (now_ms - interaction->reconnect_started_ms < TUNING_RECONNECT_GRACE_MS) {
                interaction->last_navigation = TUNING_NAVIGATION_NONE;
                interaction->navigation = (TuningNavigationEvent){0};
                return TUNING_INTERACTION_ACTION_NONE;
            }
        }
        tuning_interaction_init(interaction);
        return TUNING_INTERACTION_ACTION_NONE;
    }
    interaction->reconnect_active = false;
    interaction->reconnect_started_ms = 0;

    if (interaction->phase == TUNING_INTERACTION_RESET_RESULT) {
        if ((int32_t)(now_ms - interaction->result_deadline_ms) >= 0) {
            interaction->phase = TUNING_INTERACTION_CLOSING;
        }
        return TUNING_INTERACTION_ACTION_NONE;
    }
    if (interaction->phase == TUNING_INTERACTION_CLOSING) {
        tuning_interaction_init(interaction);
        return TUNING_INTERACTION_ACTION_NONE;
    }
    if (interaction->phase >= TUNING_INTERACTION_PEDAL_UP &&
        interaction->phase <= TUNING_INTERACTION_PEDAL_AUTOMATIC) {
        return update_pedal_operation(interaction, input, now_ms);
    }
    if (interaction->phase == TUNING_INTERACTION_CENTER_CAPTURE) {
        if (!default_center_chord_held(input)) {
            interaction->phase = TUNING_INTERACTION_ENTRY_OPEN;
            return TUNING_INTERACTION_ACTION_CAPTURE_CENTER;
        }
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
            interaction->closing ? TUNING_INTERACTION_CLOSING : TUNING_INTERACTION_ENTRY_OPEN;
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

    TuningInteractionAction center_action = start_center_capture(interaction, input);
    if (interaction->phase == TUNING_INTERACTION_CENTER_CAPTURE) {
        return center_action;
    }
    TuningInteractionAction shortcuts = entry_shortcut_action(interaction, input);
    if (start_pedal_operation(interaction, input)) {
        return shortcuts;
    }
    return shortcuts;
}

/**
 * @brief Reports whether tuning controls must be hidden from ordinary host input.
 *
 * Covers the two entry views, center-release wait, and held profile interaction represented by the
 * modern phase model.
 *
 * @param[in] interaction Current tuning interaction.
 * @return True while ordinary host controls require tuning suppression.
 */
bool tuning_interaction_suppresses_host_input(const TuningInteraction *interaction) {
    return interaction != NULL && (interaction->phase == TUNING_INTERACTION_ENTRY_OPEN ||
                                   interaction->phase == TUNING_INTERACTION_CENTER_CAPTURE ||
                                   interaction->phase == TUNING_INTERACTION_MENU_HELD);
}

/**
 * @brief Reports whether the console system button must be hidden during tuning.
 *
 * Matches the reference states for the entry views and center-release wait while leaving the held
 * profile phase available to its distinct host rule.
 *
 * @param[in] interaction Current tuning interaction.
 * @return True when the console system button must be suppressed.
 */
bool tuning_interaction_suppresses_system_button(const TuningInteraction *interaction) {
    return interaction != NULL && (interaction->phase == TUNING_INTERACTION_ENTRY_OPEN ||
                                   interaction->phase == TUNING_INTERACTION_CENTER_CAPTURE);
}

/**
 * @brief Reports whether 3.9.1.1 blocks adapter-active synchronization.
 *
 * Maps the official label/value, profile-held, and close-cleanup states to their clean modern
 * equivalents while allowing center capture, reset result, and pedal-operation phases.
 *
 * @param[in] interaction Current tuning interaction.
 * @return True only for phases corresponding to official states 1, 2, 4, and 6.
 */
bool tuning_interaction_blocks_adapter_synchronization(const TuningInteraction *interaction) {
    return interaction != NULL && (interaction->phase == TUNING_INTERACTION_ENTRY_OPEN ||
                                   interaction->phase == TUNING_INTERACTION_MENU_HELD ||
                                   interaction->phase == TUNING_INTERACTION_CLOSING);
}
