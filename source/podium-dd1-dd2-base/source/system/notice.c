#include "system/notice.h"

#include <stdbool.h>
#include <stdint.h>

enum {
    SYSTEM_NOTICE_DURATION_MS = 4000,
    SYSTEM_NOTICE_TUNING_MODE_DURATION_MS = 2000,
};

/**
 * @brief Selects a system notice presentation interval.
 *
 * Transient wheel-position, position-sensor, and motor-calibration results use the shared
 * four-second interval. Tuning reset and Standard or Advanced mode results use two seconds.
 * Position-sensor failure, an ongoing motor calibration, shutdown, and unsupported-wheel alerts
 * remain until another notice replaces them.
 *
 * @param[in] kind System notice kind.
 * @return Presentation interval in milliseconds, or zero for a persistent notice.
 */
static uint32_t notice_duration_ms(SystemNoticeKind kind) {
    if (kind == SYSTEM_NOTICE_NONE || kind == SYSTEM_NOTICE_POSITION_SENSOR_TEST_FAILED ||
        kind == SYSTEM_NOTICE_MOTOR_CALIBRATION_ONGOING || kind == SYSTEM_NOTICE_SHUTDOWN ||
        kind == SYSTEM_NOTICE_UNSUPPORTED_WHEEL_INVERTED ||
        kind == SYSTEM_NOTICE_UNSUPPORTED_WHEEL_OUTLINED) {
        return 0;
    }
    if (kind == SYSTEM_NOTICE_TUNING_MENU_RESET || kind == SYSTEM_NOTICE_STANDARD_TUNING_MODE ||
        kind == SYSTEM_NOTICE_ADVANCED_TUNING_MODE) {
        return SYSTEM_NOTICE_TUNING_MODE_DURATION_MS;
    }
    return SYSTEM_NOTICE_DURATION_MS;
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
 * Timed notices receive their type-specific deadline. Persistent notices and the empty notice keep
 * a zero deadline.
 *
 * @param[in,out] notice System notice state.
 * @param[in] kind Notice to present.
 * @param[in] now_ms Current monotonic time in milliseconds.
 */
void system_notice_show(SystemNotice *notice, SystemNoticeKind kind, uint32_t now_ms) {
    uint32_t duration_ms = notice_duration_ms(kind);
    notice->kind = kind;
    notice->deadline_ms = duration_ms == 0 ? 0 : now_ms + duration_ms;
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
    if (notice_duration_ms(notice->kind) != 0 && (int32_t)(now_ms - notice->deadline_ms) > 0) {
        system_notice_init(notice);
    }
}
