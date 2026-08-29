#include "system/notice.h"

#include <stdbool.h>
#include <stdint.h>

enum {
    SYSTEM_NOTICE_DURATION_MS = 4000,
};

/**
 * @brief Tests whether a system notice has a finite presentation interval.
 *
 * Transient tuning, wheel-position, position-sensor, and motor-calibration results use the shared
 * four-second interval. Position-sensor failure and an ongoing motor calibration remain until
 * another notice replaces them.
 *
 * @param[in] kind System notice kind.
 * @return True when the notice has a finite presentation interval.
 */
static bool notice_is_timed(SystemNoticeKind kind) {
    return kind != SYSTEM_NOTICE_NONE && kind != SYSTEM_NOTICE_POSITION_SENSOR_TEST_FAILED &&
           kind != SYSTEM_NOTICE_MOTOR_CALIBRATION_ONGOING;
}

/**
 * @brief Initializes system notice presentation state.
 *
 * Starts with no active notice or expiration deadline.
 *
 * @param[out] notice System notice state.
 */
void system_notice_init(SystemNotice *notice) { *notice = (SystemNotice){0}; }

/**
 * @brief Starts presentation of a system notice.
 *
 * Timed notices receive a four-second deadline. Persistent notices and the empty notice keep a
 * zero deadline.
 *
 * @param[in,out] notice System notice state.
 * @param[in] kind Notice to present.
 * @param[in] now_ms Current monotonic time in milliseconds.
 */
void system_notice_show(SystemNotice *notice, SystemNoticeKind kind, uint32_t now_ms) {
    notice->kind = kind;
    notice->deadline_ms = notice_is_timed(kind) ? now_ms + SYSTEM_NOTICE_DURATION_MS : 0;
}

/**
 * @brief Expires a completed timed system notice.
 *
 * Keeps the notice visible through its deadline and clears it on the first later update. Modular
 * signed subtraction preserves the comparison across millisecond-counter wrap.
 *
 * @param[in,out] notice System notice state.
 * @param[in] now_ms Current monotonic time in milliseconds.
 */
void system_notice_update(SystemNotice *notice, uint32_t now_ms) {
    if (notice_is_timed(notice->kind) && (int32_t)(now_ms - notice->deadline_ms) > 0) {
        system_notice_init(notice);
    }
}
