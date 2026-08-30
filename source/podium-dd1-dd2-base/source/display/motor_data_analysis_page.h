#ifndef OPENTEC_BASE_DISPLAY_MOTOR_DATA_ANALYSIS_PAGE_H
#define OPENTEC_BASE_DISPLAY_MOTOR_DATA_ANALYSIS_PAGE_H

#include <stdbool.h>
#include <stdint.h>

#include "board/identity.h"
#include "display/framebuffer.h"

/** @brief Number of torque samples retained for the five-second analysis chart. */
enum { DISPLAY_MOTOR_DATA_ANALYSIS_SAMPLE_COUNT = 120 };

/** @brief Motor torque history, peak hold, temperatures, and fan speed. */
typedef struct {
    uint8_t samples[DISPLAY_MOTOR_DATA_ANALYSIS_SAMPLE_COUNT];
    uint16_t next_sample;
    uint16_t sample_count;
    uint32_t next_sample_ms;
    uint32_t next_peak_update_ms;
    uint32_t peak_expiry_ms;
    int16_t torque;
    int16_t peak_torque;
    uint16_t peak_magnitude;
    int16_t motor_temperature;
    int16_t driver_temperature;
    uint16_t fan_speed_rpm;
    uint16_t torque_limit;
} DisplayMotorDataAnalysisPage;

void display_motor_data_analysis_page_open(DisplayMotorDataAnalysisPage *page, uint32_t now_ms,
                                           BoardVariant variant);
bool display_motor_data_analysis_page_update(DisplayMotorDataAnalysisPage *page, uint32_t now_ms,
                                             int16_t torque, int16_t motor_temperature,
                                             int16_t driver_temperature, uint16_t fan_speed_rpm);
void display_motor_data_analysis_page_render_title(DisplayFramebuffer framebuffer);
void display_motor_data_analysis_page_render(DisplayFramebuffer framebuffer,
                                             const DisplayMotorDataAnalysisPage *page);

#endif
