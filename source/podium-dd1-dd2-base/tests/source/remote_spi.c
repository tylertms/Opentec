#include <assert.h>
#include <common/wqr_frame.h>
#include <string.h>
#include <wheel/remote_spi.h>

static void complete(wheel_remote_exchange *exchange, uint8_t frame[WQR_FRAME_SIZE],
                     uint8_t sequence, uint8_t payload_type, const uint8_t *response) {
    assert(wqr_frame_build(frame, payload_type, sequence, response, WQR_FRAME_PAYLOAD_SIZE));
    assert(wheel_remote_exchange_receive(exchange, frame));
    assert(wheel_remote_exchange_next_frame(exchange, frame));
}

static void test_primary(void) {
    const uint8_t transmit[] = {1, 2, 3, 4};
    uint8_t response[WQR_FRAME_PAYLOAD_SIZE] = {9, 8, 7};
    wheel_remote_exchange exchange;
    wheel_remote_spi_result result;
    uint8_t frame[WQR_FRAME_SIZE];
    wqr_frame_view view;

    wheel_remote_exchange_init(&exchange, 20, 2);
    assert(wheel_remote_spi_begin(&exchange, 4, WHEEL_REMOTE_SPI_PRIMARY, transmit,
                                  sizeof(transmit), true));
    assert(wheel_remote_exchange_next_frame(&exchange, frame));
    assert(wqr_frame_parse(frame, &view));
    assert(view.type_flags == WQR_PAYLOAD_PRIMARY_SPI);
    assert(view.payload_length == WQR_FRAME_PAYLOAD_SIZE);
    assert(memcmp(view.payload, transmit, sizeof(transmit)) == 0);
    assert(view.payload[WQR_FRAME_PAYLOAD_SIZE - 1] == 1);

    response[WQR_FRAME_PAYLOAD_SIZE - 1] = 2;
    complete(&exchange, frame, 5, WQR_PAYLOAD_PRIMARY_SPI, response);
    assert(wheel_remote_spi_finish(&exchange, &result));
    assert(result.data_length == WHEEL_REMOTE_SPI_TRANSFER_SIZE);
    assert(result.peer_detected);
    assert(memcmp(result.data, response, WHEEL_REMOTE_SPI_TRANSFER_SIZE) == 0);
}

static void test_alternate(void) {
    const uint8_t transmit[] = {0x34, 0x12};
    uint8_t response[WQR_FRAME_PAYLOAD_SIZE] = {0xcd, 0xab};
    wheel_remote_exchange exchange;
    wheel_remote_spi_result result;
    uint8_t frame[WQR_FRAME_SIZE];
    wqr_frame_view view;

    wheel_remote_exchange_init(&exchange, 20, 2);
    assert(wheel_remote_spi_begin(&exchange, 9, WHEEL_REMOTE_SPI_ALTERNATE, transmit,
                                  sizeof(transmit), false));
    assert(wheel_remote_exchange_next_frame(&exchange, frame));
    assert(wqr_frame_parse(frame, &view));
    assert(view.type_flags == WQR_PAYLOAD_ALTERNATE_SPI);
    assert(view.payload[0] == 0x34);
    assert(view.payload[1] == 0x12);
    assert(view.payload[WQR_FRAME_PAYLOAD_SIZE - 1] == 0);

    complete(&exchange, frame, 10, WQR_PAYLOAD_ALTERNATE_SPI, response);
    assert(wheel_remote_spi_finish(&exchange, &result));
    assert(result.data_length == 2);
    assert(!result.peer_detected);
    assert(result.data[0] == 0xcd);
    assert(result.data[1] == 0xab);
}

static void test_validation(void) {
    wheel_remote_exchange exchange;
    wheel_remote_spi_result result;
    uint8_t data[WHEEL_REMOTE_SPI_TRANSFER_SIZE + 1] = {0};

    wheel_remote_exchange_init(&exchange, 20, 2);
    assert(!wheel_remote_spi_begin(&exchange, 0, (wheel_remote_spi_port)2, data, 1, false));
    assert(!wheel_remote_spi_begin(&exchange, 0, WHEEL_REMOTE_SPI_PRIMARY, NULL, 1, false));
    assert(
        !wheel_remote_spi_begin(&exchange, 0, WHEEL_REMOTE_SPI_PRIMARY, data, sizeof(data), false));
    assert(!wheel_remote_spi_begin(&exchange, 0, WHEEL_REMOTE_SPI_ALTERNATE, data, 3, false));
    assert(!wheel_remote_spi_finish(&exchange, &result));
    assert(!wheel_remote_spi_finish(&exchange, NULL));
}

int main(void) {
    test_primary();
    test_alternate();
    test_validation();
    return 0;
}
