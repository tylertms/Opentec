#include "profile/tuning_interaction.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/** @brief Internal wheel, button, and adapter masks for tuning interaction. */
enum {
    WHEEL_MODE_LEGACY = 0x0e,                        /**< Legacy wheel mode. */
    WHEEL_MODE_LEGACY_ALTERNATE = 0x0f,              /**< Alternate legacy wheel mode. */
    WHEEL_MODE_STANDARD = 0x10,                      /**< Standard wheel mode. */
    WHEEL_MODE_STANDARD_ALTERNATE = 0x11,            /**< Alternate standard wheel mode. */
    WHEEL_MODE_LEGACY_COMPATIBILITY = 0x17,          /**< Legacy compatibility mode. */
    WHEEL_MODE_PULSE_INPUT = 0x1b,                   /**< Pulse-input wheel mode. */
    WHEEL_MODE_EXTENDED = 0x1c,                      /**< Extended wheel mode. */
    TUNING_PRIMARY_INCREASE = 0x0100,                /**< Primary increase button. */
    TUNING_PRIMARY_PREVIOUS = 0x0200,                /**< Primary previous button. */
    TUNING_PRIMARY_NEXT = 0x0400,                    /**< Primary next button. */
    TUNING_PRIMARY_DECREASE = 0x0800,                /**< Primary decrease button. */
    TUNING_PRIMARY_STANDARD_CENTER = 0x1000,         /**< Primary standard center button. */
    TUNING_PRIMARY_LEGACY_CENTER = 0x4000,           /**< Primary legacy center button. */
    TUNING_SECONDARY_PEDAL_CHORD = 0x0009,           /**< Pedal-operation chord. */
    TUNING_SECONDARY_LEGACY_CENTER = 0x0010,         /**< Legacy center button. */
    TUNING_SECONDARY_PULSE_CENTER = 0x0012,          /**< Pulse center chord. */
    TUNING_SECONDARY_ADJUSTMENT = 0x0040,            /**< Profile adjustment chord. */
    TUNING_SECONDARY_STANDARD_CENTER = 0x0080,       /**< Standard center button. */
    TUNING_SECONDARY_PROFILE_SHORTCUT = 0x0100,      /**< Profile shortcut. */
    TUNING_SECONDARY_TOGGLE_VIEW = 0x0200,           /**< Toggle-view button. */
    TUNING_SECONDARY_CENTER_PAIR = 0x0600,           /**< Standard center button pair. */
    TUNING_SECONDARY_MENU = 0x2000,                  /**< Menu button. */
    TUNING_SECONDARY_LEGACY_DISPLAY = 0x0110,        /**< Legacy display shortcut. */
    TUNING_SECONDARY_LEGACY_ADJUSTMENT = 0x0140,     /**< Legacy adjustment shortcut. */
    TUNING_ADAPTER_EXTENDED_CENTER_PRIMARY = 0x10,   /**< Extended adapter center primary bit. */
    TUNING_ADAPTER_EXTENDED_CENTER_SECONDARY = 0x04, /**< Extended adapter center secondary bit. */
    TUNING_ADAPTER_STANDARD_CENTER = 0x0c,           /**< Standard adapter center chord. */
};

/**
 * @brief Reports whether a button mask is completely asserted.
 *
 * Applies the inclusive chord tests used by attached-wheel tuning shortcuts.
 *
 * @param[in] buttons Current button word.
 * @param[in] mask Required button bits.
 * @return true when every required bit is asserted; false otherwise.
 */
static bool buttons_include(uint16_t buttons, uint16_t mask) { return (buttons & mask) == mask; }

/**
 * @brief Reports whether an earlier profile shortcut owns the held menu input.
 *
 * Applies the attached-wheel shortcut priority that precedes the pedal end-stop query.
 *
 * @param[in] input Current attached-wheel and adapter inputs.
 * @return true when a higher-priority shortcut suppresses the pedal query; false otherwise.
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
 * @return true while the release gate remains asserted; false otherwise.
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
 * @return true when the active adapter layout carries its complete center chord; false otherwise.
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
 * @return Center-presentation action for a legacy chord; no action for a nonlegacy or absent
 * center chord.
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

void tuning_interaction_init(TuningInteraction *interaction) {
    if (interaction != NULL) {
        *interaction = (TuningInteraction){0};
    }
}

void tuning_interaction_request_close(TuningInteraction *interaction) {
    if (interaction == NULL) {
        return;
    }
    interaction->phase = TUNING_INTERACTION_CLOSING;
    interaction->closing = true;
    interaction->navigation = (TuningNavigationEvent){0};
}

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
 * @return true when a higher-priority profile interaction is active; false otherwise.
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
 * @return Shortcut, mode-toggle, reset, or no action according to the current held input.
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
 * @return true when a V3 operation state was entered; false otherwise.
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
 * @return Pedal control or completion action when its phase reaches a transition; no action while
 * waiting.
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
 * @return Combined shortcut action bits, or no action when no legacy shortcut is active.
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

bool tuning_interaction_suppresses_host_input(const TuningInteraction *interaction) {
    return interaction != NULL && (interaction->phase == TUNING_INTERACTION_ENTRY_OPEN ||
                                   interaction->phase == TUNING_INTERACTION_CENTER_CAPTURE ||
                                   interaction->phase == TUNING_INTERACTION_MENU_HELD);
}

bool tuning_interaction_suppresses_system_button(const TuningInteraction *interaction) {
    return interaction != NULL && (interaction->phase == TUNING_INTERACTION_ENTRY_OPEN ||
                                   interaction->phase == TUNING_INTERACTION_CENTER_CAPTURE);
}

bool tuning_interaction_blocks_adapter_synchronization(const TuningInteraction *interaction) {
    return interaction != NULL && (interaction->phase == TUNING_INTERACTION_ENTRY_OPEN ||
                                   interaction->phase == TUNING_INTERACTION_MENU_HELD ||
                                   interaction->phase == TUNING_INTERACTION_CLOSING);
}
