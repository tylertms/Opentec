#include "secure_element/a71ch.h"

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

/** @brief Internal A71CH status and session constants. */
enum {
    A71CH_STATUS_READY = 0x07,        /**< Status byte indicating that the device is ready. */
    A71CH_STATUS_PROCESSING = 0x17,   /**< Status byte indicating that processing continues. */
    A71CH_UNEXPECTED_RETRY_LIMIT = 2, /**< Maximum retry count for unexpected status bytes. */
    A71CH_SESSION_ATTEMPT_LIMIT =
        3, /**< Maximum completed attempts at one stage before restarting the session. */
    A71CH_SESSION_RETRY_DELAY_MS = 5, /**< Delay before retrying an invalid soft-reset response. */
    A71CH_ATR_SIZE = 29,              /**< Number of bytes in the expected answer-to-reset. */
};

/** @brief Expected A71CH answer-to-reset byte sequence. */
static const uint8_t expected_atr[A71CH_ATR_SIZE] = {
    0xb8, 0x04, 0x11, 0x01, 0x05, 0x04, 0xb9, 0x02, 0x01, 0x01, 0xba, 0x01, 0x01, 0xbb, 0x0c,
    0x41, 0x37, 0x31, 0x30, 0x35, 0x43, 0x43, 0x32, 0x34, 0x32, 0x52, 0x31, 0xbc, 0x00,
};

/** @brief Fixed-control request metadata indexed by A71chCommand. */
static const A71chControlRequest control_requests[] = {
    [A71CH_WAKE_UP] = {.selector = 0x0f},
    [A71CH_SOFT_RESET] = {.selector = 0x1f, .response_length = 2},
    [A71CH_READ_ANSWER_TO_RESET] = {.selector = 0x2f, .response_length = 0x1f},
    [A71CH_PARAMETER_EXCHANGE] = {.selector = 0xff, .response_length = 2},
    [A71CH_READ_STATUS] = {.selector = 0x07, .response_length = 2},
};

/**
 * @brief Initializes SCI2C status polling.
 *
 * Clears the retry count used to limit repeated unexpected status responses.
 *
 * @param[out] poll Status polling state to initialize.
 */
void a71ch_status_poll_init(A71chStatusPoll *poll) { poll->retry_count = 0; }

/**
 * @brief Evaluates one SCI2C status response while waiting for the A71CH.
 *
 * Status 0x07 reports that command processing is complete and clears the retry count. Status 0x17
 * reports that processing continues. Other values are retried until the retry limit is exceeded.
 *
 * @param[in,out] poll Persistent status retry state.
 * @param[in] response SCI2C status response byte.
 * @return Accepted, busy, retry, or rejected response disposition.
 */
A71chStatusResult a71ch_status_poll_evaluate(A71chStatusPoll *poll, uint8_t response) {
    if (response == A71CH_STATUS_READY) {
        poll->retry_count = 0;
        return A71CH_STATUS_ACCEPTED;
    }
    if (response == A71CH_STATUS_PROCESSING) {
        ++poll->retry_count;
        return A71CH_STATUS_BUSY;
    }
    if (poll->retry_count <= A71CH_UNEXPECTED_RETRY_LIMIT) {
        ++poll->retry_count;
        return A71CH_STATUS_RETRY;
    }
    return A71CH_STATUS_REJECTED;
}

/**
 * @brief Evaluates the SCI2C status returned after an authentication APDU.
 *
 * Status 0x07 permits response retrieval, status 0x17 keeps the exchange pending, and every other
 * value rejects the exchange.
 *
 * @param[in] response SCI2C status response byte.
 * @return Accepted, busy, or rejected response disposition.
 */
A71chStatusResult a71ch_command_response_evaluate(uint8_t response) {
    if (response == A71CH_STATUS_READY) {
        return A71CH_STATUS_ACCEPTED;
    }
    if (response == A71CH_STATUS_PROCESSING) {
        return A71CH_STATUS_BUSY;
    }
    return A71CH_STATUS_REJECTED;
}

/**
 * @brief Calculates an SCI2C longitudinal redundancy check.
 *
 * XORs every payload byte into an accumulator initialized to zero.
 *
 * @param[in] payload Bytes covered by the check.
 * @param[in] payload_length Number of payload bytes to include.
 * @return XOR of all payload bytes, or zero for an empty payload.
 */
