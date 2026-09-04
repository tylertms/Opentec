#ifndef OPENTEC_BASE_DISPLAY_MOTOR_DATA_ANALYSIS_PAGE_H
#define OPENTEC_BASE_DISPLAY_MOTOR_DATA_ANALYSIS_PAGE_H

#include <stdbool.h>
#include <stdint.h>

#include "board/identity.h"
#include "display/framebuffer.h"

/**
 * @brief Defines the motor-analysis history capacity.
 *
 * The ring buffer retains 120 samples collected at 41-millisecond intervals for approximately five
 * seconds of torque history.
 */
enum {
    DISPLAY_MOTOR_DATA_ANALYSIS_SAMPLE_COUNT = 120, /**< Number of retained torque samples. */
    DISPLAY_MOTOR_DATA_ANALYSIS_BAR_COUNT = 2,      /**< Positive and negative torque bars. */
};

/**
 * @brief Stores motor-analysis history and live telemetry.
 *
 * The state includes scaled chart samples, a ten-second peak hold, current torque, temperatures,
 * display fan tachometer speed, the variant-specific torque limit, and the two split-progress bar
 * values. Secondary bar values hold their peak for five seconds before applying the reference
 * fixed-point decay.
 */
typedef struct {
    uint8_t samples[DISPLAY_MOTOR_DATA_ANALYSIS_SAMPLE_COUNT]; /**< Scaled signed torque samples for
                                                                  the chart. */
    uint16_t next_sample; /**< Next ring index; it transiently equals the capacity after index 119. */
    uint16_t sample_count;        /**< Number of valid torque samples currently retained. */
    uint32_t next_sample_ms;      /**< Next timestamp at which a torque sample may be stored. */
    uint32_t next_peak_update_ms; /**< Next timestamp at which the peak hold may be evaluated. */
    uint32_t peak_expiry_ms;      /**< Timestamp at which the current peak hold expires. */
    int16_t torque; /**< Most recently sampled signed torque in thousandths of a newton-metre. */
    int16_t peak_torque;        /**< Signed torque value currently held as the peak. */
    uint16_t peak_magnitude;    /**< Absolute magnitude of the held peak torque. */
    int16_t motor_temperature;  /**< Current motor temperature in degrees Celsius. */
    int16_t driver_temperature; /**< Current driver temperature in degrees Celsius. */
    uint16_t fan_speed_rpm; /**< Current display fan tachometer speed in revolutions per minute. */
    uint16_t torque_limit;  /**< Variant-specific display torque limit in thousandths of a
                               newton-metre. */
    uint8_t primary_percentage[DISPLAY_MOTOR_DATA_ANALYSIS_BAR_COUNT]; /**< Current positive and
                                                                           negative bar values. */
    uint8_t secondary_percentage[DISPLAY_MOTOR_DATA_ANALYSIS_BAR_COUNT]; /**< Held positive and
                                                                            negative bar peaks. */
    uint16_t decay_accumulator[DISPLAY_MOTOR_DATA_ANALYSIS_BAR_COUNT]; /**< 8.8 fixed-point decay
                                                                           accumulators. */
    uint32_t decay_countdown[DISPLAY_MOTOR_DATA_ANALYSIS_BAR_COUNT]; /**< Millisecond hold
                                                                         countdowns. */
    uint32_t last_decay_ms; /**< Last elapsed decay timestamp applied to the bars. */
} DisplayMotorDataAnalysisPage;

/**
 * @brief Opens a motor-data analysis session.
 *
 * Preserves retained history, peak state, and split-progress values, selects the DD1 or DD2 torque
 * range, aligns the next chart sample with the global 41-millisecond cadence, and restarts the
 * elapsed decay clock at the opening instant.
 *
 * @param[in,out] page Motor-analysis state to prepare.
 * @param[in] now_ms Current monotonic time in milliseconds.
 * @param[in] variant Hardware variant selecting the torque range.
 */
void display_motor_data_analysis_page_open(DisplayMotorDataAnalysisPage *page, uint32_t now_ms,
                                           BoardVariant variant);

/**
 * @brief Updates motor-analysis history and telemetry.
 *
 * Advances elapsed split-progress decay, samples the torque chart, and evaluates the ten-second
 * peak hold on their respective schedules, while updating temperatures and the display fan
 * tachometer whenever they change.
 *
 * @param[in,out] page Motor-analysis state to update.
 * @param[in] now_ms Current monotonic time in milliseconds.
 * @param[in] torque Signed torque in thousandths of a newton-metre.
 * @param[in] motor_temperature Motor temperature in degrees Celsius.
 * @param[in] driver_temperature Driver temperature in degrees Celsius.
 * @param[in] fan_speed_rpm Display fan tachometer speed in revolutions per minute.
 * @return True when displayed motor-analysis data changed.
 */
bool display_motor_data_analysis_page_update(DisplayMotorDataAnalysisPage *page, uint32_t now_ms,
                                             int16_t torque, int16_t motor_temperature,
                                             int16_t driver_temperature, uint16_t fan_speed_rpm);

/**
 * @brief Renders the motor-analysis title.
 *
 * Draws the inverted title over the current page at the reference record coordinates.
 *
 * @param[in,out] framebuffer Framebuffer receiving the title pixels.
 */
void display_motor_data_analysis_page_render_title(DisplayFramebuffer framebuffer);

/**
 * @brief Renders motor torque history and telemetry.
 *
 * Clears the framebuffer and draws current and peak torque, the signed history chart, directional
 * split-progress torque bars, temperatures, and the display fan tachometer.
 *
 * @param[in,out] framebuffer Framebuffer receiving the rendered page.
 * @param[in] page Motor-analysis state to render.
 */
void display_motor_data_analysis_page_render(DisplayFramebuffer framebuffer,
                                             const DisplayMotorDataAnalysisPage *page);

#endif
