#include "profile/tuning_interaction.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

enum {
    WHEEL_MODE_LEGACY = 0x0e,
    WHEEL_MODE_LEGACY_ALTERNATE = 0x0f,
    WHEEL_MODE_LEGACY_COMPATIBILITY = 0x17,
    TUNING_PRIMARY_LEGACY_CENTER = 0x4000,
    TUNING_SECONDARY_LEGACY_CENTER = 0x0010,
    TUNING_SECONDARY_ADJUSTMENT = 0x0040,
    TUNING_SECONDARY_STANDARD_CENTER = 0x0080,
    TUNING_SECONDARY_PROFILE_SHORTCUT = 0x0100,
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
 * @brief Advances the tuning-menu phase and detects a wheel-side pedal-adjustment request.
 *
 * A first menu press enters profile selection. Button 0x40 requests adjustment while that press
 * remains held, subject to earlier profile shortcuts. Releasing an opening press exposes the
 * tuning entries, where legacy mode accepts chord 0x0140 unless its center chord is also held.
 * The next menu press closes the entry view after release.
 *
 * @param[in,out] interaction Current logical tuning-menu phase.
 * @param[in] input Current attached-wheel and adapter inputs.
 * @return True when the current input requests a pedal-adjustment query.
 */
bool tuning_interaction_requests_pedal_adjustment(TuningInteraction *interaction,
                                                  const TuningInteractionInput *input) {
    if (interaction == NULL || input == NULL) {
        return false;
    }
    if (!input->available) {
        tuning_interaction_init(interaction);
        return false;
    }

    bool menu_held = (input->secondary_buttons & TUNING_SECONDARY_MENU) != 0;
    if (interaction->phase == TUNING_INTERACTION_CLOSED) {
        if (menu_held) {
            interaction->phase = TUNING_INTERACTION_MENU_HELD;
            interaction->closing = false;
        }
        return false;
    }

    if (interaction->phase == TUNING_INTERACTION_MENU_HELD) {
        if (menu_held) {
            return (input->secondary_buttons & TUNING_SECONDARY_ADJUSTMENT) != 0 &&
                   !profile_shortcut_precedes_adjustment(input);
        }
        interaction->phase =
            interaction->closing ? TUNING_INTERACTION_CLOSED : TUNING_INTERACTION_ENTRY_OPEN;
        interaction->closing = false;
        return false;
    }

    bool requested = input->wheel_mode == WHEEL_MODE_LEGACY &&
                     (input->secondary_buttons & TUNING_SECONDARY_LEGACY_ADJUSTMENT) ==
                         TUNING_SECONDARY_LEGACY_ADJUSTMENT &&
                     !legacy_center_chord_held(input);
    if (menu_held) {
        interaction->phase = TUNING_INTERACTION_MENU_HELD;
        interaction->closing = true;
    }
    return requested;
}
