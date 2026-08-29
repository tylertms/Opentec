#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "wheel/updater_bridge.h"

static WheelUpdaterIo io(WheelUpdaterIoStatus status, uint32_t now_ms, const uint8_t *data,
                         uint8_t length) {
    return (WheelUpdaterIo){.data = data, .now_ms = now_ms, .length = length, .status = status};
}

static WheelUpdaterOperation step(WheelUpdaterBridge *bridge, WheelUpdaterIoStatus status,
                                  uint32_t now_ms, const uint8_t *data, uint8_t length) {
    WheelUpdaterIo result = io(status, now_ms, data, length);
    return wheel_updater_bridge_step(bridge, &result);
}

static void assert_operation(WheelUpdaterOperation operation, WheelUpdaterOperationKind kind,
                             uint8_t length) {
    assert(operation.kind == kind);
    assert(operation.length == length);
}

static void begin_response(WheelUpdaterBridge *bridge, const uint8_t *request, uint8_t length,
                           uint32_t now_ms) {
    assert(wheel_updater_bridge_start(bridge, request, length));
    WheelUpdaterOperation operation = step(bridge, WHEEL_UPDATER_IO_IDLE, now_ms, NULL, 0);
    assert_operation(operation, WHEEL_UPDATER_OPERATION_WRITE, length);
    assert(memcmp(operation.data, request, length) == 0);
    operation = step(bridge, WHEEL_UPDATER_IO_COMPLETE, now_ms, NULL, 0);
    assert_operation(operation, WHEEL_UPDATER_OPERATION_NONE, 0);
    operation = step(bridge, WHEEL_UPDATER_IO_IDLE, now_ms + 1, NULL, 0);
    assert_operation(operation, WHEEL_UPDATER_OPERATION_NONE, 0);
    operation = step(bridge, WHEEL_UPDATER_IO_IDLE, now_ms + 2, NULL, 0);
    assert_operation(operation, WHEEL_UPDATER_OPERATION_READ, 1);
}

static WheelUpdaterOperation supply_header(WheelUpdaterBridge *bridge, uint32_t now_ms,
                                           uint8_t opcode) {
    const uint8_t marker[] = {0x5a};
    const uint8_t code[] = {opcode};
    WheelUpdaterOperation operation =
        step(bridge, WHEEL_UPDATER_IO_COMPLETE, now_ms, marker, sizeof(marker));
    assert_operation(operation, WHEEL_UPDATER_OPERATION_READ, 1);
    return step(bridge, WHEEL_UPDATER_IO_COMPLETE, now_ms, code, sizeof(code));
}

static void assert_response(WheelUpdaterBridge *bridge, const uint8_t *expected, uint8_t length) {
    const uint8_t *response = NULL;
    uint8_t response_length = 0;
    assert(wheel_updater_bridge_take_response(bridge, &response, &response_length));
    assert(response_length == length);
    assert(memcmp(response, expected, length) == 0);
    assert(!wheel_updater_bridge_active(bridge));
    assert(!wheel_updater_bridge_take_response(bridge, &response, &response_length));
}

static void test_rejects_invalid_requests_and_busy_start(void) {
    WheelUpdaterBridge bridge;
    uint8_t request[WHEEL_UPDATER_BRIDGE_MAX_REQUEST_SIZE + 1] = {0x5a, 0xa2};
    wheel_updater_bridge_init(&bridge);

    assert(!wheel_updater_bridge_start(NULL, request, 2));
    assert(!wheel_updater_bridge_start(&bridge, NULL, 2));
    assert(!wheel_updater_bridge_start(&bridge, request, 1));
    assert(!wheel_updater_bridge_start(&bridge, request, sizeof(request)));
    request[0] = 0;
    assert(!wheel_updater_bridge_start(&bridge, request, 2));
    request[0] = 0x5a;
    assert(wheel_updater_bridge_start(&bridge, request, 2));
    assert(!wheel_updater_bridge_start(&bridge, request, 2));
    assert(wheel_updater_bridge_active(&bridge));
    assert(!wheel_updater_bridge_active(NULL));
    assert(wheel_updater_bridge_step(NULL, NULL).kind == WHEEL_UPDATER_OPERATION_NONE);
    assert(wheel_updater_bridge_step(&bridge, NULL).kind == WHEEL_UPDATER_OPERATION_NONE);
}

