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
    uint8_t multiplex_phase;
    uint8_t paddle_clutch_phase;
    bool x_available;
    bool y_available;
    bool packet_axis_report_enabled;
} WheelAxisOverrideProcessor;

void wheel_axis_override_processor_init(WheelAxisOverrideProcessor *processor);
void wheel_axis_override_process(WheelAxisOverrideProcessor *processor, uint8_t mode,
                                 uint8_t wheel_mode, uint8_t interface_mode, bool enabled,
                                 uint8_t bite_point_percent, uint8_t x, uint8_t y, uint8_t axes[2]);
void wheel_axis_override_process_packet(WheelAxisOverrideProcessor *processor, uint8_t mode,
                                        uint8_t wheel_mode, uint8_t interface_mode,
                                        uint8_t axis_limit, uint8_t bite_point_percent,
                                        uint8_t controls[8], uint8_t axes[2]);

#endif