uint8_t a71ch_lrc(const uint8_t *payload, uint8_t payload_length) {
    uint8_t lrc = 0;
    for (uint8_t index = 0; index < payload_length; ++index) {
        lrc ^= payload[index];
    }
    return lrc;
}

/**
 * @brief Encodes a fixed A71CH SCI2C control request.
 *
 * Maps wake-up, soft reset, read-answer-to-reset, parameter exchange, and status commands to their
 * protocol control byte and complete response length. These commands carry no separate payload.
 *
 * @param[in] command Session control command to encode.
 * @param[out] request Encoded control byte and response length.
 * @return True for a session control command; otherwise false.
 */
bool a71ch_control_request_encode(A71chCommand command, A71chControlRequest *request) {
    const A71chControlRequest *encoded = a71ch_control_request_lookup(command);
    if (encoded == NULL) {
        return false;
    }
    *request = *encoded;
    return true;
}

/**
 * @brief Looks up a fixed A71CH SCI2C control request.
 *
 * Returns the immutable control byte and response length used by a session control command.
 *
 * @param[in] command Session control command to look up.
 * @return Fixed request descriptor for a session control command, or null for another command.
 */
const A71chControlRequest *a71ch_control_request_lookup(A71chCommand command) {
    if (command < A71CH_WAKE_UP || command > A71CH_READ_STATUS) {
        return NULL;
    }
    return &control_requests[command];
}

/**
 * @brief Encodes one A71CH authentication APDU exchange.
 *
 * Builds authentication upload, retrieval, and finalization commands with optional SCI2C LRC use.
 * The rolling SCI2C sequence occupies control-byte bits four through six. Authentication APDUs use
 * class 0x80, instructions 0x44, 0x46, and 0x48, and fragments no larger than 64 bytes.
 *
 * @param[in] command Authentication upload, retrieval, or finalization command.
 * @param[in] input SCI2C sequence, fragment index, length, and optional upload data.
 * @param[out] frame Encoded APDU and expected response layout.
 * @return True when the command and fragment are valid; otherwise false.
 */
bool a71ch_authentication_encode(A71chCommand command, const A71chAuthenticationInput *input,
                                 A71chAuthenticationFrame *frame) {
    bool write = command == A71CH_AUTHENTICATION_WRITE || command == A71CH_AUTHENTICATION_WRITE_LRC;
    bool read = command == A71CH_AUTHENTICATION_READ || command == A71CH_AUTHENTICATION_READ_LRC;
    bool finish =
        command == A71CH_AUTHENTICATION_FINALIZE || command == A71CH_AUTHENTICATION_FINALIZE_LRC;
    bool use_lrc = command == A71CH_AUTHENTICATION_WRITE_LRC ||
                   command == A71CH_AUTHENTICATION_READ_LRC ||
                   command == A71CH_AUTHENTICATION_FINALIZE_LRC;

    if ((!write && !read && !finish) || input->chunk_length > A71CH_AUTHENTICATION_CHUNK_CAPACITY ||
        (write && input->chunk_length != 0 && input->chunk == NULL)) {
        return false;
    }

    *frame = (A71chAuthenticationFrame){0};
    frame->selector = (uint8_t)(((input->phase & 0x07u) << 4) | (use_lrc ? 0x04u : 0u));
    frame->write_data[1] = 0x80;

    if (finish) {
        frame->write_data[0] = use_lrc ? 4 : 5;
        frame->write_data[2] = 0x48;
        frame->write_length = use_lrc ? 5 : 6;
        frame->response_length = use_lrc ? 5 : 4;
        return true;
    }

    frame->write_data[0] =
        write ? (uint8_t)(input->chunk_length + (use_lrc ? 7u : 5u)) : (use_lrc ? 7 : 5);
    frame->write_data[2] = write ? 0x44 : 0x46;
    frame->write_data[3] = 0x40;
    frame->write_data[4] = input->chunk_index;
    frame->write_data[5] = input->chunk_length;
    frame->write_length = (uint8_t)(frame->write_data[0] + 1u);

    if (write) {
        if (input->chunk_length != 0) {
            memcpy(frame->write_data + 6, input->chunk, input->chunk_length);
        }
        frame->response_length = use_lrc ? 5 : 4;
    } else {
        if (use_lrc) {
            frame->write_data[6] = 0x86;
            frame->response_integrity_offset = 2;
            frame->response_integrity_length = (uint8_t)(input->chunk_length + 2u);
        }
        frame->response_length = (uint8_t)(input->chunk_length + 4u);
        frame->response_payload_offset = 2;
        frame->response_payload_length = input->chunk_length;
    }
    return true;
}

