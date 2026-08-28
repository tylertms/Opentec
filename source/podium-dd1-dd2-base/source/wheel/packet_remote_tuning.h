#ifndef OPENTEC_BASE_WHEEL_PACKET_REMOTE_TUNING_H
#define OPENTEC_BASE_WHEEL_PACKET_REMOTE_TUNING_H

#include <stdbool.h>
#include <stdint.h>

#include "remote_tuning/response.h"

enum {
    WHEEL_PACKET_REMOTE_TUNING_SIZE = 33,
};

/** @brief Pending remote-tuning response for an attached wheel. */
typedef struct {
    RemoteTuningResponse response;
} WheelPacketRemoteTuningOutput;

void wheel_packet_remote_tuning_init(WheelPacketRemoteTuningOutput *output);
bool wheel_packet_remote_tuning_queue(WheelPacketRemoteTuningOutput *output,
                                      const RemoteTuningResponse *response);
bool wheel_packet_remote_tuning_pending(const WheelPacketRemoteTuningOutput *output);
bool wheel_packet_remote_tuning_encode(WheelPacketRemoteTuningOutput *output,
                                       uint8_t packet[WHEEL_PACKET_REMOTE_TUNING_SIZE]);

#endif
