#include "system/notice.h"

#include <stdbool.h>
#include <stdint.h>

/**
 * @brief Internal notice presentation durations.
 *
 * The notice controller uses these intervals for transient system-display states.
 */
enum {
    SYSTEM_NOTICE_DURATION_MS = 4000,             /**< Default transient notice duration. */
    SYSTEM_NOTICE_TUNING_MODE_DURATION_MS = 2000, /**< Tuning-mode notice duration. */
};

/**
 * @brief Selects a system notice presentation interval.
 *
 * Transient wheel-position, position-sensor, and motor-calibration results use the shared
 * four-second interval. Tuning reset and Standard or Advanced mode results use two seconds.
 * Position-sensor failure, an ongoing motor calibration, maximum-rotation and shutdown notices,
 * and unsupported-wheel alerts remain until another notice replaces them.
 *
 * @param[in] kind System notice kind.
 * @return Presentation interval in milliseconds, or zero for a persistent notice.
 */
static uint32_t notice_duration_ms(SystemNoticeKind kind) {
    if (kind == SYSTEM_NOTICE_NONE || kind == SYSTEM_NOTICE_POSITION_SENSOR_TEST_FAILED ||
        kind == SYSTEM_NOTICE_MOTOR_CALIBRATION_ONGOING || kind == SYSTEM_NOTICE_SHUTDOWN ||
        kind == SYSTEM_NOTICE_MAXIMUM_ROTATIONS_EXCEEDED ||
        kind == SYSTEM_NOTICE_UNSUPPORTED_WHEEL_INVERTED ||
        kind == SYSTEM_NOTICE_UNSUPPORTED_WHEEL_OUTLINED) {
        return 0;
    }
    if (kind == SYSTEM_NOTICE_STANDARD_TUNING_MODE || kind == SYSTEM_NOTICE_ADVANCED_TUNING_MODE ||
        kind == SYSTEM_NOTICE_ALTERNATIVE_SHIFTER_ENABLED ||
        kind == SYSTEM_NOTICE_ALTERNATIVE_SHIFTER_DISABLED) {
        return SYSTEM_NOTICE_TUNING_MODE_DURATION_MS;
    }
    return SYSTEM_NOTICE_DURATION_MS;
}

void system_notice_init(SystemNotice *notice) { *notice = (SystemNotice){0}; }

static bool replaces_active(SystemNoticeKind active, SystemNoticeKind next) {
    return (active == SYSTEM_NOTICE_UNSUPPORTED_WHEEL_INVERTED &&
            next == SYSTEM_NOTICE_UNSUPPORTED_WHEEL_OUTLINED) ||
           (active == SYSTEM_NOTICE_UNSUPPORTED_WHEEL_OUTLINED &&
            next == SYSTEM_NOTICE_UNSUPPORTED_WHEEL_INVERTED);
}

static void restore_previous_notice(SystemNotice *notice, uint32_t now_ms) {
    if (notice->stack_count == 0) {
        system_notice_init(notice);
        return;
    }
    notice->stack_count--;
    notice->kind = notice->stack[notice->stack_count];
    uint32_t duration_ms = notice_duration_ms(notice->kind);
    notice->deadline_ms = duration_ms == 0 ? 0 : now_ms + duration_ms;
}

void system_notice_show(SystemNotice *notice, SystemNoticeKind kind, uint32_t now_ms) {
    if (notice->kind != SYSTEM_NOTICE_NONE && !replaces_active(notice->kind, kind)) {
        if (notice->stack_count >= SYSTEM_NOTICE_STACK_CAPACITY - 1) {
            notice->stack_count = 0;
        } else {
            notice->stack[notice->stack_count] = notice->kind;
            notice->stack_count++;
        }
    }
    uint32_t duration_ms = notice_duration_ms(kind);
    notice->kind = kind;
    notice->deadline_ms = duration_ms == 0 ? 0 : now_ms + duration_ms;
}

void system_notice_dismiss(SystemNotice *notice, uint32_t now_ms) {
    restore_previous_notice(notice, now_ms);
}

void system_notice_update(SystemNotice *notice, uint32_t now_ms) {
    if (notice_duration_ms(notice->kind) != 0 && (int32_t)(now_ms - notice->deadline_ms) > 0) {
        restore_previous_notice(notice, now_ms);
    }
}
