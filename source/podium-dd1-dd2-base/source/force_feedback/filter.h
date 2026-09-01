#ifndef OPENTEC_BASE_FORCE_FEEDBACK_FILTER_H
#define OPENTEC_BASE_FORCE_FEEDBACK_FILTER_H

#include <stdbool.h>
#include <stdint.h>

/** @brief Limits used by the force smoothing ring buffer. */
enum {
    FORCE_FILTER_MAXIMUM_WINDOW =
        40, /**< Largest averaging window selected by a valid intensity. */
    FORCE_FILTER_SAMPLE_CAPACITY =
        FORCE_FILTER_MAXIMUM_WINDOW + 1, /**< Sample capacity for the ring buffer. */
};

/**
 * @brief Runtime state for time-gated force smoothing.
 *
 * The state retains the selected window, its sample history, and the last output so callers can
 * update force requests at a higher rate than the one-millisecond smoothing schedule.
 */
typedef struct {
    int32_t
        samples[FORCE_FILTER_SAMPLE_CAPACITY]; /**< Ring-buffer samples for the active window. */
    int32_t output;                            /**< Most recently calculated smoothed request. */
    uint32_t deadline_ms;                      /**< Earliest time for another accepted sample. */
    uint8_t window;                            /**< Number of samples included in each average. */
    uint8_t index;                             /**< Current ring-buffer cursor. */
    uint8_t previous_intensity;                /**< Last configured smoothing intensity. */
    bool configured;                           /**< Whether the smoothing window is configured. */
} ForceFilter;

/**
 * @brief Configures the smoothing window for force requests.
 *
 * Selects the sample count represented by the supplied intensity and clears the samples used by
 * a changed window. Repeating the active intensity preserves the existing history and cursor.
 *
 * @param[in,out] filter Smoothing state to configure.
 * @param[in] intensity Smoothing intensity used to select the averaging window.
 */
void force_filter_configure(ForceFilter *filter, uint8_t intensity);

/**
 * @brief Updates the time-gated smoothed force request.
 *
 * Returns the previous output before the current deadline. At or after the deadline, stores the
 * new sample, averages the configured window, and schedules the next accepted update one
 * millisecond later.
 *
 * @param[in,out] filter Configured smoothing state to update.
 * @param[in] sample Current signed force request.
 * @param[in] now_ms Current system time in milliseconds.
 * @return Current smoothed force request.
 */
int32_t force_filter_update(ForceFilter *filter, int32_t sample, uint32_t now_ms);

#endif