static void test_accepts_maximum_request(void) {
    WheelUpdaterBridge bridge;
    uint8_t request[WHEEL_UPDATER_BRIDGE_MAX_REQUEST_SIZE] = {0x5a, 0xb0};
    for (uint8_t index = 2; index < sizeof(request); index++) {
        request[index] = index;
    }
    wheel_updater_bridge_init(&bridge);

    assert(wheel_updater_bridge_start(&bridge, request, sizeof(request)));
    WheelUpdaterOperation operation = step(&bridge, WHEEL_UPDATER_IO_IDLE, 0, NULL, 0);
    assert_operation(operation, WHEEL_UPDATER_OPERATION_WRITE, sizeof(request));
    assert(memcmp(operation.data, request, sizeof(request)) == 0);
}

static void test_finishes_retry_request_after_write(void) {
    WheelUpdaterBridge bridge;
    const uint8_t request[] = {0x5a, 0xa1, 0x33};
    wheel_updater_bridge_init(&bridge);

    assert(wheel_updater_bridge_start(&bridge, request, sizeof(request)));
    assert_operation(step(&bridge, WHEEL_UPDATER_IO_FAILED, 10, NULL, 0),
                     WHEEL_UPDATER_OPERATION_WRITE, sizeof(request));
    assert_operation(step(&bridge, WHEEL_UPDATER_IO_PENDING, 10, NULL, 0),
                     WHEEL_UPDATER_OPERATION_NONE, 0);
    assert_operation(step(&bridge, WHEEL_UPDATER_IO_COMPLETE, 10, NULL, 0),
                     WHEEL_UPDATER_OPERATION_NONE, 0);
    assert(!wheel_updater_bridge_active(&bridge));
}

static void test_retries_preamble_and_assembles_acknowledgement(void) {
    WheelUpdaterBridge bridge;
    const uint8_t request[] = {0x5a, 0xb0};
    const uint8_t zero[] = {0};
    const uint8_t invalid[] = {0x44};
    const uint8_t expected[] = {0x5a, 0xa2};
    wheel_updater_bridge_init(&bridge);
    begin_response(&bridge, request, sizeof(request), 20);

    assert_operation(step(&bridge, WHEEL_UPDATER_IO_COMPLETE, 22, zero, sizeof(zero)),
                     WHEEL_UPDATER_OPERATION_READ, 1);
    assert_operation(step(&bridge, WHEEL_UPDATER_IO_FAILED, 22, NULL, 0),
                     WHEEL_UPDATER_OPERATION_READ, 1);
    assert_operation(step(&bridge, WHEEL_UPDATER_IO_COMPLETE, 22, invalid, sizeof(invalid)),
                     WHEEL_UPDATER_OPERATION_WRITE, sizeof(request));
    step(&bridge, WHEEL_UPDATER_IO_COMPLETE, 30, NULL, 0);
    step(&bridge, WHEEL_UPDATER_IO_IDLE, 32, NULL, 0);
    supply_header(&bridge, 32, 0xa2);
    assert_response(&bridge, expected, sizeof(expected));
}

static void test_assembles_fixed_response(void) {
    WheelUpdaterBridge bridge;
    const uint8_t request[] = {0x5a, 0xb0, 1};
    const uint8_t payload[] = {0, 1, 2, 3, 4, 5, 6, 7};
    const uint8_t expected[] = {0x5a, 0xa7, 0, 1, 2, 3, 4, 5, 6, 7};
    wheel_updater_bridge_init(&bridge);
    begin_response(&bridge, request, sizeof(request), 0);

    WheelUpdaterOperation operation = supply_header(&bridge, 2, 0xa7);
    assert_operation(operation, WHEEL_UPDATER_OPERATION_READ, sizeof(payload));
    operation = step(&bridge, WHEEL_UPDATER_IO_COMPLETE, 2, payload, sizeof(payload));
    assert_operation(operation, WHEEL_UPDATER_OPERATION_NONE, 0);
    assert_response(&bridge, expected, sizeof(expected));
}

static void test_assembles_variable_response_and_caps_payload(void) {
    WheelUpdaterBridge bridge;
    const uint8_t request[] = {0x5a, 0xb0};
    const uint8_t length[] = {0xff, 0xff};
    const uint8_t metadata[] = {0x34, 0x12};
    uint8_t payload[60];
    uint8_t expected[66] = {0x5a, 0xa4, 0xff, 0xff, 0x34, 0x12};
    for (uint8_t index = 0; index < sizeof(payload); index++) {
        payload[index] = (uint8_t)(index + 1);
        expected[index + 6] = payload[index];
    }
    wheel_updater_bridge_init(&bridge);
    begin_response(&bridge, request, sizeof(request), 100);

    supply_header(&bridge, 102, 0xa4);
    WheelUpdaterOperation operation =
        step(&bridge, WHEEL_UPDATER_IO_COMPLETE, 102, length, sizeof(length));
    assert_operation(operation, WHEEL_UPDATER_OPERATION_READ, 2);
    operation = step(&bridge, WHEEL_UPDATER_IO_COMPLETE, 102, metadata, sizeof(metadata));
    assert_operation(operation, WHEEL_UPDATER_OPERATION_READ, sizeof(payload));
    step(&bridge, WHEEL_UPDATER_IO_COMPLETE, 102, payload, sizeof(payload));
    assert_response(&bridge, expected, sizeof(expected));
}

