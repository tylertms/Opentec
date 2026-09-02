#ifndef OPENTEC_BASE_DISPLAY_TUNING_PAGE_H
#define OPENTEC_BASE_DISPLAY_TUNING_PAGE_H

#include <stdbool.h>

#include "display/framebuffer.h"
#include "profile/bank.h"
#include "profile/tuning_menu.h"

/**
 * @brief Defines the local tuning value buffer capacity.
 *
 * The value buffer reserves one byte for its terminator and stores up to 23 display characters.
 */
enum { TUNING_PAGE_VALUE_SIZE = 24 /**< Number of bytes reserved for a tuning value. */ };

/**
 * @brief Stores text selected for one local tuning page.
 *
 * The pointers identify catalog text while the value buffer contains the formatted current
 * setting.
 */
typedef struct {
    const char *label;       /**< Short entry label shown at the top of the page. */
    const char *title;       /**< Entry title or setup name shown as the primary label. */
    const char *description; /**< Entry description shown below the primary label. */
    char value[TUNING_PAGE_VALUE_SIZE]; /**< Formatted current setting text. */
} TuningPageContent;

/**
 * @brief Builds the selected local tuning page content.
 *
 * Validates the selected menu entry and profile, then fills the label, title, description, and
 * formatted value for the selected tuning setting.
 *
 * @param[in] menu Current local tuning selection and view. The base OLED record uses Font10 at
 * (13,13) for the abbreviation and Font21 at (30,30) for the primary field.
 * @param[in] bank Current tuning profile bank.
 * @param[out] content Page content receiving the selected text.
 * @return True when valid page content was built.
 */
bool display_tuning_page_present(const TuningMenu *menu, const TuningProfileBank *bank,
                                 TuningPageContent *content);

/**
 * @brief Renders the selected local tuning page.
 *
 * Builds the selected page content, clears the framebuffer, and draws the official navigation,
 * abbreviation, primary field, optional progress, and help records.
 *
 * @param[in,out] framebuffer Framebuffer receiving the tuning page.
 * @param[in] menu Current local tuning selection and view.
 * @param[in] bank Current tuning profile bank.
 * @return True when the selected page was rendered.
 */
bool display_tuning_page_render(DisplayFramebuffer framebuffer, const TuningMenu *menu,
                                const TuningProfileBank *bank);

/**
 * @brief Renders a completed pedal operation result.
 *
 * Clears the framebuffer and draws the result in the official Font21 primary record for a
 * pedal-up, pedal-down, or automatic interaction phase.
 *
 * @param[in,out] framebuffer Framebuffer receiving the operation result.
 * @param[in] phase Pedal interaction phase to render.
 * @return True when the phase identifies a renderable operation result.
 */
bool display_tuning_operation_render(DisplayFramebuffer framebuffer, TuningInteractionPhase phase);

#endif