/**
 * @brief Parses one A71CH authentication APDU response.
 *
 * Requires the response length selected by the command and exposes retrieval data after the SCI2C
 * length and control bytes. LRC responses must reduce to zero across the covered response bytes.
 * Upload and finalization responses do not expose authentication data.
 *
 * @param[in] frame Encoded command and expected response layout.
 * @param[in] response Received response bytes.
 * @param[in] response_length Number of received bytes.
 * @param[out] parsed_response Payload view produced for a valid response.
 * @return Valid, LRC-error, or malformed-response result.
 */
A71chFrameValidation
a71ch_authentication_response_parse(const A71chAuthenticationFrame *frame, const uint8_t *response,
                                    uint8_t response_length,
                                    A71chAuthenticationResponse *parsed_response) {
    if (parsed_response == NULL) {
        return A71CH_MALFORMED_RESPONSE;
    }
    *parsed_response = (A71chAuthenticationResponse){0};
    if (frame == NULL || response == NULL || response_length != frame->response_length ||
        (uint16_t)frame->response_payload_offset + frame->response_payload_length >
            response_length ||
        (uint16_t)frame->response_integrity_offset + frame->response_integrity_length >
            response_length) {
        return A71CH_MALFORMED_RESPONSE;
    }

    if (frame->response_integrity_length != 0 &&
        a71ch_lrc(response + frame->response_integrity_offset, frame->response_integrity_length) !=
            0) {
        return A71CH_LRC_ERROR;
    }
    if (frame->response_payload_length != 0) {
        parsed_response->payload = response + frame->response_payload_offset;
        parsed_response->payload_length = frame->response_payload_length;
    }
    return A71CH_VALID;
}

/**
 * @brief Initializes the A71CH authentication exchange sequence.
 *
 * Starts with the first 64-byte upload fragment and selects either the plain or LRC command family
 * for the complete exchange.
 *
 * @param[out] sequence Authentication sequence to initialize.
 * @param[in] use_lrc True to use SCI2C LRC commands.
 */
void a71ch_authentication_sequence_init(A71chAuthenticationSequence *sequence, bool use_lrc) {
    *sequence = (A71chAuthenticationSequence){
        .stage = A71CH_AUTHENTICATION_WRITING,
        .use_lrc = use_lrc,
    };
}

/**
 * @brief Describes the current A71CH authentication step.
 *
 * Produces four 64-byte upload steps, sixteen 64-byte retrieval steps, one 16-byte retrieval step,
 * and one finalization step. Buffer offsets select the corresponding request or response fragment.
 *
 * @param[in] sequence Current transfer sequence state.
 * @param[out] step Command, phase, chunk index, length, and buffer offset for the current step.
 * @return True while an upload, retrieval, or finalization step remains; otherwise false.
 */
bool a71ch_authentication_sequence_current(const A71chAuthenticationSequence *sequence,
                                           A71chAuthenticationStep *step) {
    if (sequence->stage == A71CH_AUTHENTICATION_COMPLETE) {
        return false;
    }

    *step = (A71chAuthenticationStep){
        .phase = sequence->phase,
        .chunk_index = sequence->chunk_index,
    };
    if (sequence->stage == A71CH_AUTHENTICATION_WRITING) {
        step->command =
            sequence->use_lrc ? A71CH_AUTHENTICATION_WRITE_LRC : A71CH_AUTHENTICATION_WRITE;
        step->buffer_offset = (uint16_t)sequence->chunk_index * A71CH_AUTHENTICATION_CHUNK_CAPACITY;
        step->chunk_length = A71CH_AUTHENTICATION_CHUNK_CAPACITY;
    } else if (sequence->stage == A71CH_AUTHENTICATION_READING) {
        step->command =
            sequence->use_lrc ? A71CH_AUTHENTICATION_READ_LRC : A71CH_AUTHENTICATION_READ;
        step->buffer_offset = (uint16_t)sequence->chunk_index * A71CH_AUTHENTICATION_CHUNK_CAPACITY;
        step->chunk_length = sequence->chunk_index == 16 ? 16 : A71CH_AUTHENTICATION_CHUNK_CAPACITY;
    } else {
        step->command =
            sequence->use_lrc ? A71CH_AUTHENTICATION_FINALIZE_LRC : A71CH_AUTHENTICATION_FINALIZE;
        step->chunk_index = 0;
    }
    return true;
}

