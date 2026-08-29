#include "i2c/probe.h"

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

enum {
    I2C_PROBE_RESPONSE_ACCEPT = 0x07,
    I2C_PROBE_RESPONSE_WAIT = 0x17,
    I2C_PROBE_UNEXPECTED_RETRY_LIMIT = 2,
    I2C_PROBE_STARTUP_ATTEMPT_LIMIT = 3,
    I2C_PROBE_STARTUP_RETRY_DELAY_MS = 5,
    I2C_PROBE_STARTUP_SIGNATURE_SIZE = 29,
};

static const uint8_t startup_signature[I2C_PROBE_STARTUP_SIGNATURE_SIZE] = {
    0xb8, 0x04, 0x11, 0x01, 0x05, 0x04, 0xb9, 0x02, 0x01, 0x01, 0xba, 0x01, 0x01, 0xbb, 0x0c,
    0x41, 0x37, 0x31, 0x30, 0x35, 0x43, 0x43, 0x32, 0x34, 0x32, 0x52, 0x31, 0xbc, 0x00,
};

static const I2cProbeRequest requests[] = {
    [I2C_PROBE_BEGIN_SESSION] = {.selector = 0x0f},
    [I2C_PROBE_READ_STARTUP_STATUS] = {.selector = 0x1f, .response_length = 2},
    [I2C_PROBE_READ_SIGNATURE] = {.selector = 0x2f, .response_length = 0x1f},
    [I2C_PROBE_READ_CONFIRMATION] = {.selector = 0xff, .response_length = 2},
    [I2C_PROBE_READ_READY_STATUS] = {.selector = 0x07, .response_length = 2},
};

/**
 * @brief Initializes probe handshake response tracking.
 *
 * Clears the retry count used to limit repeated unexpected handshake responses.
 *
 * @param[out] handshake Handshake response state to initialize.
 */
void i2c_probe_handshake_init(I2cProbeHandshake *handshake) { handshake->retry_count = 0; }

/**
 * @brief Evaluates one probe handshake response.
 *
 * Response 0x07 accepts the handshake and clears the retry count. Response 0x17 reports that the
 * device is busy and increments the count without applying the unexpected-response limit. Other
 * responses receive another attempt while the prior count is at most two and are then rejected.
 *
 * @param[in,out] handshake Persistent handshake retry state.
 * @param[in] response Device response code.
 * @return Accepted, busy, retry, or rejected response disposition.
 */
I2cProbeResponseResult i2c_probe_handshake_evaluate(I2cProbeHandshake *handshake,
                                                    uint8_t response) {
    if (response == I2C_PROBE_RESPONSE_ACCEPT) {
        handshake->retry_count = 0;
        return I2C_PROBE_RESPONSE_ACCEPTED;
    }
    if (response == I2C_PROBE_RESPONSE_WAIT) {
        ++handshake->retry_count;
        return I2C_PROBE_RESPONSE_BUSY;
    }
    if (handshake->retry_count <= I2C_PROBE_UNEXPECTED_RETRY_LIMIT) {
        ++handshake->retry_count;
        return I2C_PROBE_RESPONSE_RETRY;
    }
    return I2C_PROBE_RESPONSE_REJECTED;
}

/**
 * @brief Evaluates one command-stage probe response.
 *
 * Response 0x07 accepts the command, response 0x17 keeps it pending, and every other response
 * rejects the exchange.
 *
 * @param[in] response Device response code.
 * @return Accepted, busy, or rejected response disposition.
 */
I2cProbeResponseResult i2c_probe_command_response_evaluate(uint8_t response) {
    if (response == I2C_PROBE_RESPONSE_ACCEPT) {
        return I2C_PROBE_RESPONSE_ACCEPTED;
    }
    if (response == I2C_PROBE_RESPONSE_WAIT) {
        return I2C_PROBE_RESPONSE_BUSY;
    }
    return I2C_PROBE_RESPONSE_REJECTED;
}

/**
 * @brief Calculates the probe response checksum.
 *
 * XORs every payload byte into an accumulator initialized to zero.
 *
 * @param[in] payload Response payload bytes.
 * @param[in] payload_length Number of payload bytes to include.
 * @return XOR of all payload bytes, or zero for an empty payload.
 */
uint8_t i2c_probe_checksum(const uint8_t *payload, uint8_t payload_length) {
    uint8_t checksum = 0;
    for (uint8_t index = 0; index < payload_length; ++index) {
        checksum ^= payload[index];
    }
    return checksum;
}

