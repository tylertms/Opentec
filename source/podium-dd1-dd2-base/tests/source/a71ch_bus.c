#include <assert.h>
#include <stdbool.h>
#include <stdint.h>

#include "platform/aux_bus.h"
#include "secure_element/a71ch.h"
#include "secure_element/bus.h"

typedef enum {
    BUS_CALL_NONE,
    BUS_CALL_WRITE,
    BUS_CALL_READ,
} BusCall;

static BusCall bus_call;
static uint8_t bus_address;
static uint16_t bus_register;
static const uint8_t *bus_write_data;
static uint8_t *bus_read_data;
static uint16_t bus_length;
static bool bus_accepts;

bool platform_aux_bus_start_write(uint8_t address, uint16_t register_address, const uint8_t *data,
                                  uint16_t length) {
    bus_call = BUS_CALL_WRITE;
    bus_address = address;
    bus_register = register_address;
    bus_write_data = data;
    bus_length = length;
    return bus_accepts;
}

bool platform_aux_bus_start_read(uint8_t address, uint16_t register_address, uint8_t *data,
                                 uint16_t length) {
    bus_call = BUS_CALL_READ;
    bus_address = address;
    bus_register = register_address;
    bus_read_data = data;
    bus_length = length;
    return bus_accepts;
}

static void reset_bus(void) {
    bus_call = BUS_CALL_NONE;
    bus_address = 0;
    bus_register = 0;
    bus_write_data = 0;
    bus_read_data = 0;
    bus_length = 0;
    bus_accepts = true;
}

static void test_starts_register_only_session_write(void) {
    reset_bus();
    assert(a71ch_bus_start(A71CH_WAKE_UP, 0));
    assert(bus_call == BUS_CALL_WRITE);
    assert(bus_address == 0x48);
    assert(bus_register == 0x0f);
    assert(bus_write_data == 0);
    assert(bus_length == 0);
}

static void assert_read(A71chCommand command, uint16_t selector, uint16_t length) {
    uint8_t response[0x1f];
    reset_bus();
    assert(a71ch_bus_start(command, response));
    assert(bus_call == BUS_CALL_READ);
    assert(bus_address == 0x48);
    assert(bus_register == selector);
    assert(bus_read_data == response);
    assert(bus_length == length);
}

static void test_starts_exact_response_reads(void) {
    assert_read(A71CH_SOFT_RESET, 0x1f, 2);
    assert_read(A71CH_READ_ANSWER_TO_RESET, 0x2f, 0x1f);
    assert_read(A71CH_PARAMETER_EXCHANGE, 0xff, 2);
    assert_read(A71CH_READ_STATUS, 0x07, 2);
}

static void test_rejects_invalid_requests(void) {
    reset_bus();
    assert(!a71ch_bus_start(A71CH_AUTHENTICATION_WRITE, 0));
    assert(bus_call == BUS_CALL_NONE);
    assert(!a71ch_bus_start(A71CH_READ_ANSWER_TO_RESET, 0));
    assert(bus_call == BUS_CALL_NONE);
}

static void test_propagates_bus_backpressure(void) {
    uint8_t response[2];
    reset_bus();
    bus_accepts = false;
    assert(!a71ch_bus_start(A71CH_SOFT_RESET, response));
    assert(bus_call == BUS_CALL_READ);
}

static void test_starts_encoded_transfer_write(void) {
    const uint8_t payload[] = {0x11, 0x22, 0x33};
    A71chAuthenticationInput input = {
        .phase = 3,
        .chunk_index = 2,
        .chunk = payload,
        .chunk_length = sizeof(payload),
    };
    A71chAuthenticationFrame frame;
    assert(a71ch_authentication_encode(A71CH_AUTHENTICATION_WRITE_LRC, &input, &frame));

    reset_bus();
    assert(a71ch_bus_start_frame_write(&frame));
    assert(bus_call == BUS_CALL_WRITE);
    assert(bus_address == 0x48);
    assert(bus_register == 0x34);
    assert(bus_write_data == frame.write_data);
    assert(bus_length == frame.write_length);
}

static void test_starts_encoded_transfer_response_read(void) {
    A71chAuthenticationInput input = {
        .phase = 7,
        .chunk_index = 16,
        .chunk_length = 16,
    };
    A71chAuthenticationFrame frame;
    uint8_t response[20];
    assert(a71ch_authentication_encode(A71CH_AUTHENTICATION_READ, &input, &frame));

    reset_bus();
    assert(a71ch_bus_start_frame_read(&frame, response));
    assert(bus_call == BUS_CALL_READ);
    assert(bus_address == 0x48);
    assert(bus_register == 0x82);
    assert(bus_read_data == response);
    assert(bus_length == sizeof(response));
}

static void test_rejects_invalid_transfer_frames(void) {
    A71chAuthenticationFrame frame = {0};
    uint8_t response[1];

    reset_bus();
    assert(!a71ch_bus_start_frame_write(0));
    assert(!a71ch_bus_start_frame_write(&frame));
    assert(!a71ch_bus_start_frame_read(0, response));
    assert(!a71ch_bus_start_frame_read(&frame, 0));
    assert(!a71ch_bus_start_frame_read(&frame, response));
    assert(bus_call == BUS_CALL_NONE);
}

int main(void) {
    test_starts_register_only_session_write();
    test_starts_exact_response_reads();
    test_rejects_invalid_requests();
    test_propagates_bus_backpressure();
    test_starts_encoded_transfer_write();
    test_starts_encoded_transfer_response_read();
    test_rejects_invalid_transfer_frames();
    return 0;
}