static void test_assembles_empty_variable_response(void) {
    WheelUpdaterBridge bridge;
    const uint8_t request[] = {0x5a, 0xb0};
    const uint8_t length[] = {0, 0};
    const uint8_t metadata[] = {0x34, 0x12};
    const uint8_t expected[] = {0x5a, 0xa4, 0, 0, 0x34, 0x12};
    wheel_updater_bridge_init(&bridge);
    begin_response(&bridge, request, sizeof(request), 0);

    supply_header(&bridge, 2, 0xa4);
    assert_operation(step(&bridge, WHEEL_UPDATER_IO_COMPLETE, 2, length, sizeof(length)),
                     WHEEL_UPDATER_OPERATION_READ, sizeof(metadata));
    assert_operation(step(&bridge, WHEEL_UPDATER_IO_COMPLETE, 2, metadata, sizeof(metadata)),
                     WHEEL_UPDATER_OPERATION_NONE, 0);
    assert_response(&bridge, expected, sizeof(expected));
}

static void test_ignores_unrecognized_response_opcodes(void) {
    const uint8_t request[] = {0x5a, 0xb0};
    for (uint16_t opcode = 0; opcode <= UINT8_MAX; opcode++) {
        if (opcode == 0xa1 || opcode == 0xa2 || opcode == 0xa4 || opcode == 0xa7) {
            continue;
        }
        WheelUpdaterBridge bridge;
        wheel_updater_bridge_init(&bridge);
        begin_response(&bridge, request, sizeof(request), 0);
        WheelUpdaterOperation operation = supply_header(&bridge, 2, (uint8_t)opcode);
        assert_operation(operation, WHEEL_UPDATER_OPERATION_NONE, 0);
        assert(!wheel_updater_bridge_active(&bridge));
    }
}

static void test_assembles_retry_sequence_until_zero(void) {
    WheelUpdaterBridge bridge;
    const uint8_t request[] = {0x5a, 0xb0};
    const uint8_t marker[] = {0x5a};
    const uint8_t retry[] = {0xa1};
    const uint8_t zero[] = {0};
    const uint8_t expected[] = {0x5a, 0xa1, 0x5a, 0xa1};
    wheel_updater_bridge_init(&bridge);
    begin_response(&bridge, request, sizeof(request), UINT32_MAX - 1000);

    supply_header(&bridge, UINT32_MAX - 998, 0xa1);
    assert_operation(step(&bridge, WHEEL_UPDATER_IO_COMPLETE, UINT32_MAX - 900, marker, 1),
                     WHEEL_UPDATER_OPERATION_READ, 1);
    assert_operation(step(&bridge, WHEEL_UPDATER_IO_COMPLETE, UINT32_MAX - 900, retry, 1),
                     WHEEL_UPDATER_OPERATION_READ, 1);
    step(&bridge, WHEEL_UPDATER_IO_COMPLETE, UINT32_MAX - 800, zero, 1);
    assert_response(&bridge, expected, sizeof(expected));
}

static void test_finishes_retry_sequence_after_timeout(void) {
    WheelUpdaterBridge bridge;
    const uint8_t request[] = {0x5a, 0xb0};
    const uint8_t expected[] = {0x5a, 0xa1};
    wheel_updater_bridge_init(&bridge);
    begin_response(&bridge, request, sizeof(request), UINT32_MAX - 1000);
    supply_header(&bridge, UINT32_MAX - 998, 0xa1);

    assert_operation(step(&bridge, WHEEL_UPDATER_IO_PENDING, 1000, NULL, 0),
                     WHEEL_UPDATER_OPERATION_NONE, 0);
    assert_operation(step(&bridge, WHEEL_UPDATER_IO_IDLE, 1001, NULL, 0),
                     WHEEL_UPDATER_OPERATION_NONE, 0);
    assert_response(&bridge, expected, sizeof(expected));
}

int main(void) {
    test_rejects_invalid_requests_and_busy_start();
    test_accepts_maximum_request();
    test_finishes_retry_request_after_write();
    test_retries_preamble_and_assembles_acknowledgement();
    test_assembles_fixed_response();
    test_assembles_variable_response_and_caps_payload();
    test_assembles_empty_variable_response();
    test_ignores_unrecognized_response_opcodes();
    test_assembles_retry_sequence_until_zero();
    test_finishes_retry_sequence_after_timeout();
    return 0;
}
