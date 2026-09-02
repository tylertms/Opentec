#ifndef OPENTEC_BASE_DISPLAY_SHIFTER_PAGE_H
#define OPENTEC_BASE_DISPLAY_SHIFTER_PAGE_H

#include "display/framebuffer.h"
#include "shifter/display.h"

/**
 * @brief Renders the local OLED shifter presentation.
 *
 * Gear glyphs use the page-zero middle segment position. Calibration entry prompts use the
 * official SFT, CAL, and waiting layouts, while position capture uses the official diagnostic
 * title, instruction, and frame records.
 *
 * @param[in,out] framebuffer Complete local-display framebuffer.
 * @param[in] presentation Current shifter presentation; null renders a clear page.
 */
void display_shifter_page_render(DisplayFramebuffer framebuffer,
                                 const ShifterLocalDisplay *presentation);

#endif