/**
 * @brief Validates the final response from a probe exchange.
 *
 * Either nonzero status byte reports a status error. When checksum validation is enabled, the
 * payload XOR must equal the expected checksum. Disabled checksum validation accepts a response
 * whose status bytes are both zero.
 *
 * @param[in] response Final payload, checksum, and status fields.
 * @param[in] checksum_enabled True to compare the payload XOR with the expected checksum.
 * @return Valid, checksum-error, or status-error result.
 */
I2cProbeValidationResult i2c_probe_final_response_validate(const I2cProbeFinalResponse *response,
                                                           bool checksum_enabled) {
    if (response->primary_status != 0 || response->secondary_status != 0) {
        return I2C_PROBE_STATUS_ERROR;
    }
    if (checksum_enabled && i2c_probe_checksum(response->payload, response->payload_length) !=
                                response->expected_checksum) {
        return I2C_PROBE_CHECKSUM_ERROR;
    }
    return I2C_PROBE_VALID;
}

/**
 * @brief Encodes a fixed probe request.
 *
 * Maps the five session and identification commands to their I2C selector and exact response
 * length. These requests carry no write payload.
 *
 * @param[in] command Session or identification command to encode.
 * @param[out] request Encoded selector and response length.
 * @return True for commands 1 through 5; otherwise false.
 */
bool i2c_probe_request_encode(I2cProbeCommand command, I2cProbeRequest *request) {
    const I2cProbeRequest *encoded = i2c_probe_request_lookup(command);
    if (encoded == NULL) {
        return false;
    }
    *request = *encoded;
    return true;
}

/**
 * @brief Looks up a fixed probe request.
 *
 * Maps the five session and identification commands to their immutable selector and exact response
 * length without copying the descriptor.
 *
 * @param[in] command Session or identification command to look up.
 * @return Fixed request descriptor for commands 1 through 5, or null for another command.
 */
const I2cProbeRequest *i2c_probe_request_lookup(I2cProbeCommand command) {
    if (command < I2C_PROBE_BEGIN_SESSION || command > I2C_PROBE_READ_READY_STATUS) {
        return NULL;
    }
    return &requests[command];
}

/**
 * @brief Encodes a chunk-transfer frame.
 *
 * Builds write, read, and finish frames in standard or checked mode. The three-bit phase occupies
 * selector bits 4 through 6, and checked mode sets selector bit 2. Chunk frames carry the fixed
 * 0x80, operation, and 0x40 header followed by the fragment index and length. Checked writes add
 * two zero trailer bytes, while checked reads add the 0x86 control byte and a zero trailer byte.
 *
 * @param[in] command Standard or checked write-chunk, read-chunk, or finish-transfer command.
 * @param[in] input Transfer phase, fragment index, length, and optional write payload.
 * @param[out] frame Encoded write bytes and expected response layout.
 * @return True when the command and chunk input are valid; otherwise false.
 */
bool i2c_probe_transfer_encode(I2cProbeCommand command, const I2cProbeTransferInput *input,
                               I2cProbeTransferFrame *frame) {
    bool write = command == I2C_PROBE_WRITE_CHUNK || command == I2C_PROBE_WRITE_CHECKED_CHUNK;
    bool read = command == I2C_PROBE_READ_CHUNK || command == I2C_PROBE_READ_CHECKED_CHUNK;
    bool finish =
        command == I2C_PROBE_FINISH_TRANSFER || command == I2C_PROBE_FINISH_CHECKED_TRANSFER;
    bool checked = command == I2C_PROBE_WRITE_CHECKED_CHUNK ||
                   command == I2C_PROBE_READ_CHECKED_CHUNK ||
                   command == I2C_PROBE_FINISH_CHECKED_TRANSFER;

    if ((!write && !read && !finish) || input->chunk_length > I2C_PROBE_TRANSFER_CHUNK_CAPACITY ||
        (write && input->chunk_length != 0 && input->chunk == NULL)) {
        return false;
    }

    *frame = (I2cProbeTransferFrame){0};
    frame->selector = (uint8_t)(((input->phase & 0x07u) << 4) | (checked ? 0x04u : 0u));
    frame->write_data[1] = 0x80;

    if (finish) {
        frame->write_data[0] = checked ? 4 : 5;
        frame->write_data[2] = 0x48;
        frame->write_length = checked ? 5 : 6;
        frame->response_length = checked ? 5 : 4;
        return true;
    }

    frame->write_data[0] =
        write ? (uint8_t)(input->chunk_length + (checked ? 7u : 5u)) : (checked ? 7 : 5);
    frame->write_data[2] = write ? 0x44 : 0x46;
    frame->write_data[3] = 0x40;
    frame->write_data[4] = input->chunk_index;
    frame->write_data[5] = input->chunk_length;
    frame->write_length = (uint8_t)(frame->write_data[0] + 1u);

    if (write) {
        if (input->chunk_length != 0) {
            memcpy(frame->write_data + 6, input->chunk, input->chunk_length);
        }
        frame->response_length = checked ? 5 : 4;
    } else {
        if (checked) {
            frame->write_data[6] = 0x86;
        }
        frame->response_length = (uint8_t)(input->chunk_length + 4u);
        frame->response_payload_offset = 2;
        frame->response_payload_length = input->chunk_length;
    }
    return true;
}

