#ifndef OPENTEC_BASE_DISPLAY_TEMPERATURE_ANALYSIS_PAGE_H
#define OPENTEC_BASE_DISPLAY_TEMPERATURE_ANALYSIS_PAGE_H

#include <stdbool.h>
#include <stdint.h>

#include "display/framebuffer.h"

/** @brief Temperature channels presented by the diagnostic page. */
typedef enum {
    DISPLAY_TEMPERATURE_ANALYSIS_MOTOR,
    DISPLAY_TEMPERATURE_ANALYSIS_DRIVER,
    DISPLAY_TEMPERATURE_ANALYSIS_BASE,
    DISPLAY_TEMPERATURE_ANALYSIS_QUICK_RELEASE,
    DISPLAY_TEMPERATURE_ANALYSIS_CHANNEL_COUNT,
} DisplayTemperatureAnalysisChannel;

/** @brief Number of samples retained per 90-second temperature chart. */
enum { DISPLAY_TEMPERATURE_ANALYSIS_SAMPLE_COUNT = 40 };

/** @brief Retained temperature histories and live cooling values. */
typedef struct {
    uint8_t samples[DISPLAY_TEMPERATURE_ANALYSIS_CHANNEL_COUNT]
                   [DISPLAY_TEMPERATURE_ANALYSIS_SAMPLE_COUNT];
    int16_t temperatures[DISPLAY_TEMPERATURE_ANALYSIS_CHANNEL_COUNT];
    uint32_t next_sample_ms;
    uint16_t fan_speed_rpm;
    uint8_t next_sample;
    uint8_t sample_count;
    uint8_t power_percent;
} DisplayTemperatureAnalysisPage;

void display_temperature_analysis_page_open(DisplayTemperatureAnalysisPage *page, uint32_t now_ms);
bool display_temperature_analysis_page_update(
    DisplayTemperatureAnalysisPage *page, uint32_t now_ms,
    const int16_t temperatures[DISPLAY_TEMPERATURE_ANALYSIS_CHANNEL_COUNT], uint16_t fan_speed_rpm,
    uint8_t power_percent);
void display_temperature_analysis_page_render_title(DisplayFramebuffer framebuffer);
void display_temperature_analysis_page_render(DisplayFramebuffer framebuffer,
                                              const DisplayTemperatureAnalysisPage *page);

#endif
