#include <assert.h>
#include <stddef.h>
#include <stdint.h>

#include "system/notice.h"

static void test_timed_notices_expire_after_four_seconds(void) {
    static const SystemNoticeKind kinds[] = {
        SYSTEM_NOTICE_WHEEL_CENTER_CALIBRATED,
        SYSTEM_NOTICE_POSITION_SENSOR_TEST_SUCCEEDED,
        SYSTEM_NOTICE_POSITION_SENSOR_TEST_STARTED,
        SYSTEM_NOTICE_TORQUE_REDUCED,
        SYSTEM_NOTICE_TORQUE_REDUCED_STEERING_WHEEL,
        SYSTEM_NOTICE_MOTOR_CALIBRATION_DISCONNECT_WHEEL,
        SYSTEM_NOTICE_MOTOR_CALIBRATION_UNSUPPORTED,
        SYSTEM_NOTICE_MOTOR_CALIBRATION_COMPLETED,
        SYSTEM_NOTICE_MOTOR_CALIBRATION_ERASED,
    };

    for (size_t index = 0; index < sizeof(kinds) / sizeof(kinds[0]); index++) {
        SystemNotice notice;
        system_notice_init(&notice);
        system_notice_show(&notice, kinds[index], 100);

        assert(notice.kind == kinds[index]);
        assert(notice.deadline_ms == 4100);
        system_notice_update(&notice, 4100);
        assert(notice.kind == kinds[index]);
        system_notice_update(&notice, 4101);
        assert(notice.kind == SYSTEM_NOTICE_NONE);
    }
}

static void test_persistent_notices_do_not_expire(void) {
    static const SystemNoticeKind kinds[] = {
        SYSTEM_NOTICE_POSITION_SENSOR_TEST_FAILED,
        SYSTEM_NOTICE_MOTOR_CALIBRATION_ONGOING,
        SYSTEM_NOTICE_MAXIMUM_ROTATIONS_EXCEEDED,
        SYSTEM_NOTICE_SHUTDOWN,
        SYSTEM_NOTICE_UNSUPPORTED_WHEEL_INVERTED,
        SYSTEM_NOTICE_UNSUPPORTED_WHEEL_OUTLINED,
        SYSTEM_NOTICE_TUNING_MODE_TRANSITION_STANDARD,
        SYSTEM_NOTICE_TUNING_MODE_TRANSITION_ADVANCED,
    };

    for (size_t index = 0; index < sizeof(kinds) / sizeof(kinds[0]); index++) {
        SystemNotice notice;
        system_notice_init(&notice);
        system_notice_show(&notice, kinds[index], 100);

        assert(notice.deadline_ms == 0);
        system_notice_update(&notice, UINT32_MAX);
        assert(notice.kind == kinds[index]);
    }
}

static void test_tuning_notices_expire_after_two_seconds(void) {
    static const SystemNoticeKind kinds[] = {
        SYSTEM_NOTICE_TUNING_MENU_RESET,
        SYSTEM_NOTICE_STANDARD_TUNING_MODE,
        SYSTEM_NOTICE_ADVANCED_TUNING_MODE,
        SYSTEM_NOTICE_ALTERNATIVE_SHIFTER_ENABLED,
        SYSTEM_NOTICE_ALTERNATIVE_SHIFTER_DISABLED,
    };

    for (size_t index = 0; index < sizeof(kinds) / sizeof(kinds[0]); index++) {
        SystemNotice notice;
        system_notice_init(&notice);
        system_notice_show(&notice, kinds[index], 100);

        uint32_t duration = kinds[index] == SYSTEM_NOTICE_TUNING_MENU_RESET ? 4000 : 2000;
        assert(notice.deadline_ms == 100 + duration);
        system_notice_update(&notice, 100 + duration);
        assert(notice.kind == kinds[index]);
        system_notice_update(&notice, 101 + duration);
        assert(notice.kind == SYSTEM_NOTICE_NONE);
    }
}

static void test_timed_notice_expires_across_counter_wrap(void) {
    SystemNotice notice;
    system_notice_init(&notice);
    system_notice_show(&notice, SYSTEM_NOTICE_TORQUE_REDUCED, UINT32_MAX - 1000);

    assert(notice.deadline_ms == 2999);
    system_notice_update(&notice, 2999);
    assert(notice.kind == SYSTEM_NOTICE_TORQUE_REDUCED);
    system_notice_update(&notice, 3000);
    assert(notice.kind == SYSTEM_NOTICE_NONE);
}

