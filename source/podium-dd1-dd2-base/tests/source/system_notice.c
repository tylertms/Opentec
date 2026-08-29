#include <assert.h>
#include <stddef.h>
#include <stdint.h>

#include "system/notice.h"

static void test_timed_notices_expire_after_four_seconds(void) {
    static const SystemNoticeKind kinds[] = {
        SYSTEM_NOTICE_POSITION_SENSOR_TEST_STARTED,
        SYSTEM_NOTICE_TORQUE_REDUCED,
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

static void test_failure_notice_is_persistent(void) {
    SystemNotice notice;
    system_notice_init(&notice);
    system_notice_show(&notice, SYSTEM_NOTICE_POSITION_SENSOR_TEST_FAILED, 100);

    assert(notice.deadline_ms == 0);
    system_notice_update(&notice, UINT32_MAX);
    assert(notice.kind == SYSTEM_NOTICE_POSITION_SENSOR_TEST_FAILED);
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
    test_failure_notice_is_persistent();
    test_timed_notice_expires_across_counter_wrap();
    return 0;
}
