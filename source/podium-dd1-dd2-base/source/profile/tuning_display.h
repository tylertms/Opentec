#ifndef OPENTEC_BASE_PROFILE_TUNING_DISPLAY_H
#define OPENTEC_BASE_PROFILE_TUNING_DISPLAY_H

#include <stdbool.h>

#include "profile/bank.h"
#include "profile/tuning_menu.h"
#include "wheel/display_output.h"

/**
 * @brief Renders the selected local tuning presentation.
 *
 * Writes the selected entry label or value into the attached-wheel display while preserving other
 * output fields.
 *
 * @param[in] menu Current local tuning selection and view.
 * @param[in] bank Current tuning profile bank.
 * @param[in,out] output Display output receiving the rendered glyphs.
 * @return true when the menu is valid and owns the display; false when inputs are invalid.
 */
bool tuning_display_render(const TuningMenu *menu, const TuningProfileBank *bank,
                           WheelDisplayOutput *output);

/**
 * @brief Copies four tuning-display characters into wheel glyph output.
 *
 * @param[in,out] output Wheel display output receiving the glyphs.
 * @param[in] text Four display characters in transmission order.
 */
void tuning_display_render_text(WheelDisplayOutput *output, const char text[4]);

#endif