/**
 * @brief Initializes the probe data-transfer sequence.
 *
 * Starts at phase zero with the first 64-byte write chunk and selects either the standard or
 * checked command family for every sequence step.
 *
 * @param[out] sequence Transfer sequence to initialize.
 * @param[in] checked True to use the checked write, read, and finish commands.
 */
void i2c_probe_transfer_sequence_init(I2cProbeTransferSequence *sequence, bool checked) {
    *sequence = (I2cProbeTransferSequence){
        .stage = I2C_PROBE_TRANSFER_WRITING,
        .checked = checked,
    };
}

/**
 * @brief Describes the current probe transfer step.
 *
 * Produces four 64-byte write steps, sixteen 64-byte read steps, one 16-byte read step, and one
 * finish step. Buffer offsets refer to the start of the corresponding write source or read
 * destination fragment.
 *
 * @param[in] sequence Current transfer sequence state.
 * @param[out] step Command, phase, chunk index, length, and buffer offset for the current step.
 * @return True while a write, read, or finish step remains; otherwise false.
 */
bool i2c_probe_transfer_sequence_current(const I2cProbeTransferSequence *sequence,
                                         I2cProbeTransferStep *step) {
    if (sequence->stage == I2C_PROBE_TRANSFER_COMPLETE) {
        return false;
    }

    *step = (I2cProbeTransferStep){
        .phase = sequence->phase,
        .chunk_index = sequence->chunk_index,
    };
    if (sequence->stage == I2C_PROBE_TRANSFER_WRITING) {
        step->command = sequence->checked ? I2C_PROBE_WRITE_CHECKED_CHUNK : I2C_PROBE_WRITE_CHUNK;
        step->buffer_offset = (uint16_t)sequence->chunk_index * I2C_PROBE_TRANSFER_CHUNK_CAPACITY;
        step->chunk_length = I2C_PROBE_TRANSFER_CHUNK_CAPACITY;
    } else if (sequence->stage == I2C_PROBE_TRANSFER_READING) {
        step->command = sequence->checked ? I2C_PROBE_READ_CHECKED_CHUNK : I2C_PROBE_READ_CHUNK;
        step->buffer_offset = (uint16_t)sequence->chunk_index * I2C_PROBE_TRANSFER_CHUNK_CAPACITY;
        step->chunk_length = sequence->chunk_index == 16 ? 16 : I2C_PROBE_TRANSFER_CHUNK_CAPACITY;
    } else {
        step->command =
            sequence->checked ? I2C_PROBE_FINISH_CHECKED_TRANSFER : I2C_PROBE_FINISH_TRANSFER;
        step->chunk_index = 0;
    }
    return true;
}

/**
 * @brief Advances an accepted probe transfer step.
 *
 * Increments the phase after each accepted write or read chunk, changes from writing to reading
 * after 256 bytes, and changes from reading to finishing after 1,040 bytes. Accepting the finish
 * step completes the sequence.
 *
 * @param[in,out] sequence Transfer sequence to advance.
 * @return True when a pending step was accepted; otherwise false.
 */
bool i2c_probe_transfer_sequence_accept(I2cProbeTransferSequence *sequence) {
    if (sequence->stage == I2C_PROBE_TRANSFER_COMPLETE) {
        return false;
    }
    if (sequence->stage == I2C_PROBE_TRANSFER_FINISHING) {
        sequence->stage = I2C_PROBE_TRANSFER_COMPLETE;
        return true;
    }

    ++sequence->phase;
    ++sequence->chunk_index;
    if (sequence->stage == I2C_PROBE_TRANSFER_WRITING &&
        sequence->chunk_index ==
            I2C_PROBE_TRANSFER_WRITE_SIZE / I2C_PROBE_TRANSFER_CHUNK_CAPACITY) {
        sequence->stage = I2C_PROBE_TRANSFER_READING;
        sequence->chunk_index = 0;
    } else if (sequence->stage == I2C_PROBE_TRANSFER_READING &&
               (uint16_t)sequence->chunk_index * I2C_PROBE_TRANSFER_CHUNK_CAPACITY >=
                   I2C_PROBE_TRANSFER_READ_SIZE) {
        sequence->stage = I2C_PROBE_TRANSFER_FINISHING;
        sequence->chunk_index = 0;
    }
    return true;
}

