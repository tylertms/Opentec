#ifndef OPENTEC_BASE_DISPLAY_AUXILIARY_CALIBRATION_PAGE_H
#define OPENTEC_BASE_DISPLAY_AUXILIARY_CALIBRATION_PAGE_H

#include <stdbool.h>
#include <stdint.h>

#include "display/framebuffer.h"

/** @brief Mirrored three-glyph legacy display and remote-tuning indicator. */
typedef struct {
    uint8_t glyphs[3];
    bool remote_tuning_active;
} DisplayAuxiliaryCalibrationPage;

bool display_auxiliary_calibration_page_update(DisplayAuxiliaryCalibrationPage *page,
                                               const uint8_t glyphs[3], bool remote_tuning_active);
void display_auxiliary_calibration_page_render_title(DisplayFramebuffer framebuffer);
void display_auxiliary_calibration_page_render(DisplayFramebuffer framebuffer,
                                               const DisplayAuxiliaryCalibrationPage *page);

#endif