static void test_coalesces_related_notices(void) {
    static const struct {
        SystemNoticeKind active;
        SystemNoticeKind next;
    } cases[] = {
        {SYSTEM_NOTICE_UNSUPPORTED_WHEEL_INVERTED, SYSTEM_NOTICE_UNSUPPORTED_WHEEL_OUTLINED},
        {SYSTEM_NOTICE_UNSUPPORTED_WHEEL_OUTLINED, SYSTEM_NOTICE_UNSUPPORTED_WHEEL_INVERTED},
        {SYSTEM_NOTICE_TORQUE_REDUCED, SYSTEM_NOTICE_UNSUPPORTED_WHEEL_INVERTED},
        {SYSTEM_NOTICE_TORQUE_REDUCED, SYSTEM_NOTICE_UNSUPPORTED_WHEEL_OUTLINED},
        {SYSTEM_NOTICE_TORQUE_REDUCED, SYSTEM_NOTICE_TUNING_MODE_TRANSITION_STANDARD},
        {SYSTEM_NOTICE_TUNING_MODE_TRANSITION_STANDARD, SYSTEM_NOTICE_STANDARD_TUNING_MODE},
    };

    for (size_t index = 0; index < sizeof(cases) / sizeof(cases[0]); index++) {
        SystemNotice notice;
        system_notice_init(&notice);
        system_notice_show(&notice, cases[index].active, 100);
        system_notice_show(&notice, cases[index].next, 200);

        assert(notice.kind == cases[index].next);
        assert(notice.stack_count == 0);
    }

    static const struct {
        SystemNoticeKind active;
        SystemNoticeKind next;
    } non_coalescing_cases[] = {
        {SYSTEM_NOTICE_TUNING_MENU_RESET, SYSTEM_NOTICE_STANDARD_TUNING_MODE},
        {SYSTEM_NOTICE_STANDARD_TUNING_MODE, SYSTEM_NOTICE_ADVANCED_TUNING_MODE},
        {SYSTEM_NOTICE_ALTERNATIVE_SHIFTER_ENABLED, SYSTEM_NOTICE_ALTERNATIVE_SHIFTER_DISABLED},
        {SYSTEM_NOTICE_TORQUE_REDUCED, SYSTEM_NOTICE_WHEEL_CENTER_CALIBRATED},
        {SYSTEM_NOTICE_TUNING_MODE_TRANSITION_STANDARD,
         SYSTEM_NOTICE_TUNING_MODE_TRANSITION_ADVANCED},
    };

    for (size_t index = 0; index < sizeof(non_coalescing_cases) / sizeof(non_coalescing_cases[0]);
         index++) {
        SystemNotice notice;
        system_notice_init(&notice);
        system_notice_show(&notice, non_coalescing_cases[index].active, 100);
        system_notice_show(&notice, non_coalescing_cases[index].next, 200);

        assert(notice.kind == non_coalescing_cases[index].next);
        assert(notice.stack_count == 1);
        assert(notice.stack[0] == non_coalescing_cases[index].active);
    }
}

static void test_restored_notice_gets_a_fresh_deadline(void) {
    SystemNotice notice;
    system_notice_init(&notice);
    system_notice_show(&notice, SYSTEM_NOTICE_TORQUE_REDUCED, 100);
    system_notice_show(&notice, SYSTEM_NOTICE_WHEEL_CENTER_CALIBRATED, 200);

    system_notice_update(&notice, 4201);
    assert(notice.kind == SYSTEM_NOTICE_TORQUE_REDUCED);
    assert(notice.deadline_ms == 8201);
    assert(notice.stack_count == 0);
    system_notice_update(&notice, 8201);
    assert(notice.kind == SYSTEM_NOTICE_TORQUE_REDUCED);
    system_notice_update(&notice, 8202);
    assert(notice.kind == SYSTEM_NOTICE_NONE);

    system_notice_init(&notice);
    system_notice_show(&notice, SYSTEM_NOTICE_UNSUPPORTED_WHEEL_INVERTED, 100);
    system_notice_show(&notice, SYSTEM_NOTICE_TORQUE_REDUCED, 200);
    system_notice_update(&notice, 4201);
    assert(notice.kind == SYSTEM_NOTICE_UNSUPPORTED_WHEEL_INVERTED);
    assert(notice.deadline_ms == 0);
}

