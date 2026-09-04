#ifndef OPENTEC_BASE_DISPLAY_RESET_SCHEDULER_H
#define OPENTEC_BASE_DISPLAY_RESET_SCHEDULER_H

#include <stdint.h>

/** @brief Phases of the asynchronous display reset sequence. */
typedef enum {
    DISPLAY_RESET_PHASE_SETUP,    /**< Wait before asserting reset. */
    DISPLAY_RESET_PHASE_ASSERT,   /**< Wait before driving reset low. */
    DISPLAY_RESET_PHASE_RELEASE,  /**< Wait before driving reset high. */
    DISPLAY_RESET_PHASE_COMPLETE, /**< Wait before reporting completion. */
} DisplayResetPhase;

/** @brief Hardware transition requested by one reset scheduler step. */
typedef enum {
    DISPLAY_RESET_ACTION_NONE,         /**< No hardware transition is due. */
    DISPLAY_RESET_ACTION_ASSERT_LOW,   /**< Drive the display reset pin low. */
    DISPLAY_RESET_ACTION_RELEASE_HIGH, /**< Drive the display reset pin high. */
} DisplayResetAction;

/** @brief State for one nonblocking display reset sequence. */
typedef struct {
    DisplayResetPhase phase; /**< Current reset timing phase. */
    uint32_t deadline_ms;    /**< Strict deadline for the current phase. */
} DisplayResetScheduler;

/**
 * @brief Initializes display reset timing.
 *
 * Starts at the setup phase with no elapsed deadline.
 *
 * @param[out] scheduler Reset scheduler to initialize; null is ignored.
 */
void display_reset_scheduler_init(DisplayResetScheduler *scheduler);

/**
 * @brief Advances display reset timing without blocking.
 *
 * The scheduler uses strict greater-than deadlines matching the reference sequence: two
 * milliseconds before assertion, one millisecond low, and one millisecond after release.
 *
 * @param[in,out] scheduler Reset scheduler to advance; null returns no action.
 * @param[in] now_ms Current monotonic time in milliseconds.
 * @return Hardware transition due at this timestamp. The scheduler does not drive the reset pin.
 */
DisplayResetAction display_reset_scheduler_step(DisplayResetScheduler *scheduler, uint32_t now_ms);

#endif
