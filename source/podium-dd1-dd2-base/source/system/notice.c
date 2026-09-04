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

/** @brief Official overlay modes used by notice replacement predicates. */
enum {
    SYSTEM_NOTICE_OVERLAY_UNMATCHED = -1,
    SYSTEM_NOTICE_OVERLAY_TIMED = 0,
    SYSTEM_NOTICE_OVERLAY_INVERTED = 4,
    SYSTEM_NOTICE_OVERLAY_OUTLINED = 5,
    SYSTEM_NOTICE_OVERLAY_TRANSITION = 6,
    SYSTEM_NOTICE_OVERLAY_RESULT = 7,
};

/**
 * @brief Selects a system notice presentation interval.
 *
 * Transient wheel-position, position-sensor, and motor-calibration results use the shared
 * four-second interval. Tuning reset and Standard or Advanced mode results use two seconds.
 * Position-sensor failure, an ongoing motor calibration, tuning-mode transitions,
 * maximum-rotation and shutdown notices, and unsupported-wheel alerts remain until another notice
 * replaces them.
 *
 * @param[in] kind System notice kind.
 * @return Presentation interval in milliseconds, or zero for a persistent notice.
 */
static uint32_t notice_duration_ms(SystemNoticeKind kind) {
    if (kind == SYSTEM_NOTICE_NONE || kind == SYSTEM_NOTICE_POSITION_SENSOR_TEST_FAILED ||
        kind == SYSTEM_NOTICE_MOTOR_CALIBRATION_ONGOING || kind == SYSTEM_NOTICE_SHUTDOWN ||
        kind == SYSTEM_NOTICE_MAXIMUM_ROTATIONS_EXCEEDED ||
        kind == SYSTEM_NOTICE_UNSUPPORTED_WHEEL_INVERTED ||
        kind == SYSTEM_NOTICE_UNSUPPORTED_WHEEL_OUTLINED ||
        kind == SYSTEM_NOTICE_TUNING_MODE_TRANSITION_STANDARD ||
        kind == SYSTEM_NOTICE_TUNING_MODE_TRANSITION_ADVANCED) {
        return 0;
    }
    if (kind == SYSTEM_NOTICE_STANDARD_TUNING_MODE || kind == SYSTEM_NOTICE_ADVANCED_TUNING_MODE ||
        kind == SYSTEM_NOTICE_ALTERNATIVE_SHIFTER_ENABLED ||
        kind == SYSTEM_NOTICE_ALTERNATIVE_SHIFTER_DISABLED) {
        return SYSTEM_NOTICE_TUNING_MODE_DURATION_MS;
    }
    return SYSTEM_NOTICE_DURATION_MS;
}

/**
 * @brief Maps source notice kinds to official overlay modes.
 *
 * Only modes used by the replacement predicate at 0x03BE40-0x03BE82 need representation here.
 * Unmatched notices retain ordinary stack behavior.
 *
 * @param[in] kind Source notice kind.
 * @return Official overlay mode, or the unmatched sentinel.
 */
static int8_t notice_overlay_mode(SystemNoticeKind kind) {
    switch (kind) {
    case SYSTEM_NOTICE_UNSUPPORTED_WHEEL_INVERTED:
        return SYSTEM_NOTICE_OVERLAY_INVERTED;
    case SYSTEM_NOTICE_UNSUPPORTED_WHEEL_OUTLINED:
        return SYSTEM_NOTICE_OVERLAY_OUTLINED;
    case SYSTEM_NOTICE_TUNING_MODE_TRANSITION_STANDARD:
    case SYSTEM_NOTICE_TUNING_MODE_TRANSITION_ADVANCED:
        return SYSTEM_NOTICE_OVERLAY_TRANSITION;
    case SYSTEM_NOTICE_STANDARD_TUNING_MODE:
    case SYSTEM_NOTICE_ADVANCED_TUNING_MODE:
    case SYSTEM_NOTICE_ALTERNATIVE_SHIFTER_ENABLED:
    case SYSTEM_NOTICE_ALTERNATIVE_SHIFTER_DISABLED:
        return SYSTEM_NOTICE_OVERLAY_RESULT;
    case SYSTEM_NOTICE_TUNING_MENU_RESET:
    case SYSTEM_NOTICE_WHEEL_CENTER_CALIBRATED:
    case SYSTEM_NOTICE_POSITION_SENSOR_TEST_SUCCEEDED:
    case SYSTEM_NOTICE_POSITION_SENSOR_TEST_STARTED:
    case SYSTEM_NOTICE_TORQUE_REDUCED:
    case SYSTEM_NOTICE_TORQUE_REDUCED_STEERING_WHEEL:
    case SYSTEM_NOTICE_MOTOR_CALIBRATION_DISCONNECT_WHEEL:
    case SYSTEM_NOTICE_MOTOR_CALIBRATION_UNSUPPORTED:
    case SYSTEM_NOTICE_MOTOR_CALIBRATION_COMPLETED:
    case SYSTEM_NOTICE_MOTOR_CALIBRATION_ERASED:
        return SYSTEM_NOTICE_OVERLAY_TIMED;
    default:
        return SYSTEM_NOTICE_OVERLAY_UNMATCHED;
    }
}

void system_notice_init(SystemNotice *notice) {
    notice->kind = SYSTEM_NOTICE_NONE;
    notice->deadline_ms = 0;
    for (uint8_t index = 0; index < SYSTEM_NOTICE_STACK_CAPACITY; index++) {
        notice->stack[index] = SYSTEM_NOTICE_NONE;
    }
    notice->stack_count = 0;
}

static bool replaces_active(SystemNoticeKind active, SystemNoticeKind next) {
    int8_t active_mode = notice_overlay_mode(active);
    int8_t next_mode = notice_overlay_mode(next);
    return (active_mode == SYSTEM_NOTICE_OVERLAY_TIMED &&
            (next_mode == SYSTEM_NOTICE_OVERLAY_INVERTED ||
             next_mode == SYSTEM_NOTICE_OVERLAY_OUTLINED ||
             next_mode == SYSTEM_NOTICE_OVERLAY_TRANSITION)) ||
           (active_mode == SYSTEM_NOTICE_OVERLAY_INVERTED &&
            next_mode == SYSTEM_NOTICE_OVERLAY_OUTLINED) ||
           (active_mode == SYSTEM_NOTICE_OVERLAY_OUTLINED &&
            next_mode == SYSTEM_NOTICE_OVERLAY_INVERTED) ||
           (active_mode == SYSTEM_NOTICE_OVERLAY_TRANSITION &&
            next_mode == SYSTEM_NOTICE_OVERLAY_RESULT);
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
        if (notice->stack_count >= SYSTEM_NOTICE_STACK_CAPACITY) {
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
