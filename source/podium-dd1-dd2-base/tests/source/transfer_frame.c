#include <assert.h>
#include <stdint.h>
#include <string.h>

#include "transfer/frame.h"

static void test_packs_commands(void) {
    uint16_t data = transfer_data_command(2, 3, 5);
    assert(data == 0x061d);
    assert(transfer_command_group(data) == 2);
    assert(transfer_command_sequence(data) == 3);
    assert(transfer_command_parameter(data) == 5);
    assert(transfer_status_command(3, 6) == 0x0b06);
    assert(transfer_progress_command(1, 2, 0x1f) == 0x09fa);
}

static void test_encodes_and_decodes_markers(void) {
    const TransferFrame source = {
        .command = 0x043c,
        .payload = {0x3c, 0x3d, 0x3e, 0x55},
        .payload_length = 4,
    };
    const uint8_t expected[] = {0x3c, 0x04, 0x3d, 0x28, 0x3d, 0x28, 0x3d,
                                0x3d, 0x3d, 0x29, 0x55, 0xae, 0x3e};
    uint8_t encoded[TRANSFER_FRAME_MAX_ENCODED_SIZE];
    TransferFrame decoded;

    uint16_t encoded_length = transfer_frame_encode(&source, encoded);
    assert(encoded_length == sizeof(expected));
    assert(memcmp(encoded, expected, sizeof(expected)) == 0);
    assert(transfer_frame_decode(encoded, encoded_length, &decoded) == TRANSFER_FRAME_VALID);
    assert(decoded.command == source.command);
    assert(decoded.payload_length == source.payload_length);
    assert(memcmp(decoded.payload, source.payload, source.payload_length) == 0);
}

static void test_rejects_invalid_frames(void) {
    TransferFrame frame;
    const uint8_t short_frame[] = {0x3c, 0, 0, 0x3e};
    const uint8_t bad_boundary[] = {0x3b, 0, 0, 0, 0x3e};
    const uint8_t bad_escape[] = {0x3c, 0, 0, 0x3d, 0x27, 0x3e};
    const uint8_t bad_checksum[] = {0x3c, 0, 0, 0, 0x3e};

    assert(transfer_frame_decode(short_frame, sizeof(short_frame), &frame) ==
           TRANSFER_FRAME_INVALID_LENGTH);
    assert(transfer_frame_decode(bad_boundary, sizeof(bad_boundary), &frame) ==
           TRANSFER_FRAME_INVALID_BOUNDARY);
    assert(transfer_frame_decode(bad_escape, sizeof(bad_escape), &frame) ==
           TRANSFER_FRAME_INVALID_ESCAPE);
    assert(transfer_frame_decode(bad_checksum, sizeof(bad_checksum), &frame) ==
           TRANSFER_FRAME_INVALID_CHECKSUM);
}

static void test_enforces_payload_limits(void) {
    TransferFrame source = {
        .command = 0x0400,
        .payload_length = TRANSFER_FRAME_MAX_SEND_PAYLOAD_SIZE + 1,
    };
    uint8_t encoded[TRANSFER_FRAME_MAX_ENCODED_SIZE];
    assert(transfer_frame_encode(&source, encoded) == 0);
}

int main(void) {
    test_packs_commands();
    test_encodes_and_decodes_markers();
    test_rejects_invalid_frames();
    test_enforces_payload_limits();
    return 0;
}
