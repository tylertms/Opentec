#include <assert.h>
#include <common/wqr_frame.h>
#include <string.h>
#include <wheel/remote_i2c.h>

static wqr_frame_view next_request(wheel_remote_exchange *exchange, uint8_t frame[WQR_FRAME_SIZE]) {
    wqr_frame_view view;

    assert(wheel_remote_exchange_next_frame(exchange, frame));
    assert(wqr_frame_parse(frame, &view));
    return view;
}

static void complete(wheel_remote_exchange *exchange, uint8_t frame[WQR_FRAME_SIZE],
                     uint8_t sequence, const uint8_t *response, size_t response_length) {
    assert(wqr_frame_build(frame, WQR_PAYLOAD_I2C, sequence, response, response_length));
    assert(wheel_remote_exchange_receive(exchange, frame));
    assert(wheel_remote_exchange_next_frame(exchange, frame));
    assert(exchange->state == WHEEL_REMOTE_COMPLETE);
}

static void test_write(void) {
    const uint8_t data[] = {0x10, 0x22};
    const uint8_t response[] = {1, 0xa0, 0x10};
    wheel_remote_exchange exchange;
    wheel_remote_i2c_result result;
    uint8_t frame[WQR_FRAME_SIZE];
    wqr_frame_view view;

    wheel_remote_exchange_init(&exchange, 20, 2);
    assert(wheel_remote_i2c_write_begin(&exchange, 5, 0x50, data, sizeof(data)));
    view = next_request(&exchange, frame);
    assert(view.type_flags == WQR_PAYLOAD_I2C);
    assert(view.sequence == 5);
    assert(view.payload_length == 4);
    assert(view.payload[0] == 0);
    assert(view.payload[1] == 0xa0);
    assert(memcmp(view.payload + 2, data, sizeof(data)) == 0);

    complete(&exchange, frame, 6, response, sizeof(response));
    assert(wheel_remote_i2c_finish(&exchange, &result));
    assert(result.succeeded);
    assert(result.address == 0x50);
    assert(result.data_length == 1);
    assert(result.data[0] == 0x10);
}

static void test_read(void) {
    const uint8_t response[] = {1, 0xa1, 0x34, 0x12, 0x78};
    wheel_remote_exchange exchange;
    wheel_remote_i2c_result result;
    uint8_t frame[WQR_FRAME_SIZE];
    wqr_frame_view view;

    wheel_remote_exchange_init(&exchange, 20, 2);
    assert(wheel_remote_i2c_read_begin(&exchange, 9, 0x50, 0x20, 3));
    view = next_request(&exchange, frame);
    assert(view.payload_length == 5);
    assert(view.payload[0] == 0);
    assert(view.payload[1] == 0xa1);
    assert(view.payload[2] == 0x20);
    assert(view.payload[3] == 3);
    assert(view.payload[4] == 0);

    complete(&exchange, frame, 10, response, sizeof(response));
    assert(wheel_remote_i2c_finish(&exchange, &result));
    assert(result.succeeded);
    assert(result.address == 0x50);
    assert(result.data_length == 3);
    assert(memcmp(result.data, response + 2, 3) == 0);
}

static void test_failure(void) {
    const uint8_t response[] = {0, 0xa1, 0x20};
    wheel_remote_exchange exchange;
    wheel_remote_i2c_result result;
    uint8_t frame[WQR_FRAME_SIZE];

    wheel_remote_exchange_init(&exchange, 20, 2);
    assert(wheel_remote_i2c_read_begin(&exchange, 0, 0x50, 0x20, 3));
    next_request(&exchange, frame);
    complete(&exchange, frame, 1, response, sizeof(response));
    assert(wheel_remote_i2c_finish(&exchange, &result));
    assert(!result.succeeded);
    assert(result.data_length == 0);
}

static void test_validation(void) {
    wheel_remote_exchange exchange;
    wheel_remote_i2c_result result;
    uint8_t data[WHEEL_REMOTE_I2C_DATA_CAPACITY + 1] = {0};

    wheel_remote_exchange_init(&exchange, 20, 2);
    assert(!wheel_remote_i2c_write_begin(&exchange, 0, 0x80, data, 1));
    assert(!wheel_remote_i2c_write_begin(&exchange, 0, 0x50, NULL, 1));
    assert(!wheel_remote_i2c_write_begin(&exchange, 0, 0x50, data, 0));
    assert(!wheel_remote_i2c_write_begin(&exchange, 0, 0x50, data, sizeof(data)));
    assert(!wheel_remote_i2c_read_begin(&exchange, 0, 0x80, 0, 1));
    assert(!wheel_remote_i2c_read_begin(&exchange, 0, 0x50, 0, sizeof(data)));
    assert(!wheel_remote_i2c_finish(&exchange, &result));
    assert(!wheel_remote_i2c_finish(&exchange, NULL));
}

int main(void) {
    test_write();
    test_read();
    test_failure();
    test_validation();
    return 0;
}