/**
 * @brief Initializes one probe command exchange.
 *
 * Starts with the device-ready poll, clears its retry count, and marks the exchange pending.
 *
 * @param[out] exchange Command exchange to initialize.
 */
void i2c_probe_exchange_init(I2cProbeExchange *exchange) {
    *exchange = (I2cProbeExchange){
        .stage = I2C_PROBE_EXCHANGE_WAIT_READY,
        .result = I2C_PROBE_EXCHANGE_PENDING,
    };
}

/**
 * @brief Processes a ready or command-acceptance status.
 *
 * During the initial ready poll, response 0x07 advances to command submission, response 0x17
 * remains busy, and three unexpected responses are retried before the fourth fails. During command
 * acceptance, response 0x07 advances to the final response, response 0x17 remains busy, and every
 * other response fails immediately.
 *
 * @param[in,out] exchange Active command exchange.
 * @param[in] response Device status byte.
 * @return True when the exchange was waiting for this status; otherwise false.
 */
bool i2c_probe_exchange_status(I2cProbeExchange *exchange, uint8_t response) {
    I2cProbeResponseResult status;
    if (exchange->stage == I2C_PROBE_EXCHANGE_WAIT_READY) {
        status = i2c_probe_handshake_evaluate(&exchange->readiness, response);
        if (status == I2C_PROBE_RESPONSE_ACCEPTED) {
            exchange->stage = I2C_PROBE_EXCHANGE_QUEUE_COMMAND;
        } else if (status == I2C_PROBE_RESPONSE_REJECTED) {
            exchange->stage = I2C_PROBE_EXCHANGE_FAILED;
            exchange->result = I2C_PROBE_EXCHANGE_COMMAND_ERROR;
        }
        return true;
    }
    if (exchange->stage != I2C_PROBE_EXCHANGE_WAIT_ACCEPTANCE) {
        return false;
    }

    status = i2c_probe_command_response_evaluate(response);
    if (status == I2C_PROBE_RESPONSE_ACCEPTED) {
        exchange->stage = I2C_PROBE_EXCHANGE_WAIT_RESPONSE;
    } else if (status == I2C_PROBE_RESPONSE_REJECTED) {
        exchange->stage = I2C_PROBE_EXCHANGE_FAILED;
        exchange->result = I2C_PROBE_EXCHANGE_COMMAND_ERROR;
    }
    return true;
}

/**
 * @brief Records successful submission of the probe command.
 *
 * Advances a queued command to acceptance polling. A busy transport leaves the exchange in the
 * queue stage and should not call this function.
 *
 * @param[in,out] exchange Active command exchange.
 * @return True when a command was waiting to be submitted; otherwise false.
 */
bool i2c_probe_exchange_command_queued(I2cProbeExchange *exchange) {
    if (exchange->stage != I2C_PROBE_EXCHANGE_QUEUE_COMMAND) {
        return false;
    }
    exchange->stage = I2C_PROBE_EXCHANGE_WAIT_ACCEPTANCE;
    return true;
}

/**
 * @brief Validates and completes the final probe response.
 *
 * Completes responses whose status bytes are zero and whose optional payload XOR matches. Status
 * failures and checksum failures remain distinct for the transfer controller.
 *
 * @param[in,out] exchange Active command exchange.
 * @param[in] response Final response payload, checksum, and status fields.
 * @param[in] checksum_enabled True to validate the response payload XOR.
 * @return True when the exchange was waiting for its final response; otherwise false.
 */
bool i2c_probe_exchange_finalize(I2cProbeExchange *exchange, const I2cProbeFinalResponse *response,
                                 bool checksum_enabled) {
    if (exchange->stage != I2C_PROBE_EXCHANGE_WAIT_RESPONSE) {
        return false;
    }

    I2cProbeValidationResult validation =
        i2c_probe_final_response_validate(response, checksum_enabled);
    if (validation == I2C_PROBE_VALID) {
        exchange->stage = I2C_PROBE_EXCHANGE_COMPLETE;
        exchange->result = I2C_PROBE_EXCHANGE_SUCCEEDED;
    } else {
        exchange->stage = I2C_PROBE_EXCHANGE_FAILED;
        exchange->result = validation == I2C_PROBE_CHECKSUM_ERROR
                               ? I2C_PROBE_EXCHANGE_CHECKSUM_ERROR
                               : I2C_PROBE_EXCHANGE_RESPONSE_ERROR;
    }
    return true;
}

