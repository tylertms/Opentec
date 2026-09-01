#ifndef OPENTEC_BASE_DISPLAY_IDENTITY_LOGO_H
#define OPENTEC_BASE_DISPLAY_IDENTITY_LOGO_H

#include <stdint.h>

#include "display/framebuffer.h"

/**
 * @brief Draws the Opentec identity logo.
 *
 * Expands the built-in 256-by-26 grayscale logo bitmap at the requested framebuffer position.
 *
 * @param[in,out] framebuffer Framebuffer receiving the logo pixels.
 * @param[in] x Left logo coordinate.
 * @param[in] y Top logo coordinate.
 */
void display_identity_logo_draw(DisplayFramebuffer framebuffer, uint16_t x, uint16_t y);

#endif
