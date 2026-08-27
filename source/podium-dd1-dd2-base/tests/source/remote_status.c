#include <assert.h>
#include <common/wqr_frame.h>
#include <wheel/remote_status.h>

static void test_decode(void) {
    const uint8_t payload[WHEEL_REMOTE_STATUS_SIZE] = {
        7, 5, 0x85, 0xff, 0x78, 0x56, 0x34, 0x12, 0xef, 0xcd, 0xab, 0x90, 4, 1, 0xaa,
    };
    wheel_remote_status status;

    assert(wheel_remote_status_decode(&status, payload, sizeof(payload)));
    assert(status.firmware_revision == 7);
    assert(status.hardware_revision == 5);
    assert(status.temperature_c == -123);
    assert(status.uptime_seconds == 0x12345678);
    assert(status.communication_errors == 0x90abcdef);
    assert(status.link_state == WHEEL_REMOTE_LINK_READY);
    assert(status.transfer_mode == 1);
    assert(status.reset_acknowledged);
}

static void test_decode_accepts_extensions(void) {
    uint8_t payload[WHEEL_REMOTE_STATUS_SIZE + 2] = {0};
    wheel_remote_status status;

    payload[12] = WHEEL_REMOTE_LINK_DETECTED;
    assert(wheel_remote_status_decode(&status, payload, sizeof(payload)));
    assert(status.link_state == WHEEL_REMOTE_LINK_DETECTED);
    assert(!status.reset_acknowledged);
}

static void test_decode_rejects_invalid_arguments(void) {
    uint8_t payload[WHEEL_REMOTE_STATUS_SIZE] = {0};
    wheel_remote_status status;

    assert(!wheel_remote_status_decode(NULL, payload, sizeof(payload)));
    assert(!wheel_remote_status_decode(&status, NULL, sizeof(payload)));
    assert(!wheel_remote_status_decode(&status, payload, sizeof(payload) - 1));
}

static void test_status_exchange(void) {
    const uint8_t payload[WHEEL_REMOTE_STATUS_SIZE] = {
        7, 3, 25, 0, 9, 0, 0, 0, 2, 0, 0, 0, 1, 0, 0,
    };
    wheel_remote_exchange exchange;
    wheel_remote_status status;
    uint8_t frame[WQR_FRAME_SIZE];
    wqr_frame_view view;

    wheel_remote_exchange_init(&exchange, 20, 2);
    assert(wheel_remote_status_begin(&exchange, 14, false));
    assert(wheel_remote_exchange_next_frame(&exchange, frame));
    assert(wqr_frame_parse(frame, &view));
    assert(view.type_flags == WQR_PAYLOAD_STATUS);
    assert(view.sequence == 14);
    assert(view.payload_length == 0);

    assert(wqr_frame_build(frame, WQR_PAYLOAD_STATUS, 15, payload, sizeof(payload)));
    assert(wheel_remote_exchange_receive(&exchange, frame));
    assert(wheel_remote_exchange_next_frame(&exchange, frame));
    assert(wheel_remote_status_finish(&exchange, &status));
    assert(status.hardware_revision == 3);
    assert(status.temperature_c == 25);
    assert(status.uptime_seconds == 9);
    assert(status.communication_errors == 2);
    assert(status.link_state == WHEEL_REMOTE_LINK_WAITING);
}

static void test_reset_exchange(void) {
    wheel_remote_exchange exchange;
    uint8_t frame[WQR_FRAME_SIZE];
    wqr_frame_view view;

    wheel_remote_exchange_init(&exchange, 20, 2);
    assert(wheel_remote_status_begin(&exchange, 1, true));
    assert(wheel_remote_exchange_next_frame(&exchange, frame));
    assert(wqr_frame_parse(frame, &view));
    assert(view.payload_length == 1);
    assert(view.payload[0] == 0xaa);
}

int main(void) {
    test_decode();
    test_decode_accepts_extensions();
    test_decode_rejects_invalid_arguments();
    test_status_exchange();
    test_reset_exchange();
    return 0;
}
