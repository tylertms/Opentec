#ifndef OPENTEC_BASE_WHEEL_COMMAND_FORWARDER_H
#define OPENTEC_BASE_WHEEL_COMMAND_FORWARDER_H

#include <stdbool.h>
#include <stdint.h>

#include "transfer/command.h"

enum {
    WHEEL_COMMAND_FORWARDER_PAYLOAD_SIZE = 61,
};

typedef enum {
    WHEEL_COMMAND_FORWARDER_PROBE_READY,
    WHEEL_COMMAND_FORWARDER_PROBE_PENDING,
    WHEEL_COMMAND_FORWARDER_READY,
    WHEEL_COMMAND_FORWARDER_WRITE_PENDING,
} WheelCommandForwarderPhase;

/** @brief Generic attached-device command forwarding state. */
typedef struct {
    uint8_t payload[WHEEL_COMMAND_FORWARDER_PAYLOAD_SIZE];
    uint8_t probe[4];
    uint8_t payload_length;
    uint8_t endpoint_index;
    WheelCommandForwarderPhase phase;
} WheelCommandForwarder;

void wheel_command_forwarder_init(WheelCommandForwarder *forwarder);
bool wheel_command_forwarder_accepting(const WheelCommandForwarder *forwarder);
bool wheel_command_forwarder_queue(WheelCommandForwarder *forwarder, const uint8_t *payload,
                                   uint8_t length);
void wheel_command_forwarder_run(WheelCommandForwarder *forwarder, CommandTransport *transport);

#endif
