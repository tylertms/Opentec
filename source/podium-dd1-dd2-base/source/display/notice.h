#ifndef OPENTEC_BASE_DISPLAY_NOTICE_H
#define OPENTEC_BASE_DISPLAY_NOTICE_H

#include <stdbool.h>
#include <stdint.h>

#include "display/framebuffer.h"
#include "system/notice.h"

/**
 * @brief Renders the persistent torque-disabled notice.
 *
 * Clears the framebuffer and, when visible, draws the official filled white panel, inverted
 * warning bitmap, and centered Font10 power-button message.
 *
 * @param[in,out] framebuffer Framebuffer receiving the notice.
 * @param[in] visible Whether the notice should be drawn after clearing the framebuffer.
 */
void display_notice_render_torque_disabled(DisplayFramebuffer framebuffer, bool visible);

/**
 * @brief Renders a system notice.
 *
 * Clears the framebuffer and draws the official filled or outlined overlay, including the
 * one-pixel ring centered at (128, 21) for outlined wheel warnings, icon, and Font10 message
 * selected by the supplied notice kind; the none kind leaves the framebuffer clear.
 *
 * @param[in,out] framebuffer Framebuffer receiving the notice.
 * @param[in] kind System notice kind to render.
 */
void display_notice_render_system(DisplayFramebuffer framebuffer, SystemNoticeKind kind);

#endif
