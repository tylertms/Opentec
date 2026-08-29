#ifndef OPENTEC_BASE_WHEEL_PACKET_CRC_H
#define OPENTEC_BASE_WHEEL_PACKET_CRC_H

#include <stdbool.h>
#include <stdint.h>

#include "wheel/adapter.h"
#include "wheel/display_output.h"
#include "wheel/packet_common.h"

enum {
    WHEEL_PACKET_CRC_RESPONSE_SIZE = 33,
    WHEEL_PACKET_CRC_REQUEST_SIZE = 33,
    WHEEL_PACKET_CRC_SNAPSHOT_SIZE = 30,
    WHEEL_PACKET_CRC_BUTTON_COUNT = 3,
    WHEEL_PACKET_CRC_FILTERED_CONTROL_COUNT = 5,
    WHEEL_PACKET_CRC_CONTROL_COUNT = 8,
    WHEEL_PACKET_CRC_HISTORY_DEPTH = 3,
    WHEEL_PACKET_CRC_AXIS_VALUE_COUNT = 2,
};

/** @brief Logical input carried by the CRC attached-wheel packet family. */
typedef WheelPacketCommonInput WheelPacketCrcInput;

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
    uint8_t vibration[2];
    uint8_t legacy_axes[2];
    uint8_t report_state;
    bool command_restart_pending;
    bool status_update_pending;
} WheelPacketCrcOutput;

bool wheel_packet_crc_applies(uint8_t wheel_mode);
void wheel_packet_crc_filter_init(WheelPacketCrcFilter *filter);
void wheel_packet_crc_decode(const uint8_t request[WHEEL_PACKET_CRC_REQUEST_SIZE],
                             WheelPacketCrcInput *input);
void wheel_packet_crc_prepare(WheelPacketCrcInput *input, uint8_t wheel_mode,
                              uint8_t interface_mode);
void wheel_packet_crc_filter(WheelPacketCrcFilter *filter, WheelPacketCrcInput *input,
                             uint8_t wheel_mode);
void wheel_packet_crc_smooth_axes(WheelPacketCrcFilter *filter, WheelPacketCrcInput *input);
void wheel_packet_crc_normalize(WheelPacketCrcInput *input, uint8_t wheel_mode,
                                uint8_t interface_mode, WheelAdapterInput *adapter);
void wheel_packet_crc_snapshot(const WheelPacketCrcInput *input,
                               uint8_t snapshot[WHEEL_PACKET_CRC_SNAPSHOT_SIZE]);
void wheel_packet_crc_encode(uint8_t wheel_mode, bool host_capability_enabled,
                             WheelPacketCrcOutput *output,
                             uint8_t response[WHEEL_PACKET_CRC_RESPONSE_SIZE]);

#endif
