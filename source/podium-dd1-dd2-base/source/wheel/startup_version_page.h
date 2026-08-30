#ifndef OPENTEC_BASE_WHEEL_STARTUP_VERSION_PAGE_H
#define OPENTEC_BASE_WHEEL_STARTUP_VERSION_PAGE_H

#include <stdbool.h>
#include <stdint.h>

#include "motor/identity.h"
#include "wheel/adapter.h"

enum {
    WHEEL_STARTUP_VERSION_LINE_COUNT = 4,
    WHEEL_STARTUP_VERSION_LINE_CAPACITY = 20,
};

typedef struct {
    uint8_t text[WHEEL_STARTUP_VERSION_LINE_CAPACITY];
    uint8_t length;
} WheelStartupVersionLine;

typedef struct {
    WheelStartupVersionLine lines[WHEEL_STARTUP_VERSION_LINE_COUNT];
} WheelStartupVersionPage;

bool wheel_startup_adapter_version_page_build(const MotorIdentity *motor_identity,
                                              const WheelAdapterInput *adapter,
                                              WheelStartupVersionPage *page);

#endif
