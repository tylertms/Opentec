#ifndef OPENTEC_BASE_DISPLAY_IDENTITY_PAGE_H
#define OPENTEC_BASE_DISPLAY_IDENTITY_PAGE_H

#include "board/identity.h"
#include "display/framebuffer.h"

void display_identity_page_render(DisplayFramebuffer framebuffer, BoardIdentity identity);

#endif
