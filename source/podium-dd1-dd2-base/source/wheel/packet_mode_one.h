#ifndef OPENTEC_BASE_WHEEL_PACKET_MODE_ONE_H
#define OPENTEC_BASE_WHEEL_PACKET_MODE_ONE_H

#include <stdbool.h>
#include <stdint.h>

#include "wheel/display_output.h"

enum {
    WHEEL_PACKET_MODE_ONE_RESPONSE_SIZE = 9,
    WHEEL_PACKET_MODE_ONE_REQUEST_SIZE = 32,
    WHEEL_PACKET_MODE_ONE_BUTTON_COUNT = 3,
    WHEEL_PACKET_MODE_ONE_BUTTON_HISTORY_DEPTH = 3,
    WHEEL_PACKET_MODE_ONE_CONTROL_AXIS_COUNT = 2,
    WHEEL_PACKET_MODE_ONE_CONTROL_AXIS_HISTORY_DEPTH = 3,
    WHEEL_PACKET_MODE_ONE_AXIS_OUTPUT_COUNT = 2,
    WHEEL_PACKET_MODE_ONE_AXIS_VALUE_COUNT = 2,
};

typedef struct {
    uint8_t values[2];
    uint8_t enabled;
    uint8_t latch_flags;
    uint8_t x;
    uint8_t y;
    uint8_t mode;
    uint8_t packed_values;
} WheelPacketModeOneControls;

typedef struct {
    uint8_t buttons[WHEEL_PACKET_MODE_ONE_BUTTON_COUNT];
    uint8_t axis_outputs[WHEEL_PACKET_MODE_ONE_AXIS_OUTPUT_COUNT];
    int8_t motion;
    WheelPacketModeOneControls controls;
    uint16_t axis_values[WHEEL_PACKET_MODE_ONE_AXIS_VALUE_COUNT];
    uint8_t mode_buttons;
    uint8_t axis_report_enabled;
    uint16_t capability_flags;
    uint8_t axis_limit;
} WheelPacketModeOneInput;

typedef struct {
    uint8_t samples[WHEEL_PACKET_MODE_ONE_BUTTON_HISTORY_DEPTH][WHEEL_PACKET_MODE_ONE_BUTTON_COUNT];
    uint8_t next_sample;
} WheelPacketModeOneButtonFilter;

typedef struct {
    uint8_t samples[WHEEL_PACKET_MODE_ONE_CONTROL_AXIS_HISTORY_DEPTH]
                   [WHEEL_PACKET_MODE_ONE_CONTROL_AXIS_COUNT];
    uint8_t next_sample;
} WheelPacketModeOneControlAxisFilter;

typedef struct {
    WheelDisplayOutput display;
    uint8_t operating_mode;
    uint8_t display_state[2];
    uint8_t link_status[2];
} WheelPacketModeOneOutput;

bool wheel_packet_mode_one_applies(uint8_t wheel_mode);
void wheel_packet_mode_one_button_filter_init(WheelPacketModeOneButtonFilter *filter);
void wheel_packet_mode_one_filter_buttons(WheelPacketModeOneButtonFilter *filter,
                                          WheelPacketModeOneInput *input);
void wheel_packet_mode_one_control_axis_filter_init(WheelPacketModeOneControlAxisFilter *filter);
void wheel_packet_mode_one_filter_control_axes(WheelPacketModeOneControlAxisFilter *filter,
                                               WheelPacketModeOneInput *input);
void wheel_packet_mode_one_decode(const uint8_t request[WHEEL_PACKET_MODE_ONE_REQUEST_SIZE],
                                  WheelPacketModeOneInput *input);
void wheel_packet_mode_one_encode(const WheelPacketModeOneOutput *output,
                                  uint8_t response[WHEEL_PACKET_MODE_ONE_RESPONSE_SIZE]);

#endif
