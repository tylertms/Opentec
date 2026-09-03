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
enum {
    DISPLAY_FORCE_FEEDBACK_ANALYSIS_SAMPLE_COUNT = 200, /**< Number of retained samples. */
    DISPLAY_FORCE_FEEDBACK_ANALYSIS_BAR_COUNT = 2,      /**< Counterclockwise and clockwise bars. */
};

/**
 * @brief Stores force-feedback set-point history and current direction.
 *
 * Samples are retained in a ring buffer and the latest scaled set point is stored separately for
 * the summary and directional bars. The millisecond timer callback exclusively owns sampling,
 * ring-buffer, and split-progress state. The foreground service publishes timestamped reopen
 * requests through the two-slot mailbox and consumes the timer's one-byte render indication.
 */
typedef struct {
    uint8_t samples[DISPLAY_FORCE_FEEDBACK_ANALYSIS_SAMPLE_COUNT]; /**< Scaled chart samples from
                                                                      0 through 39 rows. */
    uint16_t next_sample;    /**< Ring-buffer index where the next sample is stored. */
    uint32_t next_sample_ms; /**< Next global 25-millisecond tick at which a sample is stored. */
    uint32_t open_request_ms[2]; /**< Timestamp mailbox for reopen requests. */
    uint8_t open_request_slot; /**< Published timestamp mailbox slot. */
    uint8_t percentage;      /**< Most recently sampled set-point percentage. */
    uint8_t direction;       /**< Most recently sampled direction; zero is counterclockwise. */
    /** @brief Current counterclockwise and clockwise bar values. */
    uint8_t primary_percentage[DISPLAY_FORCE_FEEDBACK_ANALYSIS_BAR_COUNT];
    /** @brief Decaying counterclockwise and clockwise trail values. */
    uint8_t secondary_percentage[DISPLAY_FORCE_FEEDBACK_ANALYSIS_BAR_COUNT];
    /** @brief 8.8 fixed-point secondary decay accumulators. */
    uint16_t decay_accumulator[DISPLAY_FORCE_FEEDBACK_ANALYSIS_BAR_COUNT];
    /** @brief One-millisecond hold countdowns for each bar. */
    uint32_t decay_countdown[DISPLAY_FORCE_FEEDBACK_ANALYSIS_BAR_COUNT];
    uint32_t last_tick_ms; /**< Last one-millisecond update applied to this page. */
    bool tick_initialized; /**< Whether last_tick_ms is valid. */
    bool open_pending; /**< Whether the timer must apply the published reopen request. */
    bool render_pending; /**< Atomic indication that the timer changed visible page data. */
    bool render_active; /**< Whether foreground access is reading or consuming timer-owned state. */
} DisplayForceFeedbackAnalysisPage;

/**
 * @brief Opens a force-feedback analysis session.
 *
 * Publishes a timestamped reopen request. Timer 1 applies the request before its next sampling or
 * decay update, preserving the retained history while restarting the one-millisecond decay clock.
 *
 * @param[in,out] page Force-feedback analysis state to prepare.
 * @param[in] now_ms Current monotonic time in milliseconds.
 */
void display_force_feedback_analysis_page_open(volatile DisplayForceFeedbackAnalysisPage *page,
                                               uint32_t now_ms);

/**
 * @brief Consumes a force-feedback analysis render indication.
 *
 * Returns and clears the one-byte indication published by Timer 1. Sampling, decay, and all
 * multi-byte page state remain exclusively timer-owned.
 *
 * @param[in,out] page Analysis state whose pending indication is consumed.
 * @return True when a sample or visible bar value changed.
 */
bool display_force_feedback_analysis_page_update(volatile DisplayForceFeedbackAnalysisPage *page);

/**
 * @brief Advances the force-feedback analysis from one millisecond timer tick.
 *
 * Samples the live set point on the global 25-millisecond phase and advances both split-progress
 * secondary trails at one-millisecond resolution. It defers updates while foreground rendering
 * holds the render gate and applies queued reopen requests before resuming. The foreground update
 * only consumes the resulting one-byte render indication.
 *
 * @param[in,out] page Force-feedback analysis state to update.
 * @param[in] now_ms Current monotonic timer tick.
 * @param[in] direction Zero for counterclockwise and nonzero for clockwise.
 * @param[in] position Unsigned live set-point magnitude.
 * @return True when a sample or visible secondary value changed.
 */
bool display_force_feedback_analysis_page_tick(volatile DisplayForceFeedbackAnalysisPage *page,
                                               uint32_t now_ms, uint8_t direction,
                                               uint16_t position);

/**
 * @brief Renders the force-feedback analysis title.
 *
 * Draws the inverted title at the reference record coordinates over the current page content.
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
 * @param[in,out] page Force-feedback analysis state to render and protect during drawing.
 */
void display_force_feedback_analysis_page_render(
    DisplayFramebuffer framebuffer, volatile DisplayForceFeedbackAnalysisPage *page);

#endif
