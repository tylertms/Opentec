#ifndef OPENTEC_BASE_MOTOR_OUTPUT_TRANSPORT_H
#define OPENTEC_BASE_MOTOR_OUTPUT_TRANSPORT_H

#include <stdbool.h>
#include <stdint.h>

#include "force_feedback/output_report.h"
#include "motor/live_frame.h"

enum {
    MOTOR_OUTPUT_COMMAND_SIZE = 7,
    MOTOR_OUTPUT_QUEUE_CAPACITY = 100,
    MOTOR_OUTPUT_STATUS_REMOTE_EFFECTS = 1 << 0,
    MOTOR_OUTPUT_STATUS_ENABLED = 1 << 1,
    MOTOR_OUTPUT_STATUS_PRIMARY_DISABLED = 1 << 4,
    MOTOR_OUTPUT_STATUS_SECONDARY_DISABLED = 1 << 5,
    MOTOR_OUTPUT_STATUS_USB_DISCONNECTED = 1 << 6,
};

typedef struct {
    uint8_t commands[MOTOR_OUTPUT_QUEUE_CAPACITY][MOTOR_OUTPUT_COMMAND_SIZE];
    uint8_t read_index;
    uint8_t write_index;
    uint8_t count;
    uint8_t previous_status;
} MotorOutputTransport;

void motor_output_transport_init(MotorOutputTransport *transport);
bool motor_output_transport_enqueue_command(MotorOutputTransport *transport,
                                            const uint8_t command[MOTOR_OUTPUT_COMMAND_SIZE]);
bool motor_output_transport_enqueue_opcode(MotorOutputTransport *transport, uint8_t opcode);
void motor_output_transport_build_frame(MotorOutputTransport *transport, uint8_t status,
                                        int16_t center_position, const ForceOutputReport *report,
                                        MotorLiveFrame *frame);

#endif
