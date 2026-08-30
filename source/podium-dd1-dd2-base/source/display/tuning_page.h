#ifndef OPENTEC_BASE_DISPLAY_TUNING_PAGE_H
#define OPENTEC_BASE_DISPLAY_TUNING_PAGE_H

#include <stdbool.h>

#include "display/framebuffer.h"
#include "profile/bank.h"
#include "profile/tuning_menu.h"

enum { TUNING_PAGE_VALUE_SIZE = 24 };

/** @brief Text selected for one local OLED tuning page. */
typedef struct {
    const char *label;
    const char *title;
    const char *description;
    char value[TUNING_PAGE_VALUE_SIZE];
} TuningPageContent;

bool display_tuning_page_present(const TuningMenu *menu, const TuningProfileBank *bank,
                                 TuningPageContent *content);
bool display_tuning_page_render(DisplayFramebuffer framebuffer, const TuningMenu *menu,
                                const TuningProfileBank *bank);

#endif