/**
 * @brief Advances an accepted A71CH authentication step.
 *
 * Increments the phase after each accepted write or read chunk, changes from writing to reading
 * after 256 bytes, and changes from reading to finishing after 1,040 bytes. Accepting the finish
 * step completes the sequence.
 *
 * @param[in,out] sequence Transfer sequence to advance.
 * @return True when a pending step was accepted; otherwise false.
 */
bool a71ch_authentication_sequence_accept(A71chAuthenticationSequence *sequence) {
    if (sequence->stage == A71CH_AUTHENTICATION_COMPLETE) {
        return false;
    }
    if (sequence->stage == A71CH_AUTHENTICATION_FINISHING) {
        sequence->stage = A71CH_AUTHENTICATION_COMPLETE;
        return true;
    }

    ++sequence->phase;
    ++sequence->chunk_index;
    if (sequence->stage == A71CH_AUTHENTICATION_WRITING &&
        sequence->chunk_index ==
            A71CH_AUTHENTICATION_WRITE_SIZE / A71CH_AUTHENTICATION_CHUNK_CAPACITY) {
        sequence->stage = A71CH_AUTHENTICATION_READING;
        sequence->chunk_index = 0;
    } else if (sequence->stage == A71CH_AUTHENTICATION_READING &&
               (uint16_t)sequence->chunk_index * A71CH_AUTHENTICATION_CHUNK_CAPACITY >=
                   A71CH_AUTHENTICATION_READ_SIZE) {
        sequence->stage = A71CH_AUTHENTICATION_FINISHING;
        sequence->chunk_index = 0;
    }
    return true;
}

/**
 * @brief Initializes one A71CH authentication APDU exchange.
 *
 * Starts with an SCI2C status poll and marks the APDU exchange pending.
 *
 * @param[out] exchange Command exchange to initialize.
 */
void a71ch_exchange_init(A71chExchange *exchange) {
    *exchange = (A71chExchange){
        .stage = A71CH_EXCHANGE_WAIT_READY,
        .result = A71CH_EXCHANGE_PENDING,
    };
}

/**
 * @brief Processes a ready or command-acceptance status.
 *
 * During the initial ready poll, response 0x07 advances to command submission, response 0x17
 * remains busy, and unexpected responses are retried until the retry limit is exceeded. During
 * command acceptance, response 0x07 advances to the final response, response 0x17 remains busy,
 * and every other response fails immediately.
 *
 * @param[in,out] exchange Active command exchange.
 * @param[in] response Device status byte.
 * @return True when the exchange was waiting for this status; otherwise false.
 */
bool a71ch_exchange_status(A71chExchange *exchange, uint8_t response) {
    A71chStatusResult status;
    if (exchange->stage == A71CH_EXCHANGE_WAIT_READY) {
        status = a71ch_status_poll_evaluate(&exchange->readiness, response);
        if (status == A71CH_STATUS_ACCEPTED) {
            exchange->stage = A71CH_EXCHANGE_QUEUE_COMMAND;
        } else if (status == A71CH_STATUS_REJECTED) {
            exchange->stage = A71CH_EXCHANGE_FAILED;
            exchange->result = A71CH_EXCHANGE_COMMAND_ERROR;
        }
        return true;
    }
    if (exchange->stage != A71CH_EXCHANGE_WAIT_ACCEPTANCE) {
        return false;
    }

    status = a71ch_command_response_evaluate(response);
    if (status == A71CH_STATUS_ACCEPTED) {
        exchange->stage = A71CH_EXCHANGE_WAIT_RESPONSE;
    } else if (status == A71CH_STATUS_REJECTED) {
        exchange->stage = A71CH_EXCHANGE_FAILED;
        exchange->result = A71CH_EXCHANGE_COMMAND_ERROR;
    }
    return true;
}

/**
 * @brief Records successful submission of the authentication APDU.
 *
 * Advances a queued command to acceptance polling. A busy transport leaves the exchange in the
 * queue stage and should not call this function.
 *
 * @param[in,out] exchange Active command exchange.
 * @return True when a command was waiting to be submitted; otherwise false.
 */
bool a71ch_exchange_command_queued(A71chExchange *exchange) {
    if (exchange->stage != A71CH_EXCHANGE_QUEUE_COMMAND) {
        return false;
    }
    exchange->stage = A71CH_EXCHANGE_WAIT_ACCEPTANCE;
    return true;
}

