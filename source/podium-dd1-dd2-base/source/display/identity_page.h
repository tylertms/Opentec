#ifndef OPENTEC_BASE_DISPLAY_IDENTITY_PAGE_H
#define OPENTEC_BASE_DISPLAY_IDENTITY_PAGE_H

#include "board/identity.h"
#include "display/framebuffer.h"

/**
 * @brief Renders the hardware identity page.
 *
 * Clears the framebuffer, draws the Opentec logo, and selects the DD1 or DD2 model text with the
 * PS4 suffix when the hardware option strap is active.
 *
 * @param[in,out] framebuffer Framebuffer receiving the identity page.
 * @param[in] identity Hardware variant and option-strapping information.
 */
void display_identity_page_render(DisplayFramebuffer framebuffer, BoardIdentity identity);

#endif
