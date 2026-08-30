#ifndef OPENTEC_BASE_PEDAL_TRANSFER_QUEUE_H
#define OPENTEC_BASE_PEDAL_TRANSFER_QUEUE_H

#include <stdbool.h>
#include <stdint.h>

enum {
    PEDAL_TRANSFER_PAYLOAD_CAPACITY = 124,
    PEDAL_TRANSFER_QUEUE_CAPACITY = 11,
};

/**
 * @brief Stores one complete host request for the V4 pedal controller.
 *
 * Keeps the logical payload independent of its USB and pedal-link framing.
 */
typedef struct {
    uint8_t data[PEDAL_TRANSFER_PAYLOAD_CAPACITY];
    uint8_t length;
} PedalTransferRequest;

/**
 * @brief Retains host pedal requests in arrival order.
 *
 * Tracks the front request separately while it awaits a pedal response and exposes eleven usable
 * request slots.
 */
typedef struct {
    PedalTransferRequest requests[PEDAL_TRANSFER_QUEUE_CAPACITY];
    uint8_t read_index;
    uint8_t write_index;
    uint8_t count;
    bool active;
} PedalTransferQueue;

void pedal_transfer_queue_init(PedalTransferQueue *queue);
bool pedal_transfer_queue_push(PedalTransferQueue *queue, const uint8_t *data, uint8_t length);
const PedalTransferRequest *pedal_transfer_queue_front(const PedalTransferQueue *queue);
void pedal_transfer_queue_start(PedalTransferQueue *queue);
void pedal_transfer_queue_finish(PedalTransferQueue *queue);
bool pedal_transfer_queue_active(const PedalTransferQueue *queue);

#endif
