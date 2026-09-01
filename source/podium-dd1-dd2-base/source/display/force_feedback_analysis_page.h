#ifndef OPENTEC_BASE_DISPLAY_FORCE_FEEDBACK_ANALYSIS_PAGE_H
#define OPENTEC_BASE_DISPLAY_FORCE_FEEDBACK_ANALYSIS_PAGE_H

#include <stdbool.h>
#include <stdint.h>

#include "display/framebuffer.h"

/**
 * @brief Defines the force-feedback analysis history capacity.
 *
 * The ring buffer retains 200 samples collected at 25-millisecond intervals, covering five
 * seconds of set-point history.
 */
enum { DISPLAY_FORCE_FEEDBACK_ANALYSIS_SAMPLE_COUNT = 200 /**< Number of retained samples. */ };

/**
 * @brief Stores force-feedback set-point history and current direction.
 *
 * Samples are retained in a ring buffer and the latest scaled set point is stored separately for
 * the summary and directional bars.
 */
typedef struct {
    uint8_t samples[DISPLAY_FORCE_FEEDBACK_ANALYSIS_SAMPLE_COUNT]; /**< Scaled set-point samples
                                                                      from 0 through 99 percent. */
    uint16_t next_sample;    /**< Ring-buffer index where the next sample is stored. */
    uint16_t sample_count;   /**< Number of valid samples currently retained. */
    uint32_t next_sample_ms; /**< Next timestamp at which a sample may be stored. */
    uint8_t percentage;      /**< Most recently sampled set-point percentage. */
    uint8_t direction;       /**< Most recently sampled direction; zero is counterclockwise. */
} DisplayForceFeedbackAnalysisPage;

/**
 * @brief Opens a force-feedback analysis session.
 *
 * Preserves the retained history and aligns the next sample deadline with the global 25-millisecond
 * cadence.
 *
 * @param[in,out] page Force-feedback analysis state to prepare.
 * @param[in] now_ms Current monotonic time in milliseconds.
 */
void display_force_feedback_analysis_page_open(DisplayForceFeedbackAnalysisPage *page,
                                               uint32_t now_ms);

/**
 * @brief Updates the force-feedback analysis history.
 *
 * When the sampling deadline is due, converts the unsigned magnitude into a percentage, stores it
 * in the five-second ring buffer, and records the direction.
 *
 * @param[in,out] page Force-feedback analysis state to update.
 * @param[in] now_ms Current monotonic time in milliseconds.
 * @param[in] direction Zero for counterclockwise and nonzero for clockwise.
 * @param[in] position Unsigned set-point magnitude, where the full 16-bit range maps to 0 through
 * 99 percent.
 * @return True when a new sample was stored.
 */
bool display_force_feedback_analysis_page_update(DisplayForceFeedbackAnalysisPage *page,
                                                 uint32_t now_ms, uint8_t direction,
                                                 uint16_t position);

/**
 * @brief Renders the force-feedback analysis title.
 *
 * Clears the framebuffer and centers the title shown while the analysis page opens.
 *
 * @param[in,out] framebuffer Framebuffer receiving the title pixels.
 */
void display_force_feedback_analysis_page_render_title(DisplayFramebuffer framebuffer);

/**
 * @brief Renders force-feedback set-point history and direction.
 *
 * Clears the framebuffer and draws the current percentage, direction, history chart, and
 * direction-specific level bars.
 *
 * @param[in,out] framebuffer Framebuffer receiving the rendered page.
 * @param[in] page Force-feedback analysis state to render.
 */
void display_force_feedback_analysis_page_render(DisplayFramebuffer framebuffer,
                                                 const DisplayForceFeedbackAnalysisPage *page);

#endif
