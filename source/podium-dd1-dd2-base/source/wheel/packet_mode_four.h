#ifndef OPENTEC_BASE_WHEEL_PACKET_MODE_FOUR_H
#define OPENTEC_BASE_WHEEL_PACKET_MODE_FOUR_H

#include <stdint.h>

#include "wheel/display_output.h"

enum {
    WHEEL_PACKET_MODE_FOUR_RESPONSE_SIZE = 9,
    WHEEL_PACKET_MODE_FOUR_REQUEST_SIZE = 32,
    WHEEL_PACKET_MODE_FOUR_SNAPSHOT_SIZE = 30,
    WHEEL_PACKET_MODE_FOUR_BUTTON_COUNT = 3,
    WHEEL_PACKET_MODE_FOUR_BUTTON_HISTORY_DEPTH = 3,
    WHEEL_PACKET_MODE_FOUR_CONTROL_COUNT = 4,
    WHEEL_PACKET_MODE_FOUR_CONTROL_HISTORY_DEPTH = 4,
    WHEEL_PACKET_MODE_FOUR_AXIS_OUTPUT_COUNT = 2,
    WHEEL_PACKET_MODE_FOUR_AXIS_VALUE_COUNT = 2,
    WHEEL_PACKET_MODE_FOUR_CONTROL_DATA_COUNT = 4,
    WHEEL_PACKET_MODE_FOUR_AUXILIARY_DATA_COUNT = 4,
};

typedef struct {
    uint8_t buttons[WHEEL_PACKET_MODE_FOUR_BUTTON_COUNT];
    uint8_t axis_outputs[WHEEL_PACKET_MODE_FOUR_AXIS_OUTPUT_COUNT];
    int8_t motion;
    uint8_t controls[WHEEL_PACKET_MODE_FOUR_CONTROL_COUNT];
    uint8_t control_data[WHEEL_PACKET_MODE_FOUR_CONTROL_DATA_COUNT];
    uint8_t reserved_axes[2];
    uint16_t axis_values[WHEEL_PACKET_MODE_FOUR_AXIS_VALUE_COUNT];
    uint8_t mode_buttons;
    uint8_t axis_report_enabled;
    uint8_t auxiliary_data[WHEEL_PACKET_MODE_FOUR_AUXILIARY_DATA_COUNT];
    uint8_t report_mode;
    uint8_t reserved_report;
    uint8_t report_capabilities;
    uint8_t axis_limit;
} WheelPacketModeFourInput;

typedef struct {
    uint8_t button_samples[WHEEL_PACKET_MODE_FOUR_BUTTON_HISTORY_DEPTH]
                          [WHEEL_PACKET_MODE_FOUR_BUTTON_COUNT];
    uint8_t control_samples[WHEEL_PACKET_MODE_FOUR_CONTROL_HISTORY_DEPTH]
                           [WHEEL_PACKET_MODE_FOUR_CONTROL_COUNT];
    uint8_t next_button_sample;
    uint8_t next_control_sample;
} WheelPacketModeFourFilter;

typedef struct {
    uint8_t extended_buttons;
    uint8_t axis_report_enabled;
} WheelPacketModeFourRuntime;

typedef struct {
    WheelDisplayOutput display;
    uint8_t display_state[2];
    uint8_t legacy_axes[2];
} WheelPacketModeFourOutput;

void wheel_packet_mode_four_filter_init(WheelPacketModeFourFilter *filter);
void wheel_packet_mode_four_decode(const uint8_t request[WHEEL_PACKET_MODE_FOUR_REQUEST_SIZE],
                                   WheelPacketModeFourInput *input);
void wheel_packet_mode_four_filter(WheelPacketModeFourFilter *filter,
                                   WheelPacketModeFourInput *input);
void wheel_packet_mode_four_normalize(WheelPacketModeFourInput *input, uint8_t interface_mode,
                                      WheelPacketModeFourRuntime *runtime,
                                      uint8_t snapshot[WHEEL_PACKET_MODE_FOUR_SNAPSHOT_SIZE]);
void wheel_packet_mode_four_encode(const WheelPacketModeFourOutput *output,
                                   uint8_t response[WHEEL_PACKET_MODE_FOUR_RESPONSE_SIZE]);

#endif
