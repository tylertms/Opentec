#include "motor/output_transport.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

/**
 * @brief Advances a force-command queue index.
 *
 * Wraps the index to zero after the final record in the 100-entry queue.
 *
 * @param[in] index Current queue index.
 * @return Index of the following queue record.
 */
static uint8_t next_index(uint8_t index) {
    index++;
    return index == MOTOR_OUTPUT_QUEUE_CAPACITY ? 0 : index;
}

/**
 * @brief Initializes the force-feedback command transport.
 *
 * Clears the 100-record command queue and the status value used to decide whether the next motor
 * exchange needs a status packet.
 *
 * @param[out] transport Transport state to initialize.
 */
void motor_output_transport_init(MotorOutputTransport *transport) {
    if (transport != NULL) {
        memset(transport, 0, sizeof(*transport));
    }
}

/**
 * @brief Queues one complete force-feedback command for the motor controller.
 *
 * Copies all seven command bytes into the next free ring-buffer record. The newest command is
 * discarded when all 100 records are occupied.
 *
 * @param[in,out] transport Command transport to update.
 * @param[in] command Seven-byte force-feedback command.
 * @return True when the command was queued; otherwise false.
 */
bool motor_output_transport_enqueue_command(MotorOutputTransport *transport,
                                            const uint8_t command[MOTOR_OUTPUT_COMMAND_SIZE]) {
    if (transport == NULL || command == NULL || transport->count == MOTOR_OUTPUT_QUEUE_CAPACITY) {
        return false;
    }

    memcpy(transport->commands[transport->write_index], command, MOTOR_OUTPUT_COMMAND_SIZE);
    transport->write_index = next_index(transport->write_index);
    transport->count++;
    return true;
}

/**
 * @brief Queues a force-feedback command containing only an opcode.
 *
 * Stores the opcode in the first byte and clears the remaining six bytes so the motor controller
 * receives the same fixed-width command record used for complete commands.
 *
 * @param[in,out] transport Command transport to update.
 * @param[in] opcode Force-feedback command opcode.
 * @return True when the opcode was queued; otherwise false.
 */
bool motor_output_transport_enqueue_opcode(MotorOutputTransport *transport, uint8_t opcode) {
    if (transport == NULL || transport->count == MOTOR_OUTPUT_QUEUE_CAPACITY) {
        return false;
    }

    uint8_t *destination = transport->commands[transport->write_index];
    memset(destination, 0, MOTOR_OUTPUT_COMMAND_SIZE);
    destination[0] = opcode;
    transport->write_index = next_index(transport->write_index);
    transport->count++;
    return true;
}

/**
 * @brief Builds the next payload sent to the motor controller.
 *
 * Sends the oldest queued command first, then a status-only packet when the status byte changes,
 * and otherwise sends the current center position and live force output. Command and status
 * packets use motor-link type 2; live force packets use type 1.
 *
 * @param[in,out] transport Command queue and previous status value.
 * @param[in] status Current force-feedback status bits.
 * @param[in] center_position Stored wheel-center position.
 * @param[in] report Current live force direction and magnitudes.
 * @param[out] frame Motor-link payload selected for the next exchange.
 */
void motor_output_transport_build_frame(MotorOutputTransport *transport, uint8_t status,
                                        int16_t center_position, const ForceOutputReport *report,
                                        MotorLiveFrame *frame) {
    if (transport->count != 0) {
        frame->type = MOTOR_LIVE_STATUS_TYPE;
        frame->payload[0] = status;
        memcpy(frame->payload + 1, transport->commands[transport->read_index],
               MOTOR_OUTPUT_COMMAND_SIZE);
        memset(transport->commands[transport->read_index], 0, MOTOR_OUTPUT_COMMAND_SIZE);
        transport->read_index = next_index(transport->read_index);
        transport->count--;
        transport->previous_status = status;
        return;
    }

    if (status != transport->previous_status) {
        frame->type = MOTOR_LIVE_STATUS_TYPE;
        frame->payload[0] = status;
        memset(frame->payload + 1, 0, MOTOR_OUTPUT_COMMAND_SIZE);
        transport->previous_status = status;
        return;
    }

    motor_live_force_frame_init(center_position, report, frame);
}
