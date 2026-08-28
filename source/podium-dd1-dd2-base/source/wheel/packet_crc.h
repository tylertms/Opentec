#ifndef OPENTEC_BASE_WHEEL_PACKET_CRC_H
#define OPENTEC_BASE_WHEEL_PACKET_CRC_H

#include <stdbool.h>
#include <stdint.h>

#include "wheel/display_output.h"

enum {
    WHEEL_PACKET_CRC_RESPONSE_SIZE = 33,
    WHEEL_PACKET_CRC_REQUEST_SIZE = 33,
    WHEEL_PACKET_CRC_SNAPSHOT_SIZE = 30,
    WHEEL_PACKET_CRC_BUTTON_COUNT = 3,
    WHEEL_PACKET_CRC_FILTERED_CONTROL_COUNT = 5,
    WHEEL_PACKET_CRC_CONTROL_COUNT = 8,
    WHEEL_PACKET_CRC_HISTORY_DEPTH = 3,
    WHEEL_PACKET_CRC_AXIS_VALUE_COUNT = 2,
    WHEEL_PACKET_CRC_ADAPTER_ROTARY_COUNT = 3,
};

typedef struct {
    uint8_t buttons[WHEEL_PACKET_CRC_BUTTON_COUNT];
    uint8_t axis_outputs[2];
    int8_t motion;
    uint8_t controls[WHEEL_PACKET_CRC_CONTROL_COUNT];
    uint8_t reserved_axes[2];
    uint16_t axis_values[WHEEL_PACKET_CRC_AXIS_VALUE_COUNT];
    uint8_t mode_buttons;
    uint8_t axis_report_enabled;
    uint8_t auxiliary_data[4];
    uint8_t report_mode;
    uint8_t reserved_report;
    uint8_t report_capabilities;
    uint8_t axis_limit;
} WheelPacketCrcInput;

typedef struct {
    uint8_t button_samples[WHEEL_PACKET_CRC_HISTORY_DEPTH][WHEEL_PACKET_CRC_BUTTON_COUNT];
    uint8_t control_samples[WHEEL_PACKET_CRC_HISTORY_DEPTH]
                           [WHEEL_PACKET_CRC_FILTERED_CONTROL_COUNT];
    uint8_t axis_samples[WHEEL_PACKET_CRC_HISTORY_DEPTH][WHEEL_PACKET_CRC_AXIS_VALUE_COUNT];
    uint8_t next_button_sample;
    uint8_t next_control_sample;
    uint8_t next_axis_sample;
} WheelPacketCrcFilter;

typedef struct {
    WheelDisplayOutput display;
    uint8_t legacy_axes[2];
    uint8_t report_state;
    bool status_update_pending;
} WheelPacketCrcOutput;

typedef struct {
    uint8_t buttons[3];
    uint8_t axes[2];
    uint8_t rotary_positions[WHEEL_PACKET_CRC_ADAPTER_ROTARY_COUNT];
    uint16_t mode;
    int8_t primary_delta;
    bool connected;
    bool buttons_active;
} WheelPacketCrcAdapter;

bool wheel_packet_crc_applies(uint8_t wheel_mode);
void wheel_packet_crc_filter_init(WheelPacketCrcFilter *filter);
void wheel_packet_crc_decode(const uint8_t request[WHEEL_PACKET_CRC_REQUEST_SIZE],
                             WheelPacketCrcInput *input);
void wheel_packet_crc_prepare(WheelPacketCrcInput *input, uint8_t wheel_mode,
                              uint8_t interface_mode);
void wheel_packet_crc_filter(WheelPacketCrcFilter *filter, WheelPacketCrcInput *input);
void wheel_packet_crc_smooth_axes(WheelPacketCrcFilter *filter, WheelPacketCrcInput *input);
void wheel_packet_crc_normalize(WheelPacketCrcInput *input, uint8_t wheel_mode,
                                uint8_t interface_mode, WheelPacketCrcAdapter *adapter);
void wheel_packet_crc_snapshot(const WheelPacketCrcInput *input,
                               uint8_t snapshot[WHEEL_PACKET_CRC_SNAPSHOT_SIZE]);
void wheel_packet_crc_encode(uint8_t wheel_mode, WheelPacketCrcOutput *output,
                             uint8_t response[WHEEL_PACKET_CRC_RESPONSE_SIZE]);

#endif
