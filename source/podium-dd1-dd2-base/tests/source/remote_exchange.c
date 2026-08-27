#include <assert.h>
#include <common/wqr_frame.h>
#include <string.h>
#include <wheel/remote_exchange.h>

static wqr_frame_view parse(const uint8_t frame[WQR_FRAME_SIZE]) {
    wqr_frame_view view;

    assert(wqr_frame_parse(frame, &view));
    return view;
}

static void fill(uint8_t *data, size_t length, uint8_t seed) {
    size_t index;

    for (index = 0; index < length; ++index) {
        data[index] = (uint8_t)(seed + index * 17);
    }
}

static void test_single_frame_exchange(void) {
    wheel_remote_exchange exchange;
    uint8_t request[] = {0xaa};
    uint8_t response[] = {1, 2, 3};
    uint8_t frame[WQR_FRAME_SIZE];
    const uint8_t *result;
    size_t result_length;
    wqr_frame_view view;

    wheel_remote_exchange_init(&exchange, 20, 2);
    assert(wheel_remote_exchange_start(&exchange, WQR_PAYLOAD_STATUS, 7, request, sizeof(request)));
    assert(wheel_remote_exchange_next_frame(&exchange, frame));
    view = parse(frame);
    assert(view.type_flags == WQR_PAYLOAD_STATUS);
    assert(view.sequence == 7);
    assert(view.payload_length == sizeof(request));
    assert(memcmp(view.payload, request, sizeof(request)) == 0);

    assert(wqr_frame_build(frame, WQR_PAYLOAD_STATUS, 8, response, sizeof(response)));
    assert(wheel_remote_exchange_receive(&exchange, frame));
    assert(exchange.state == WHEEL_REMOTE_ACK_READY);
    assert(wheel_remote_exchange_next_frame(&exchange, frame));
    view = parse(frame);
    assert(view.type_flags == WQR_FRAME_ACK);
    assert(view.sequence == 8);
    assert(exchange.state == WHEEL_REMOTE_COMPLETE);

    result = wheel_remote_exchange_response(&exchange, &result_length);
    assert(result != NULL);
    assert(result_length == sizeof(response));
    assert(memcmp(result, response, sizeof(response)) == 0);
}

static void acknowledge_request(wheel_remote_exchange *exchange, uint8_t frame[WQR_FRAME_SIZE],
                                uint8_t sequence) {
    uint8_t payload = exchange->payload_type;

    assert(wqr_frame_build(frame, WQR_FRAME_ACK, sequence, &payload, 1));
    assert(wheel_remote_exchange_receive(exchange, frame));
}

static void test_fragmented_request(void) {
    wheel_remote_exchange exchange;
    uint8_t request[120];
    uint8_t response[] = {9};
    uint8_t frame[WQR_FRAME_SIZE];
    wqr_frame_view view;

    fill(request, sizeof(request), 3);
    wheel_remote_exchange_init(&exchange, 20, 2);
    assert(wheel_remote_exchange_start(&exchange, WQR_PAYLOAD_I2C, 250, request, sizeof(request)));

    assert(wheel_remote_exchange_next_frame(&exchange, frame));
    view = parse(frame);
    assert(view.type_flags == (WQR_FRAME_FIRST | WQR_PAYLOAD_I2C));
    assert(view.sequence == 250);
    assert(view.payload_length == WQR_FRAME_PAYLOAD_SIZE);
    acknowledge_request(&exchange, frame, 251);

    assert(wheel_remote_exchange_next_frame(&exchange, frame));
    view = parse(frame);
    assert(view.type_flags == (WQR_FRAME_MORE | WQR_PAYLOAD_I2C));
    assert(view.sequence == 251);
    assert(view.payload_length == WQR_FRAME_PAYLOAD_SIZE);
    acknowledge_request(&exchange, frame, 252);

    assert(wheel_remote_exchange_next_frame(&exchange, frame));
    view = parse(frame);
    assert(view.type_flags == (WQR_FRAME_LAST | WQR_PAYLOAD_I2C));
    assert(view.sequence == 252);
    assert(view.payload_length == 6);
    assert(memcmp(view.payload, request + 114, 6) == 0);

    assert(wqr_frame_build(frame, WQR_PAYLOAD_I2C, 253, response, sizeof(response)));
    assert(wheel_remote_exchange_receive(&exchange, frame));
    assert(wheel_remote_exchange_next_frame(&exchange, frame));
    assert(exchange.state == WHEEL_REMOTE_COMPLETE);
}

static void accept_response_fragment(wheel_remote_exchange *exchange, uint8_t frame[WQR_FRAME_SIZE],
                                     uint8_t type_flags, uint8_t sequence, const uint8_t *payload,
                                     size_t length) {
    wqr_frame_view ack;

    assert(wqr_frame_build(frame, type_flags, sequence, payload, length));
    assert(wheel_remote_exchange_receive(exchange, frame));
    assert(wheel_remote_exchange_next_frame(exchange, frame));
    ack = parse(frame);
    assert(ack.type_flags == WQR_FRAME_ACK);
    assert(ack.sequence == sequence);
}

