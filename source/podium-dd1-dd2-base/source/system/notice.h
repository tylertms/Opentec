#ifndef OPENTEC_BASE_SYSTEM_NOTICE_H
#define OPENTEC_BASE_SYSTEM_NOTICE_H

#include <stdint.h>

typedef enum {
    SYSTEM_NOTICE_NONE,
    SYSTEM_NOTICE_POSITION_SENSOR_TEST_STARTED,
    SYSTEM_NOTICE_POSITION_SENSOR_TEST_FAILED,
    SYSTEM_NOTICE_TORQUE_REDUCED,
} SystemNoticeKind;

typedef struct {
    SystemNoticeKind kind;
    uint32_t deadline_ms;
} SystemNotice;

void system_notice_init(SystemNotice *notice);
void system_notice_show(SystemNotice *notice, SystemNoticeKind kind, uint32_t now_ms);
void system_notice_update(SystemNotice *notice, uint32_t now_ms);

#endif
