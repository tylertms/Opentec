#ifndef OPENTEC_BASE_WHEEL_AXIS_OVERRIDE_H
#define OPENTEC_BASE_WHEEL_AXIS_OVERRIDE_H

#include <stdbool.h>
#include <stdint.h>

enum {
    WHEEL_AXIS_OVERRIDE_MODE_NONE = 0,
    WHEEL_AXIS_OVERRIDE_MODE_CALIBRATED = 1,
    WHEEL_AXIS_OVERRIDE_MODE_SECONDARY = 2,
    WHEEL_AXIS_OVERRIDE_MODE_PRIMARY = 3,
    WHEEL_AXIS_OVERRIDE_MODE_MULTIPLEXED = 4,
    WHEEL_AXIS_MULTIPLEX_SELECT = 0,
    WHEEL_AXIS_MULTIPLEX_X = 1,
    WHEEL_AXIS_MULTIPLEX_Y = 2,
    WHEEL_AXIS_MULTIPLEX_INTERFACE_FIRST = 6,
    WHEEL_AXIS_MULTIPLEX_INTERFACE_LAST = 8,
    WHEEL_PADDLE_CLUTCH_IDLE = 0,
    WHEEL_PADDLE_CLUTCH_ADJUSTING = 1,
    WHEEL_PADDLE_CLUTCH_ARMED = 2,
    WHEEL_PADDLE_CLUTCH_ACTIVE = 3,
};

typedef struct {
    bool enabled;
    uint8_t value;
} WheelAxisOverride;

typedef struct {
    WheelAxisOverride axis_5;
    WheelAxisOverride axis_6;
    WheelAxisOverride axis_7;
    WheelAxisOverride auxiliary;
} WheelAxisOverrides;

typedef struct {
    WheelAxisOverrides overrides;
    uint32_t paddle_adjustment_deadline_ms;
    uint8_t multiplex_phase;
    uint8_t paddle_clutch_phase;
    bool x_available;
    bool y_available;
    bool packet_axis_report_enabled;
    bool paddle_bite_point_report_pending;
    bool paddle_bite_point_commit_pending;
} WheelAxisOverrideProcessor;

void wheel_axis_override_processor_init(WheelAxisOverrideProcessor *processor);
void wheel_axis_override_process(WheelAxisOverrideProcessor *processor, uint8_t mode,
                                 uint8_t wheel_mode, uint8_t interface_mode, bool enabled,
                                 uint32_t now_ms, uint8_t *bite_point_percent, uint8_t *buttons,
                                 int8_t *motion, uint8_t x, uint8_t y, uint8_t axes[2]);
void wheel_axis_override_process_packet(WheelAxisOverrideProcessor *processor, uint8_t mode,
                                        uint8_t wheel_mode, uint8_t interface_mode,
                                        uint8_t axis_limit, uint32_t now_ms,
                                        uint8_t *bite_point_percent, uint8_t *buttons,
                                        int8_t *motion, uint8_t controls[8], uint8_t axes[2]);
void wheel_axis_override_process_axis_mode(WheelAxisOverrideProcessor *processor, uint8_t mode,
                                           uint8_t interface_mode, uint32_t now_ms,
                                           uint8_t *bite_point_percent, uint8_t *buttons,
                                           int8_t *motion, const uint8_t controls[8],
                                           uint8_t axes[2]);
bool wheel_axis_override_take_bite_point(WheelAxisOverrideProcessor *processor,
                                         uint8_t bite_point_percent, uint8_t *updated_percent);
bool wheel_axis_override_take_bite_point_report(WheelAxisOverrideProcessor *processor,
                                                uint8_t bite_point_percent,
                                                uint8_t *updated_percent);

#endif
