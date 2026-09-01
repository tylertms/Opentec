#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "motor/command_channel.h"
#include "motor/command_mailbox.h"
#include "motor/command_packet.h"
#include "motor/command_startup_service.h"
#include "transfer/command.h"

enum {
    TEST_BUFFER_SIZE = 128,
    TEST_MAILBOX_PAYLOAD_OFFSET = 0x80,
    TEST_MAILBOX_LENGTH_OFFSET = 0x81,
    TEST_MAILBOX_CONTROL_OFFSET = 0x82,
};

typedef struct {
    MotorCommandStartupService service;
    MotorCommandChannel channel;
    MotorCommandMailboxExchange exchange;
    CommandTransport transport;
    uint8_t receive_assembly[TEST_BUFFER_SIZE];
    uint8_t mailbox_receive[TEST_BUFFER_SIZE];
    uint8_t transmit[TEST_BUFFER_SIZE];
    uint8_t pending_payload[TEST_BUFFER_SIZE];
    uint8_t response[TEST_BUFFER_SIZE];
    uint16_t response_length;
    uint8_t response_sequence;
    bool response_pending;
} Fixture;

static void fixture_init(Fixture *fixture) {
    memset(fixture, 0, sizeof(*fixture));
    MotorCommandChannelBuffers buffers = {
        .receive_assembly = fixture->receive_assembly,
        .receive_assembly_capacity = sizeof(fixture->receive_assembly),
        .transmit = fixture->transmit,
        .transmit_capacity = sizeof(fixture->transmit),
        .pending_payload = fixture->pending_payload,
        .pending_payload_capacity = sizeof(fixture->pending_payload),
    };
    assert(motor_command_channel_init(&fixture->channel, &buffers));
    assert(motor_command_mailbox_exchange_init(&fixture->exchange, fixture->mailbox_receive,
                                               sizeof(fixture->mailbox_receive)));
    command_transport_init(&fixture->transport);
    motor_command_startup_service_init(&fixture->service);
}

static void prepare_response(Fixture *fixture) {
    uint8_t payload[21] = {0};
    uint16_t payload_length;
    uint8_t selector = 0;
    if (fixture->service.startup.phase == MOTOR_COMMAND_STARTUP_WAIT_DIGEST) {
        payload[0] = 0x87;
        payload[4] = 16;
        for (uint8_t index = 0; index < 16; index++) {
            payload[5 + index] = index;
        }
        payload_length = sizeof(payload);
    } else {
        selector = fixture->service.startup.phase == MOTOR_COMMAND_STARTUP_WAIT_FIRST_INFO ? 3 : 4;
        payload[0] = 0x85;
        payload[2] = selector;
        payload[4] = 2;
        payload[5] = selector;
        payload[6] = (uint8_t)(selector + 1);
        payload_length = 7;
    }
    uint8_t adjacent = fixture->channel.receiver.sequence.transmit == 0
                           ? 3
                           : fixture->channel.receiver.sequence.transmit - 1;
    assert(motor_command_packet_payload_encode(
        0, fixture->response_sequence++, adjacent, payload, payload_length, fixture->response,
        sizeof(fixture->response), &fixture->response_length));
    fixture->response_pending = true;
}

static void complete_transport(Fixture *fixture) {
    const uint8_t *request;
    uint16_t request_length;
    if (!command_transport_request(&fixture->transport, &request, &request_length)) {
        return;
    }
    uint8_t offset = request[2];
    bool read = (request[1] & 1) != 0;
    assert(command_transport_request_sent(&fixture->transport));
    if (!read) {
        const uint8_t accepted[] = {1};
        command_transport_receive(&fixture->transport, accepted, sizeof(accepted));
        return;
    }

    uint8_t response[TEST_BUFFER_SIZE + 2] = {1, 0};
    if (offset == TEST_MAILBOX_LENGTH_OFFSET) {
        response[2] = 0;
        response[3] = TEST_BUFFER_SIZE;
        command_transport_receive(&fixture->transport, response, 4);
        return;
    }
    if (offset == TEST_MAILBOX_CONTROL_OFFSET) {
        if (fixture->exchange.write_packet == 0 &&
            (fixture->service.startup.phase == MOTOR_COMMAND_STARTUP_WAIT_DIGEST ||
             fixture->service.startup.phase == MOTOR_COMMAND_STARTUP_WAIT_FIRST_INFO ||
             fixture->service.startup.phase == MOTOR_COMMAND_STARTUP_WAIT_SECOND_INFO) &&
            !fixture->service.response_ready && !fixture->response_pending) {
            prepare_response(fixture);
        }
        if (fixture->response_pending) {
            response[2] = MOTOR_COMMAND_MAILBOX_CONTROL_PAYLOAD_AVAILABLE;
            response[4] = (uint8_t)(fixture->response_length >> 8);
            response[5] = (uint8_t)fixture->response_length;
        }
        command_transport_receive(&fixture->transport, response, 6);
        return;
    }
    assert(offset == TEST_MAILBOX_PAYLOAD_OFFSET);
    assert(fixture->response_pending);
    memcpy(response + 2, fixture->response, fixture->response_length);
    command_transport_receive(&fixture->transport, response, fixture->response_length + 2);
    fixture->response_pending = false;
}

static void test_completes_startup_and_collects_identity(void) {
    Fixture fixture;
    fixture_init(&fixture);

    MotorCommandStartupServiceResult result = MOTOR_COMMAND_STARTUP_SERVICE_RUNNING;
    for (uint16_t iteration = 0; iteration < 500 && result == MOTOR_COMMAND_STARTUP_SERVICE_RUNNING;
         iteration++) {
        result = motor_command_startup_service_run(&fixture.service, &fixture.channel,
                                                   &fixture.exchange, &fixture.transport);
        complete_transport(&fixture);
    }

    assert(result == MOTOR_COMMAND_STARTUP_SERVICE_COMPLETE);
    assert(!command_transport_is_owner(&fixture.transport, MOTOR_COMMAND_STARTUP_OWNER));
    const MotorCommandApplication *application =
        motor_command_channel_application(&fixture.channel);
    static const uint8_t expected_digest[] = {0, 1, 2, 3, 14, 14, 0, 0};
    assert(memcmp(application->digest, expected_digest, sizeof(expected_digest)) == 0);
    assert(application->information.selector_3 == 0x0304);
    assert(application->information.selector_4 == 0x0405);
}

int main(void) {
    test_completes_startup_and_collects_identity();
    return 0;
}
