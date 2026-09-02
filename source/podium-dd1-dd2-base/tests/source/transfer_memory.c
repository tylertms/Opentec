#include <assert.h>
#include <stdint.h>
#include <string.h>

#include "transfer/memory.h"

static void test_encodes_read_request(void) {
    uint8_t request[MEMORY_TRANSFER_READ_REQUEST_SIZE];

    assert(MEMORY_TRANSFER_MAX_WIRE_SIZE == 512);
    assert(MEMORY_TRANSFER_MAX_READ_SIZE == 510);
    assert(MEMORY_TRANSFER_MAX_WRITE_SIZE == 509);
    assert(MEMORY_TRANSFER_MAX_REQUEST_SIZE == MEMORY_TRANSFER_MAX_WIRE_SIZE);
    assert(memory_transfer_encode_read(0x20, 0x80, 16, request) == sizeof(request));
    static const uint8_t expected[] = {2, 0x41, 0x80, 0x10, 0x00};
    assert(memcmp(request, expected, sizeof(expected)) == 0);
    assert(memory_transfer_encode_read(0x20, 0x80, MEMORY_TRANSFER_MAX_READ_SIZE + 1, request) ==
           0);
    assert(memory_transfer_encode_read(0x20, 0x80, MEMORY_TRANSFER_MAX_WIRE_SIZE, request) == 0);
}

static void test_encodes_write_request(void) {
    static const uint8_t data[] = {0xaa, 0xbb};
    uint8_t request[MEMORY_TRANSFER_MAX_REQUEST_SIZE];

    assert(memory_transfer_encode_write(0x12, 0xb0, data, sizeof(data), request) == 5);
    static const uint8_t expected[] = {2, 0x24, 0xb0, 0xaa, 0xbb};
    assert(memcmp(request, expected, sizeof(expected)) == 0);
    assert(memory_transfer_encode_write(0x12, 0xb0, 0, MEMORY_TRANSFER_MAX_WRITE_SIZE, request) ==
           0);
}

static void test_encodes_maximum_write_request(void) {
    uint8_t data[MEMORY_TRANSFER_MAX_WRITE_SIZE];
    uint8_t request[MEMORY_TRANSFER_MAX_REQUEST_SIZE];
    for (uint16_t index = 0; index < sizeof(data); index++) {
        data[index] = (uint8_t)index;
    }

    assert(memory_transfer_encode_write(0x20, 0x80, data, sizeof(data), request) ==
           sizeof(request));
    assert(request[0] == 2);
    assert(request[1] == 0x40);
    assert(request[2] == 0x80);
    assert(memcmp(request + MEMORY_TRANSFER_HEADER_SIZE, data, sizeof(data)) == 0);
    assert(memory_transfer_encode_write(0x20, 0x80, data, sizeof(data) + 1, request) == 0);
    assert(memory_transfer_encode_write(0x20, 0x80, data, MEMORY_TRANSFER_MAX_WIRE_SIZE, request) ==
           0);
}

static void test_encodes_maximum_read_request(void) {
    uint8_t request[MEMORY_TRANSFER_READ_REQUEST_SIZE];

    assert(memory_transfer_encode_read(0x20, 0x80, MEMORY_TRANSFER_MAX_READ_SIZE, request) ==
           sizeof(request));
    assert(request[3] == 0xfe);
    assert(request[4] == 1);
}

static void test_decodes_responses(void) {
    static const uint8_t accepted_read[] = {1, 0, 0x12, 0x34, 0x56};
    static const uint8_t accepted_write[] = {1};
    static const uint8_t rejected[] = {0};
    static const uint8_t invalid[] = {2};
    uint8_t output[3];

    assert(memory_transfer_decode_read(accepted_read, sizeof(accepted_read), output,
                                       sizeof(output)) == MEMORY_TRANSFER_ACCEPTED);
    assert(memcmp(output, &accepted_read[2], sizeof(output)) == 0);
    assert(memory_transfer_decode_read(accepted_read, sizeof(accepted_read) - 1, output,
                                       sizeof(output)) == MEMORY_TRANSFER_INVALID_RESPONSE);
    assert(memory_transfer_decode_read(rejected, sizeof(rejected), output, sizeof(output)) ==
           MEMORY_TRANSFER_REJECTED);
    assert(memory_transfer_decode_write(accepted_write, sizeof(accepted_write)) ==
           MEMORY_TRANSFER_ACCEPTED);
    assert(memory_transfer_decode_write(invalid, sizeof(invalid)) ==
           MEMORY_TRANSFER_INVALID_RESPONSE);
}

int main(void) {
    test_encodes_read_request();
    test_encodes_write_request();
    test_encodes_maximum_write_request();
    test_encodes_maximum_read_request();
    test_decodes_responses();
    return 0;
}
