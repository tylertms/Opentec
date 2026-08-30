#ifndef OPENTEC_BASE_DISPLAY_FORCE_FEEDBACK_ANALYSIS_PAGE_H
#define OPENTEC_BASE_DISPLAY_FORCE_FEEDBACK_ANALYSIS_PAGE_H

#include <stdbool.h>
#include <stdint.h>

#include "display/framebuffer.h"

/** @brief Number of set-point samples retained for the five-second analysis chart. */
enum { DISPLAY_FORCE_FEEDBACK_ANALYSIS_SAMPLE_COUNT = 200 };

/** @brief Force-feedback set-point sample history and current direction. */
typedef struct {
    uint8_t samples[DISPLAY_FORCE_FEEDBACK_ANALYSIS_SAMPLE_COUNT];
    uint16_t next_sample;
    uint16_t sample_count;
    uint32_t next_sample_ms;
    uint8_t percentage;
    uint8_t direction;
} DisplayForceFeedbackAnalysisPage;

void display_force_feedback_analysis_page_open(DisplayForceFeedbackAnalysisPage *page,
                                               uint32_t now_ms);
bool display_force_feedback_analysis_page_update(DisplayForceFeedbackAnalysisPage *page,
                                                 uint32_t now_ms, uint8_t direction,
                                                 uint16_t position);
void display_force_feedback_analysis_page_render_title(DisplayFramebuffer framebuffer);
void display_force_feedback_analysis_page_render(DisplayFramebuffer framebuffer,
                                                 const DisplayForceFeedbackAnalysisPage *page);

#endif
