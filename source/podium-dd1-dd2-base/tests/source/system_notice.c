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
        SYSTEM_NOTICE_POSITION_SENSOR_TEST_FAILED, SYSTEM_NOTICE_MOTOR_CALIBRATION_ONGOING,
        SYSTEM_NOTICE_MAXIMUM_ROTATIONS_EXCEEDED,  SYSTEM_NOTICE_SHUTDOWN,
        SYSTEM_NOTICE_UNSUPPORTED_WHEEL_INVERTED,  SYSTEM_NOTICE_UNSUPPORTED_WHEEL_OUTLINED,
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

        assert(notice.deadline_ms == 2100);
        system_notice_update(&notice, 2100);
        assert(notice.kind == kinds[index]);
        system_notice_update(&notice, 2101);
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

int main(void) {
    test_timed_notices_expire_after_four_seconds();
    test_persistent_notices_do_not_expire();
    test_tuning_notices_expire_after_two_seconds();
    test_timed_notice_expires_across_counter_wrap();
    return 0;
}
