#ifndef OPENTEC_BASE_WHEEL_PACKET_REMOTE_TUNING_H
#define OPENTEC_BASE_WHEEL_PACKET_REMOTE_TUNING_H

#include <stdbool.h>
#include <stdint.h>

#include "remote_tuning/response.h"

/** @brief Remote-tuning response dimensions. */
enum {
    WHEEL_PACKET_REMOTE_TUNING_SIZE = 33, /**< Encoded response size in bytes. */
};

/** @brief Pending remote-tuning response for an attached wheel. */
typedef struct {
    RemoteTuningResponse response; /**< Retained semantic response. */
} WheelPacketRemoteTuningOutput;

/**
 * @brief Initializes attached-wheel remote-tuning output.
 *
 * Clears the retained response and its pending state.
 *
 * @param[out] output Remote-tuning output state to initialize.
 */
void wheel_packet_remote_tuning_init(WheelPacketRemoteTuningOutput *output);

/**
 * @brief Queues an attached-wheel remote-tuning response.
 *
 * Retains a response when its link, code, and value can be represented by the wheel protocol.
 *
 * @param[in,out] output Remote-tuning output state to update.
 * @param[in] response Semantic response to retain.
 * @return True when the response was accepted; otherwise false.
 */
bool wheel_packet_remote_tuning_queue(WheelPacketRemoteTuningOutput *output,
                                      const RemoteTuningResponse *response);

/**
 * @brief Reports whether a remote-tuning response is pending.
 *
 * Reads the pending response state without consuming it.
 *
 * @param[in] output Remote-tuning output state to inspect.
 * @return True while a supported response is pending; otherwise false.
 */
bool wheel_packet_remote_tuning_pending(const WheelPacketRemoteTuningOutput *output);

/**
 * @brief Encodes one pending remote-tuning response.
 *
 * Writes the protocol response and clears the pending state after a successful encoding.
 *
 * @param[in,out] output Pending response state to consume.
 * @param[out] packet Thirty-three-byte response destination.
 * @return True when a response was encoded; otherwise false.
 */
bool wheel_packet_remote_tuning_encode(WheelPacketRemoteTuningOutput *output,
                                       uint8_t packet[WHEEL_PACKET_REMOTE_TUNING_SIZE]);

#endif