static void test_fragmented_response_and_sequence_wrap(void) {
    wheel_remote_exchange exchange;
    uint8_t response[120];
    uint8_t frame[WQR_FRAME_SIZE];
    const uint8_t *result;
    size_t result_length;

    fill(response, sizeof(response), 11);
    wheel_remote_exchange_init(&exchange, 20, 2);
    assert(wheel_remote_exchange_start(&exchange, WQR_PAYLOAD_I2C, 254, NULL, 0));
    assert(wheel_remote_exchange_next_frame(&exchange, frame));

    accept_response_fragment(&exchange, frame, WQR_FRAME_FIRST | WQR_PAYLOAD_I2C, 255, response,
                             57);
    assert(exchange.state == WHEEL_REMOTE_WAITING);
    accept_response_fragment(&exchange, frame, WQR_FRAME_MORE | WQR_PAYLOAD_I2C, 0, response + 57,
                             57);
    assert(exchange.state == WHEEL_REMOTE_WAITING);
    accept_response_fragment(&exchange, frame, WQR_FRAME_LAST | WQR_PAYLOAD_I2C, 1, response + 114,
                             6);
    assert(exchange.state == WHEEL_REMOTE_COMPLETE);

    result = wheel_remote_exchange_response(&exchange, &result_length);
    assert(result_length == sizeof(response));
    assert(memcmp(result, response, sizeof(response)) == 0);
}

static void test_retry_and_timeout(void) {
    wheel_remote_exchange exchange;
    uint8_t request[60] = {0};
    uint8_t frame[WQR_FRAME_SIZE];
    uint8_t first_frame[WQR_FRAME_SIZE];
    wqr_frame_view view;

    wheel_remote_exchange_init(&exchange, 10, 2);
    assert(wheel_remote_exchange_start(&exchange, WQR_PAYLOAD_I2C, 3, request, sizeof(request)));
    assert(wheel_remote_exchange_next_frame(&exchange, first_frame));
    assert(wqr_frame_build(frame, WQR_FRAME_ACK, 4, NULL, 0));
    assert(wheel_remote_exchange_receive(&exchange, frame));
    assert(exchange.state == WHEEL_REMOTE_REQUEST_READY);
    assert(wheel_remote_exchange_next_frame(&exchange, frame));
    assert(memcmp(frame, first_frame, sizeof(frame)) == 0);

    acknowledge_request(&exchange, frame, 4);
    assert(wheel_remote_exchange_next_frame(&exchange, frame));
    wheel_remote_exchange_tick(&exchange, 9);
    assert(exchange.state == WHEEL_REMOTE_WAITING);
    wheel_remote_exchange_tick(&exchange, 1);
    assert(exchange.state == WHEEL_REMOTE_NACK_READY);
    assert(wheel_remote_exchange_next_frame(&exchange, frame));
    view = parse(frame);
    assert(view.type_flags == WQR_FRAME_NACK);
    assert(view.sequence == 4);
}

static void test_response_capacity(void) {
    wheel_remote_exchange exchange;
    uint8_t payload[WQR_FRAME_PAYLOAD_SIZE] = {0};
    uint8_t frame[WQR_FRAME_SIZE];
    uint8_t fragment;

    wheel_remote_exchange_init(&exchange, 10, 2);
    assert(wheel_remote_exchange_start(&exchange, WQR_PAYLOAD_I2C, 0, NULL, 0));
    assert(wheel_remote_exchange_next_frame(&exchange, frame));

    for (fragment = 0; fragment < 8; ++fragment) {
        uint8_t flags = fragment == 0 ? WQR_FRAME_FIRST : WQR_FRAME_MORE;

        accept_response_fragment(&exchange, frame, flags | WQR_PAYLOAD_I2C, (uint8_t)(fragment + 1),
                                 payload, sizeof(payload));
    }

    assert(wqr_frame_build(frame, WQR_FRAME_MORE | WQR_PAYLOAD_I2C, 9, payload, sizeof(payload)));
    assert(!wheel_remote_exchange_receive(&exchange, frame));
    assert(exchange.state == WHEEL_REMOTE_FAILED);
    assert(exchange.error == WHEEL_REMOTE_ERROR_OVERFLOW);
}

static void test_failures(void) {
    wheel_remote_exchange exchange;
    uint8_t request[WHEEL_REMOTE_EXCHANGE_CAPACITY + 1] = {0};
    uint8_t frame[WQR_FRAME_SIZE];

    wheel_remote_exchange_init(&exchange, 1, 0);
    assert(!wheel_remote_exchange_start(&exchange, WQR_FRAME_ACK, 0, NULL, 0));
    assert(exchange.error == WHEEL_REMOTE_ERROR_ARGUMENT);
    assert(
        !wheel_remote_exchange_start(&exchange, WQR_PAYLOAD_STATUS, 0, request, sizeof(request)));
    assert(wheel_remote_exchange_start(&exchange, WQR_PAYLOAD_STATUS, 0, NULL, 0));
    assert(!wheel_remote_exchange_start(&exchange, WQR_PAYLOAD_STATUS, 0, NULL, 0));
    assert(exchange.error == WHEEL_REMOTE_ERROR_BUSY);
    assert(wheel_remote_exchange_next_frame(&exchange, frame));
    wheel_remote_exchange_tick(&exchange, 1);
    assert(exchange.state == WHEEL_REMOTE_FAILED);
    assert(exchange.error == WHEEL_REMOTE_ERROR_TIMEOUT);
    assert(wheel_remote_exchange_response(&exchange, NULL) == NULL);
}

int main(void) {
    test_single_frame_exchange();
    test_fragmented_request();
    test_fragmented_response_and_sequence_wrap();
    test_retry_and_timeout();
    test_response_capacity();
    test_failures();
    return 0;
}
