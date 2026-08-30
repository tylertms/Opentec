#include <assert.h>
#include <stdint.h>
#include <string.h>

#include "pedal/transfer_queue.h"

static void test_retains_requests_in_order(void) {
    PedalTransferQueue queue;
    const uint8_t first[] = {1, 2, 3};
    const uint8_t second[] = {4, 5};
    pedal_transfer_queue_init(&queue);

    assert(pedal_transfer_queue_push(&queue, first, sizeof(first)));
    assert(pedal_transfer_queue_push(&queue, second, sizeof(second)));
    const PedalTransferRequest *request = pedal_transfer_queue_front(&queue);
    assert(request != NULL && request->length == sizeof(first));
    assert(memcmp(request->data, first, sizeof(first)) == 0);

    pedal_transfer_queue_start(&queue);
    assert(pedal_transfer_queue_active(&queue));
    assert(pedal_transfer_queue_front(&queue) == NULL);
    pedal_transfer_queue_finish(&queue);
    request = pedal_transfer_queue_front(&queue);
    assert(request != NULL && request->length == sizeof(second));
    assert(memcmp(request->data, second, sizeof(second)) == 0);
}

static void test_exposes_eleven_request_slots(void) {
    PedalTransferQueue queue;
    uint8_t value = 0;
    pedal_transfer_queue_init(&queue);

    for (uint8_t index = 0; index < PEDAL_TRANSFER_QUEUE_CAPACITY; index++) {
        value = index;
        assert(pedal_transfer_queue_push(&queue, &value, 1));
    }
    assert(!pedal_transfer_queue_push(&queue, &value, 1));

    for (uint8_t index = 0; index < PEDAL_TRANSFER_QUEUE_CAPACITY; index++) {
        const PedalTransferRequest *request = pedal_transfer_queue_front(&queue);
        assert(request != NULL && request->data[0] == index);
        pedal_transfer_queue_start(&queue);
        pedal_transfer_queue_finish(&queue);
    }
    assert(pedal_transfer_queue_front(&queue) == NULL);
}

static void test_rejects_invalid_payloads(void) {
    PedalTransferQueue queue;
    uint8_t payload[PEDAL_TRANSFER_PAYLOAD_CAPACITY + 1] = {0};
    pedal_transfer_queue_init(&queue);

    assert(!pedal_transfer_queue_push(&queue, NULL, 1));
    assert(!pedal_transfer_queue_push(&queue, payload, 0));
    assert(!pedal_transfer_queue_push(&queue, payload, sizeof(payload)));
    assert(!pedal_transfer_queue_active(&queue));
}

int main(void) {
    test_retains_requests_in_order();
    test_exposes_eleven_request_slots();
    test_rejects_invalid_payloads();
    return 0;
}