/**
 * @brief Applies the final authentication response validation.
 *
 * Completes a valid response and preserves LRC and malformed-response failures as distinct results.
 *
 * @param[in,out] exchange Active command exchange.
 * @param[in] validation Parsed response disposition.
 * @return True when the exchange was waiting for its final response; otherwise false.
 */
bool a71ch_exchange_finalize(A71chExchange *exchange, A71chFrameValidation validation) {
    if (exchange->stage != A71CH_EXCHANGE_WAIT_RESPONSE) {
        return false;
    }

    if (validation == A71CH_VALID) {
        exchange->stage = A71CH_EXCHANGE_COMPLETE;
        exchange->result = A71CH_EXCHANGE_SUCCEEDED;
    } else {
        exchange->stage = A71CH_EXCHANGE_FAILED;
        exchange->result = validation == A71CH_LRC_ERROR ? A71CH_EXCHANGE_LRC_ERROR
                                                         : A71CH_EXCHANGE_RESPONSE_ERROR;
    }
    return true;
}

/**
 * @brief Initializes the A71CH SCI2C session sequence.
 *
 * Selects wake-up as the first command and clears retry and completion state.
 *
 * @param[out] session Session sequence to initialize.
 */
void a71ch_session_init(A71chSession *session) {
    *session = (A71chSession){.command = A71CH_WAKE_UP};
}

/**
 * @brief Gets the next A71CH session command when it is ready.
 *
 * Holds failed soft-reset responses through their five-millisecond retry deadline. Completed
 * sessions have no current command.
 *
 * @param[in,out] session Session sequence whose delay state may expire.
 * @param[in] now_ms Current system time in milliseconds.
 * @param[out] command Command to execute next.
 * @return True when a command is ready; otherwise false.
 */
bool a71ch_session_current(A71chSession *session, uint32_t now_ms, A71chCommand *command) {
    if (session->complete) {
        return false;
    }
    if (session->waiting) {
        if ((int32_t)(now_ms - session->retry_after_ms) <= 0) {
            return false;
        }
        session->waiting = false;
    }
    *command = session->command;
    return true;
}

/**
 * @brief Applies one completed A71CH session command.
 *
 * Advances through wake-up, soft reset, the 29-byte A71CH answer-to-reset, a parameter-exchange
 * response with status 0xCC, and ready status 0x07. Invalid soft-reset responses wait five
 * milliseconds. Four failed responses at one stage restart the session, as does a negative final
 * status.
 *
 * @param[in,out] session Active session sequence.
 * @param[in] command Command whose exchange completed.
 * @param[in] response Response fields, or null for wake-up.
 * @param[in] now_ms Current system time in milliseconds.
 * @return True when the event belongs to the current sequence state; otherwise false.
 */
bool a71ch_session_accept(A71chSession *session, A71chCommand command,
                          const A71chSessionResponse *response, uint32_t now_ms) {
    if (session->complete || session->waiting || command != session->command ||
        (command != A71CH_WAKE_UP && response == NULL)) {
        return false;
    }

    if (command == A71CH_WAKE_UP) {
        session->command = A71CH_SOFT_RESET;
        session->completed_attempts = 0;
        return true;
    }

    ++session->completed_attempts;
    if (session->completed_attempts > A71CH_SESSION_ATTEMPT_LIMIT) {
        a71ch_session_init(session);
        return true;
    }

    if (command == A71CH_SOFT_RESET) {
        if (response->declared_length == 1 && response->status == 0) {
            session->command = A71CH_READ_ANSWER_TO_RESET;
            session->completed_attempts = 0;
        } else {
            session->waiting = true;
            session->retry_after_ms = now_ms + A71CH_SESSION_RETRY_DELAY_MS;
        }
    } else if (command == A71CH_READ_ANSWER_TO_RESET) {
        if (response->status == 0 && response->payload != NULL &&
            response->payload_length >= A71CH_ATR_SIZE &&
            memcmp(response->payload, expected_atr, A71CH_ATR_SIZE) == 0) {
            session->command = A71CH_PARAMETER_EXCHANGE;
            session->completed_attempts = 0;
        }
    } else if (command == A71CH_PARAMETER_EXCHANGE) {
        if (response->declared_length == 1 && response->status == 0xcc) {
            session->command = A71CH_READ_STATUS;
            session->completed_attempts = 0;
        }
    } else if ((int8_t)response->status < 0) {
        a71ch_session_init(session);
    } else if (response->status == A71CH_STATUS_READY) {
        session->completed_attempts = 0;
        session->complete = true;
    }
    return true;
}