static void test_replacing_warning_discards_superseded_notice(void) {
    SystemNotice notice;
    system_notice_init(&notice);
    system_notice_show(&notice, SYSTEM_NOTICE_WHEEL_CENTER_CALIBRATED, 100);
    system_notice_show(&notice, SYSTEM_NOTICE_UNSUPPORTED_WHEEL_INVERTED, 200);

    system_notice_dismiss(&notice, 300);

    assert(notice.kind == SYSTEM_NOTICE_NONE);
    assert(notice.deadline_ms == 0);
    assert(notice.stack_count == 0);
}

static void test_dismisses_a_targeted_notice(void) {
    SystemNotice notice;
    system_notice_init(&notice);
    system_notice_show(&notice, SYSTEM_NOTICE_TORQUE_REDUCED, 100);
    system_notice_show(&notice, SYSTEM_NOTICE_MOTOR_CALIBRATION_ONGOING, 200);
    system_notice_show(&notice, SYSTEM_NOTICE_WHEEL_CENTER_CALIBRATED, 300);

    system_notice_dismiss_kind(&notice, SYSTEM_NOTICE_MOTOR_CALIBRATION_ONGOING, 400);

    assert(notice.kind == SYSTEM_NOTICE_WHEEL_CENTER_CALIBRATED);
    assert(notice.stack_count == 1);
    assert(notice.stack[0] == SYSTEM_NOTICE_TORQUE_REDUCED);

    system_notice_dismiss_kind(&notice, SYSTEM_NOTICE_WHEEL_CENTER_CALIBRATED, 500);

    assert(notice.kind == SYSTEM_NOTICE_TORQUE_REDUCED);
    assert(notice.deadline_ms == 4500);
    assert(notice.stack_count == 0);

    system_notice_dismiss_kind(&notice, SYSTEM_NOTICE_MOTOR_CALIBRATION_ONGOING, 600);
    assert(notice.kind == SYSTEM_NOTICE_TORQUE_REDUCED);
}

static void test_compacts_the_fifth_notice_and_retains_it_below_the_sixth(void) {
    static const SystemNoticeKind kinds[] = {
        SYSTEM_NOTICE_TORQUE_REDUCED,
        SYSTEM_NOTICE_WHEEL_CENTER_CALIBRATED,
        SYSTEM_NOTICE_POSITION_SENSOR_TEST_SUCCEEDED,
        SYSTEM_NOTICE_POSITION_SENSOR_TEST_STARTED,
        SYSTEM_NOTICE_MOTOR_CALIBRATION_COMPLETED,
        SYSTEM_NOTICE_MOTOR_CALIBRATION_ERASED,
    };
    SystemNotice notice;
    system_notice_init(&notice);

    for (size_t index = 0; index < 4; index++) {
        system_notice_show(&notice, kinds[index], (uint32_t)(index * 100));
    }
    assert(notice.kind == kinds[3]);
    assert(notice.stack_count == SYSTEM_NOTICE_STACK_CAPACITY);
    for (size_t index = 0; index < SYSTEM_NOTICE_STACK_CAPACITY; index++) {
        assert(notice.stack[index] == kinds[index]);
    }

    system_notice_show(&notice, kinds[4], 400);
    assert(notice.kind == kinds[4]);
    assert(notice.deadline_ms == 4400);
    assert(notice.stack_count == 0);

    system_notice_show(&notice, kinds[5], 500);
    assert(notice.kind == kinds[5]);
    assert(notice.deadline_ms == 4500);
    assert(notice.stack_count == 1);
    assert(notice.stack[0] == kinds[4]);
}

int main(void) {
    test_timed_notices_expire_after_four_seconds();
    test_persistent_notices_do_not_expire();
    test_tuning_notices_expire_after_two_seconds();
    test_timed_notice_expires_across_counter_wrap();
    test_coalesces_related_notices();
    test_restored_notice_gets_a_fresh_deadline();
    test_replacing_warning_discards_superseded_notice();
    test_dismisses_a_targeted_notice();
    test_compacts_the_fifth_notice_and_retains_it_below_the_sixth();
    return 0;
}
