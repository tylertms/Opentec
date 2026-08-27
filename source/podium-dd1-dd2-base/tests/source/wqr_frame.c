#include <assert.h>
#include <common/wqr_frame.h>
#include <string.h>

static void test_crc(void) {
    static const uint8_t value[] = "123456789";

    assert(wqr_frame_crc(value, sizeof(value) - 1) == 0x2189);
}

static void test_round_trip(void) {
    const uint8_t payload[] = {0x12, 0x34, 0x56};
    uint8_t frame[WQR_FRAME_SIZE];
    wqr_frame_view view;

    assert(wqr_frame_build(frame, WQR_FRAME_FIRST | 2, 47, payload, sizeof(payload)));
    assert(frame[0] == 0x7b);
    assert(frame[63] == 0x7d);
    assert(wqr_frame_parse(frame, &view));
    assert(view.type_flags == (WQR_FRAME_FIRST | 2));
    assert(view.sequence == 47);
    assert(view.payload_length == sizeof(payload));
    assert(memcmp(view.payload, payload, sizeof(payload)) == 0);
}

static void test_empty_frame(void) {
    uint8_t frame[WQR_FRAME_SIZE];
    wqr_frame_view view;

    assert(wqr_frame_build(frame, 5, 0, NULL, 0));
    assert(wqr_frame_parse(frame, &view));
    assert(view.payload_length == 0);
}

static void test_build_rejects_invalid_arguments(void) {
    uint8_t frame[WQR_FRAME_SIZE];
    uint8_t payload[WQR_FRAME_PAYLOAD_SIZE + 1] = {0};

    assert(!wqr_frame_build(NULL, 0, 0, NULL, 0));
    assert(!wqr_frame_build(frame, 0, 0, NULL, 1));
    assert(!wqr_frame_build(frame, 0, 0, payload, sizeof(payload)));
}

static void test_parse_rejects_corruption(void) {
    uint8_t frame[WQR_FRAME_SIZE];
    wqr_frame_view view;

    assert(wqr_frame_build(frame, 5, 9, NULL, 0));
    frame[0] = 0;
    assert(!wqr_frame_parse(frame, &view));

    assert(wqr_frame_build(frame, 5, 9, NULL, 0));
    frame[63] = 0;
    assert(!wqr_frame_parse(frame, &view));

    assert(wqr_frame_build(frame, 5, 9, NULL, 0));
    frame[3] = WQR_FRAME_PAYLOAD_SIZE + 1;
    assert(!wqr_frame_parse(frame, &view));

    assert(wqr_frame_build(frame, 5, 9, NULL, 0));
    frame[4] ^= 1;
    assert(!wqr_frame_parse(frame, &view));

    assert(!wqr_frame_parse(NULL, &view));
    assert(!wqr_frame_parse(frame, NULL));
}

int main(void) {
    test_crc();
    test_round_trip();
    test_empty_frame();
    test_build_rejects_invalid_arguments();
    test_parse_rejects_corruption();
    return 0;
}
