#ifndef OPENTEC_BASE_DISPLAY_HIGH_TORQUE_ICON_H
#define OPENTEC_BASE_DISPLAY_HIGH_TORQUE_ICON_H

#include <stdint.h>

#include "display/framebuffer.h"

/**
 * @brief Draws the high-torque safety icon.
 *
 * Expands the built-in eleven-by-ten grayscale bitmap at the requested framebuffer position.
 *
 * @param[in,out] framebuffer Framebuffer receiving the icon pixels.
 * @param[in] x Left icon coordinate.
 * @param[in] y Top icon coordinate.
 */
void display_high_torque_icon_draw(DisplayFramebuffer framebuffer, uint16_t x, uint16_t y);

#endif
