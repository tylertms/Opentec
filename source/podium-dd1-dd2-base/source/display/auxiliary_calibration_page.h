#ifndef OPENTEC_BASE_DISPLAY_AUXILIARY_CALIBRATION_PAGE_H
#define OPENTEC_BASE_DISPLAY_AUXILIARY_CALIBRATION_PAGE_H

#include <stdbool.h>
#include <stdint.h>

#include "display/bitmap.h"
#include "display/framebuffer.h"

/**
 * @brief Stores the legacy display state mirrored by the local OLED.
 *
 * The state contains three raw seven-segment glyphs and the general remote-tuning indicator.
 */
typedef struct {
    uint8_t glyphs[3];         /**< Raw seven-segment glyph values, including decimal-point bits. */
    bool remote_tuning_active; /**< Whether the general remote-tuning session is active. */
} DisplayAuxiliaryCalibrationPage;

/**
 * @brief Updates the mirrored legacy display state.
 *
 * Copies all three glyphs and the remote-tuning state, and reports whether any displayed value
 * changed.
 *
 * @param[in,out] page Legacy display state to update.
 * @param[in] glyphs Three raw seven-segment glyph values.
 * @param[in] remote_tuning_active Whether the general remote-tuning session is active.
 * @return True when at least one stored value changed.
 */
bool display_auxiliary_calibration_page_update(DisplayAuxiliaryCalibrationPage *page,
                                               const uint8_t glyphs[3], bool remote_tuning_active);

/**
 * @brief Renders the legacy-display page title.
 *
 * Clears the framebuffer and draws the inverted title at the reference record coordinates.
 *
 * @param[in,out] framebuffer Framebuffer receiving the title pixels.
 */
void display_auxiliary_calibration_page_render_title(DisplayFramebuffer framebuffer);

/**
 * @brief Renders the mirrored legacy display.
 *
 * Clears the framebuffer, draws the three stored seven-segment glyphs, and shows the remote-tuning
 * indicator when the session is active.
 *
 * @param[in,out] framebuffer Framebuffer receiving the rendered page.
 * @param[in] page Legacy display state to render.
 */
void display_auxiliary_calibration_page_render(DisplayFramebuffer framebuffer,
                                               const DisplayAuxiliaryCalibrationPage *page);

/**
 * @brief Renders the mirrored legacy display while queuing glyph bitmap rows.
 *
 * @param[in,out] framebuffer Framebuffer receiving the rendered page.
 * @param[in,out] queue Bitmap queue owned by the framebuffer composition.
 * @param[in] page Legacy display state to render.
 */
void display_auxiliary_calibration_page_render_queued(DisplayFramebuffer framebuffer,
                                                     DisplayBitmapQueue *queue,
                                                     const DisplayAuxiliaryCalibrationPage *page);

#endif
