#ifndef OPENTEC_BASE_DISPLAY_TEMPERATURE_ANALYSIS_PAGE_H
#define OPENTEC_BASE_DISPLAY_TEMPERATURE_ANALYSIS_PAGE_H

#include <stdbool.h>
#include <stdint.h>

#include "display/framebuffer.h"

/**
 * @brief Identifies the temperature history rendered by the diagnostic page.
 *
 * The values also define the index order expected by the page state and update function.
 */
typedef enum {
    DISPLAY_TEMPERATURE_ANALYSIS_MOTOR,         /**< Motor temperature channel. */
    DISPLAY_TEMPERATURE_ANALYSIS_DRIVER,        /**< Motor-driver temperature channel. */
    DISPLAY_TEMPERATURE_ANALYSIS_BASE,          /**< Base temperature channel. */
    DISPLAY_TEMPERATURE_ANALYSIS_QUICK_RELEASE, /**< Wheel quick-release temperature channel. */
    DISPLAY_TEMPERATURE_ANALYSIS_CHANNEL_COUNT, /**< Number of temperature channels. */
} DisplayTemperatureAnalysisChannel;

/**
 * @brief Defines the temperature-analysis history capacity.
 *
 * Forty samples collected at 2.25-second global tick intervals cover the 90-second chart history
 * for each channel.
 */
enum {
    DISPLAY_TEMPERATURE_ANALYSIS_SAMPLE_COUNT = 40 /**< Number of retained samples per channel. */
};

/**
 * @brief Stores temperature histories and live cooling values.
 *
 * Each channel has a ring buffer of scaled chart samples while the latest temperatures, display fan
 * tachometer speed, and actual thermal output duty remain available for the summary fields.
 */
typedef struct {
    uint8_t samples[DISPLAY_TEMPERATURE_ANALYSIS_CHANNEL_COUNT]
                   [DISPLAY_TEMPERATURE_ANALYSIS_SAMPLE_COUNT]; /**< Scaled chart samples by channel
                                                                   and ring index. */
    int16_t
        temperatures[DISPLAY_TEMPERATURE_ANALYSIS_CHANNEL_COUNT]; /**< Latest channel temperatures
                                                                     in degrees Celsius. */
    uint32_t startup_deadline_ms; /**< Timestamp at which startup sampling may begin. */
    uint32_t last_sample_tick_ms; /**< Most recent global tick that stored a sample. */
    bool startup_initialized;     /**< Whether the startup deadline has been established. */
    bool startup_pending;         /**< Whether the one-second startup delay is still active. */
    bool sample_tick_seen;        /**< Whether a divisible global tick has already been sampled. */
    uint16_t fan_speed_rpm; /**< Current display fan tachometer speed in revolutions per minute. */
    uint8_t next_sample; /**< Next ring index; it transiently equals the capacity after index 39. */
    uint8_t power_percent; /**< Current actual thermal output duty in percent. */
} DisplayTemperatureAnalysisPage;

/**
 * @brief Updates temperature histories and cooling values.
 *
 * Establishes a one-second startup delay, then samples all channels once per divisible global
 * 2,250-millisecond tick. Fan tachometer speed and actual output duty are updated whenever either
 * live value changes.
 *
 * @param[in,out] page Temperature-analysis state to update.
 * @param[in] now_ms Current monotonic time in milliseconds.
 * @param[in] temperatures Motor, driver, base, and wheel quick-release temperatures in degrees
 * Celsius.
 * @param[in] fan_speed_rpm Display fan tachometer speed in revolutions per minute.
 * @param[in] power_percent Actual thermal output duty in percent.
 * @return True when displayed analysis data changed.
 */
bool display_temperature_analysis_page_update(
    DisplayTemperatureAnalysisPage *page, uint32_t now_ms,
    const int16_t temperatures[DISPLAY_TEMPERATURE_ANALYSIS_CHANNEL_COUNT], uint16_t fan_speed_rpm,
    uint8_t power_percent);

/**
 * @brief Renders the temperature-analysis title.
 *
 * Clears the framebuffer and draws the inverted title at the official record coordinates.
 *
 * @param[in,out] framebuffer Framebuffer receiving the title pixels.
 */
void display_temperature_analysis_page_render_title(DisplayFramebuffer framebuffer);

/**
 * @brief Renders temperature histories and cooling telemetry.
 *
 * Clears the framebuffer and draws four temperature charts with one-decimal scale labels, current
 * values, display fan tachometer speed, and actual thermal output duty.
 *
 * @param[in,out] framebuffer Framebuffer receiving the rendered page.
 * @param[in] page Temperature-analysis state to render.
 */
void display_temperature_analysis_page_render(DisplayFramebuffer framebuffer,
                                              const DisplayTemperatureAnalysisPage *page);

#endif
