#ifndef OPENTEC_BASE_WHEEL_PACKET_ALTERNATE_H
#define OPENTEC_BASE_WHEEL_PACKET_ALTERNATE_H

#include <stdbool.h>
#include <stdint.h>

#include "wheel/packet_common.h"

enum {
    WHEEL_PACKET_ALTERNATE_HISTORY_DEPTH = 3,
    WHEEL_PACKET_ALTERNATE_PAYLOAD_SIZE = 30,
    WHEEL_PACKET_ALTERNATE_RESPONSE_SIZE = 33,
};

/** @brief Logical input fields carried by an alternate mode packet. */
typedef WheelPacketCommonInput WheelPacketAlternateInput;

/** @brief Three-sample button filter used by alternate mode input. */
typedef struct {
    uint8_t samples[WHEEL_PACKET_ALTERNATE_HISTORY_DEPTH][WHEEL_PACKET_COMMON_BUTTON_COUNT];
    uint8_t next_sample;
} WheelPacketAlternateFilter;

/** @brief Retained display, command, and transfer state for alternate mode output. */
typedef struct {
    WheelDisplayOutput display;
    uint8_t payload[WHEEL_PACKET_ALTERNATE_PAYLOAD_SIZE];
    uint8_t sequence;
    uint8_t report_state;
    uint8_t auxiliary_link_option;
    bool auxiliary_status;
    bool suppress_auxiliary_display;
    bool command_restart_pending;
    bool payload_pending;
    bool payload_suppressed;
    bool transfer_active;
} WheelPacketAlternateOutput;

bool wheel_packet_alternate_applies(uint8_t wheel_mode);
void wheel_packet_alternate_filter_init(WheelPacketAlternateFilter *filter);
void wheel_packet_alternate_filter(WheelPacketAlternateFilter *filter,
                                   WheelPacketAlternateInput *input, uint8_t interface_mode);
bool wheel_packet_alternate_queue_payload(
    WheelPacketAlternateOutput *output, const uint8_t payload[WHEEL_PACKET_ALTERNATE_PAYLOAD_SIZE]);
bool wheel_packet_alternate_payload_pending(const WheelPacketAlternateOutput *output);
void wheel_packet_alternate_encode(WheelPacketAlternateOutput *output,
                                   uint8_t response[WHEEL_PACKET_ALTERNATE_RESPONSE_SIZE]);

#endif
