#ifndef OPENTEC_BASE_DISPLAY_NOTICE_H
#define OPENTEC_BASE_DISPLAY_NOTICE_H

#include <stdbool.h>
#include <stdint.h>

#include "display/framebuffer.h"
#include "system/notice.h"

void display_notice_render_torque_disabled(DisplayFramebuffer framebuffer, bool visible);
void display_notice_render_system(DisplayFramebuffer framebuffer, SystemNoticeKind kind);

#endif