/**
 * @brief Initializes the accessory startup sequence.
 *
 * Selects the session-begin command and clears the retry and completion state.
 *
 * @param[out] startup Startup sequence to initialize.
 */
void i2c_probe_startup_init(I2cProbeStartup *startup) {
    *startup = (I2cProbeStartup){.command = I2C_PROBE_BEGIN_SESSION};
}

/**
 * @brief Gets the next accessory startup command when it is ready.
 *
 * Holds startup-status retries through their five-millisecond deadline. A retry becomes available
 * on the first later millisecond. Completed startup sequences have no current command.
 *
 * @param[in,out] startup Startup sequence whose delay state may expire.
 * @param[in] now_ms Current system time in milliseconds.
 * @param[out] command Command to execute next.
 * @return True when a command is ready; otherwise false.
 */
bool i2c_probe_startup_current(I2cProbeStartup *startup, uint32_t now_ms,
                               I2cProbeCommand *command) {
    if (startup->complete) {
        return false;
    }
    if (startup->waiting) {
        if ((int32_t)(now_ms - startup->retry_after_ms) <= 0) {
            return false;
        }
        startup->waiting = false;
    }
    *command = startup->command;
    return true;
}

/**
 * @brief Applies one completed accessory startup command.
 *
 * Advances through session start, startup status, the fixed 29-byte identity signature, the 0xCC
 * confirmation, and ready status 7. Invalid startup status waits five milliseconds before another
 * read. Invalid signature, confirmation, and nonnegative ready responses repeat immediately.
 * Four completed attempts at one response-bearing stage restart the sequence, as does a negative
 * ready status.
 *
 * @param[in,out] startup Active startup sequence.
 * @param[in] command Command whose exchange completed.
 * @param[in] response Response fields, or null for the session-begin command.
 * @param[in] now_ms Current system time in milliseconds.
 * @return True when the event belongs to the current sequence state; otherwise false.
 */
bool i2c_probe_startup_accept(I2cProbeStartup *startup, I2cProbeCommand command,
                              const I2cProbeStartupResponse *response, uint32_t now_ms) {
    if (startup->complete || startup->waiting || command != startup->command ||
        (command != I2C_PROBE_BEGIN_SESSION && response == NULL)) {
        return false;
    }

    if (command == I2C_PROBE_BEGIN_SESSION) {
        startup->command = I2C_PROBE_READ_STARTUP_STATUS;
        startup->completed_attempts = 0;
        return true;
    }

    ++startup->completed_attempts;
    if (startup->completed_attempts > I2C_PROBE_STARTUP_ATTEMPT_LIMIT) {
        i2c_probe_startup_init(startup);
        return true;
    }

    if (command == I2C_PROBE_READ_STARTUP_STATUS) {
        if (response->declared_length == 1 && response->status == 0) {
            startup->command = I2C_PROBE_READ_SIGNATURE;
            startup->completed_attempts = 0;
        } else {
            startup->waiting = true;
            startup->retry_after_ms = now_ms + I2C_PROBE_STARTUP_RETRY_DELAY_MS;
        }
    } else if (command == I2C_PROBE_READ_SIGNATURE) {
        if (response->status == 0 && response->payload != NULL &&
            response->payload_length >= I2C_PROBE_STARTUP_SIGNATURE_SIZE &&
            memcmp(response->payload, startup_signature, I2C_PROBE_STARTUP_SIGNATURE_SIZE) == 0) {
            startup->command = I2C_PROBE_READ_CONFIRMATION;
            startup->completed_attempts = 0;
        }
    } else if (command == I2C_PROBE_READ_CONFIRMATION) {
        if (response->declared_length == 1 && response->status == 0xcc) {
            startup->command = I2C_PROBE_READ_READY_STATUS;
            startup->completed_attempts = 0;
        }
    } else if ((int8_t)response->status < 0) {
        i2c_probe_startup_init(startup);
    } else if (response->status == I2C_PROBE_RESPONSE_ACCEPT) {
        startup->completed_attempts = 0;
        startup->complete = true;
    }
    return true;
}
