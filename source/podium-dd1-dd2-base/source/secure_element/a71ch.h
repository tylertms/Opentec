#ifndef OPENTEC_BASE_A71CH_H
#define OPENTEC_BASE_A71CH_H

#include <stdbool.h>
#include <stdint.h>

/** @brief Disposition returned while evaluating a secure-element status byte. */
typedef enum {
    A71CH_STATUS_RETRY,    /**< The status should be retried because it was unexpected. */
    A71CH_STATUS_BUSY,     /**< The secure element is still processing the command. */
    A71CH_STATUS_ACCEPTED, /**< The secure element is ready for the next exchange step. */
    A71CH_STATUS_REJECTED, /**< The status retry limit was exceeded or the command was rejected. */
} A71chStatusResult;

/** @brief Validation result for an authentication response frame. */
typedef enum {
    A71CH_VALID,              /**< The response has the expected layout and integrity. */
    A71CH_LRC_ERROR,          /**< The response LRC does not validate. */
    A71CH_MALFORMED_RESPONSE, /**< The response does not match the expected frame layout. */
} A71chFrameValidation;

/** @brief Retry state for SCI2C status polling. */
typedef struct {
    uint8_t retry_count; /**< Number of status responses counted in this poll sequence. */
} A71chStatusPoll;

/** @brief Command steps used by the A71CH protocol. */
typedef enum {
    A71CH_WAKE_UP = 1,                 /**< Wake the secure element. */
    A71CH_SOFT_RESET = 2,              /**< Reset the secure element. */
    A71CH_READ_ANSWER_TO_RESET = 3,    /**< Read and validate the answer-to-reset bytes. */
    A71CH_PARAMETER_EXCHANGE = 4,      /**< Exchange the secure-element protocol parameters. */
    A71CH_READ_STATUS = 5,             /**< Read the secure-element status. */
    A71CH_AUTHENTICATION_WRITE,        /**< Upload one authentication request fragment. */
    A71CH_AUTHENTICATION_WRITE_LRC,    /**< Upload one authentication request fragment with LRC. */
    A71CH_AUTHENTICATION_READ,         /**< Read one authentication response fragment. */
    A71CH_AUTHENTICATION_READ_LRC,     /**< Read one authentication response fragment with LRC. */
    A71CH_AUTHENTICATION_FINALIZE,     /**< Finalize authentication. */
    A71CH_AUTHENTICATION_FINALIZE_LRC, /**< Finalize authentication with LRC. */
} A71chCommand;

/** @brief Encoded fixed-control request metadata. */
typedef struct {
    uint8_t selector;        /**< SCI2C control selector sent for the command. */
    uint8_t response_length; /**< Number of response bytes expected after the command. */
} A71chControlRequest;

/** @brief Authentication transfer sizing constants. */
enum {
    A71CH_AUTHENTICATION_CHUNK_CAPACITY =
        64, /**< Maximum authentication payload bytes per fragment. */
    A71CH_AUTHENTICATION_WRITE_CAPACITY =
        A71CH_AUTHENTICATION_CHUNK_CAPACITY +
        8, /**< Capacity of an encoded authentication write body. */
    A71CH_AUTHENTICATION_WRITE_SIZE = 0x100, /**< Number of bytes in the authentication request. */
    A71CH_AUTHENTICATION_READ_SIZE = 0x410,  /**< Number of bytes in the authentication response. */
};

/** @brief Input fragment metadata used to build an authentication frame. */
typedef struct {
    uint8_t phase;        /**< Authentication phase encoded in the SCI2C selector. */
    uint8_t chunk_index;  /**< Zero-based fragment index within the current transfer direction. */
    const uint8_t *chunk; /**< Fragment bytes to upload, or null for read and finalize commands. */
    uint8_t chunk_length; /**< Number of fragment bytes to upload or retrieve. */
} A71chAuthenticationInput;

/** @brief Encoded authentication APDU and expected response layout. */
typedef struct {
    uint8_t selector; /**< SCI2C control selector for this frame. */
    uint8_t write_data[A71CH_AUTHENTICATION_WRITE_CAPACITY]; /**< Length-prefixed APDU body sent to
                                                                the secure element. */
    uint8_t write_length;              /**< Number of bytes in write_data that are transmitted. */
    uint8_t response_length;           /**< Number of response bytes expected for this frame. */
    uint8_t response_payload_offset;   /**< Offset of the returned authentication payload. */
    uint8_t response_payload_length;   /**< Length of the returned authentication payload. */
    uint8_t response_integrity_offset; /**< Offset of the response bytes covered by LRC. */
    uint8_t response_integrity_length; /**< Number of response bytes covered by LRC. */
} A71chAuthenticationFrame;

/** @brief View of payload bytes parsed from an authentication response. */
typedef struct {
    const uint8_t *payload; /**< Pointer into received payload, or null when absent. */
    uint8_t payload_length; /**< Number of payload bytes available through payload. */
} A71chAuthenticationResponse;

/** @brief Stage of the multi-fragment authentication transfer. */
typedef enum {
    A71CH_AUTHENTICATION_WRITING,   /**< Upload fragments are being generated. */
    A71CH_AUTHENTICATION_READING,   /**< Response fragments are being read. */
    A71CH_AUTHENTICATION_FINISHING, /**< The final authentication command is being generated. */
    A71CH_AUTHENTICATION_COMPLETE,  /**< All authentication fragments have completed. */
} A71chAuthenticationStage;

/** @brief State machine for the authentication fragment sequence. */
typedef struct {
    A71chAuthenticationStage stage; /**< Current transfer stage. */
    uint8_t phase;                  /**< Next SCI2C phase value. */
    uint8_t chunk_index;            /**< Next fragment index within the current stage. */
    bool use_lrc;                   /**< Whether the LRC command family is selected. */
} A71chAuthenticationSequence;

/** @brief Description of the next authentication exchange step. */
typedef struct {
    A71chCommand command;   /**< Protocol command for the step. */
    uint16_t buffer_offset; /**< Offset into the request or response buffer. */
    uint8_t phase;          /**< SCI2C phase value for the step. */
    uint8_t chunk_index;    /**< Fragment index for the step. */
    uint8_t chunk_length;   /**< Number of bytes in the fragment. */
} A71chAuthenticationStep;

/** @brief Stage of one asynchronous authentication APDU exchange. */
typedef enum {
    A71CH_EXCHANGE_WAIT_READY,      /**< Waiting for the secure element to become ready. */
    A71CH_EXCHANGE_QUEUE_COMMAND,   /**< Waiting to submit the encoded command. */
    A71CH_EXCHANGE_WAIT_ACCEPTANCE, /**< Waiting for command acceptance. */
    A71CH_EXCHANGE_WAIT_RESPONSE,   /**< Waiting to read the command response. */
    A71CH_EXCHANGE_COMPLETE,        /**< The exchange completed successfully. */
    A71CH_EXCHANGE_FAILED,          /**< The exchange failed. */
} A71chExchangeStage;

/** @brief Result of one asynchronous authentication APDU exchange. */
typedef enum {
    A71CH_EXCHANGE_PENDING,        /**< No terminal result is available yet. */
    A71CH_EXCHANGE_SUCCEEDED,      /**< The exchange completed successfully. */
    A71CH_EXCHANGE_COMMAND_ERROR,  /**< Command readiness or acceptance status was rejected. */
    A71CH_EXCHANGE_LRC_ERROR,      /**< The response failed its LRC check. */
    A71CH_EXCHANGE_RESPONSE_ERROR, /**< The response was malformed. */
} A71chExchangeResult;

/** @brief State for one asynchronous authentication APDU exchange. */
typedef struct {
    A71chExchangeStage stage;   /**< Current exchange stage. */
    A71chExchangeResult result; /**< Current exchange result. */
    A71chStatusPoll readiness;  /**< Retry state for the initial readiness poll. */
} A71chExchange;

/** @brief Parsed response fields for a session control command. */
typedef struct {
    uint8_t declared_length; /**< Declared response payload length reported by the device. */
    uint8_t status;          /**< Device status byte. */
    const uint8_t
        *payload; /**< Pointer to response payload bytes, or null when none are present. */
    uint8_t payload_length; /**< Number of response payload bytes. */
} A71chSessionResponse;

/** @brief State machine for initializing the A71CH session. */
typedef struct {
    A71chCommand command;       /**< Command currently expected to complete. */
    uint8_t completed_attempts; /**< Number of completed attempts at the current stage. */
    uint32_t retry_after_ms;    /**< Time after which a delayed retry may begin. */
    bool waiting;               /**< Whether the current command is delayed for retry. */
    bool complete;              /**< Whether session initialization completed successfully. */
} A71chSession;

/**
 * @brief Initializes status-poll retry state.
 *
 * Clears the count of unexpected status responses for a new polling sequence.
 *
 * @param[out] poll Status-poll state to initialize.
 */
void a71ch_status_poll_init(A71chStatusPoll *poll);

/**
 * @brief Evaluates one SCI2C status response.
 *
 * Updates the retry state and classifies the response as retryable, busy, accepted, or rejected.
 *
 * @param[in,out] poll Status-poll retry state.
 * @param[in] response Device status byte.
 * @return Response disposition; unexpected statuses become rejected after the retry limit.
 */
A71chStatusResult a71ch_status_poll_evaluate(A71chStatusPoll *poll, uint8_t response);

/**
 * @brief Evaluates the status returned after an authentication command.
 *
 * Classifies the status without maintaining retry state because command acceptance is terminal for
 * this polling step.
 *
 * @param[in] response Device status byte.
 * @return Accepted, busy, or rejected disposition.
 */
A71chStatusResult a71ch_command_response_evaluate(uint8_t response);

/**
 * @brief Calculates the SCI2C longitudinal redundancy check.
 *
 * XORs the requested payload bytes and returns the resulting check byte.
 *
 * @param[in] payload Bytes covered by the check.
 * @param[in] payload_length Number of bytes to include.
 * @return XOR of the payload bytes, or zero when payload_length is zero.
 */
uint8_t a71ch_lrc(const uint8_t *payload, uint8_t payload_length);

/**
 * @brief Encodes a fixed A71CH control request.
 *
 * Copies the selector and expected response length for a session control command into request.
 *
 * @param[in] command Session control command.
 * @param[out] request Destination for encoded request metadata.
 * @return True when command is a fixed control command and request is populated; otherwise false.
 */
bool a71ch_control_request_encode(A71chCommand command, A71chControlRequest *request);

/**
 * @brief Looks up fixed A71CH control-request metadata.
 *
 * Returns immutable selector and response-length metadata for the five session control commands.
 *
 * @param[in] command Session control command.
 * @return Control-request metadata, or null when command is not a fixed control command.
 */
const A71chControlRequest *a71ch_control_request_lookup(A71chCommand command);

/**
 * @brief Encodes one authentication APDU frame.
 *
 * Builds the selected write, read, or finalization command and records the response layout needed
 * for parsing the corresponding secure-element response.
 *
 * @param[in] command Authentication command variant.
 * @param[in] input Fragment phase, index, pointer, and length.
 * @param[out] frame Encoded command and response-layout metadata.
 * @return True when command and input describe a valid frame; otherwise false.
 */
bool a71ch_authentication_encode(A71chCommand command, const A71chAuthenticationInput *input,
                                 A71chAuthenticationFrame *frame);

/**
 * @brief Parses and validates an authentication response frame.
 *
 * Checks the expected response length and optional LRC, then exposes the response payload through
 * parsed_response without copying it.
 *
 * @param[in] frame Expected response layout from the encoded command.
 * @param[in] response Received response bytes.
 * @param[in] response_length Number of received bytes.
 * @param[out] parsed_response View of the validated payload, or an empty view on failure.
 * @return Validation result distinguishing valid, LRC-error, and malformed responses.
 */
A71chFrameValidation
a71ch_authentication_response_parse(const A71chAuthenticationFrame *frame, const uint8_t *response,
                                    uint8_t response_length,
                                    A71chAuthenticationResponse *parsed_response);

/**
 * @brief Initializes the authentication fragment sequence.
 *
 * Selects the first upload fragment and records whether the LRC command family is in use.
 *
 * @param[out] sequence Sequence state to initialize.
 * @param[in] use_lrc True to use LRC command variants; false to use plain variants.
 */
void a71ch_authentication_sequence_init(A71chAuthenticationSequence *sequence, bool use_lrc);

/**
 * @brief Describes the next authentication fragment step.
 *
 * Reports the command, buffer offset, phase, index, and length until the complete sequence has been
 * accepted.
 *
 * @param[in] sequence Current sequence state.
 * @param[out] step Destination for the next step description.
 * @return True when a step remains and step is populated; otherwise false.
 */
bool a71ch_authentication_sequence_current(const A71chAuthenticationSequence *sequence,
                                           A71chAuthenticationStep *step);

/**
 * @brief Accepts the current authentication fragment step.
 *
 * Advances the phase and fragment state and transitions through writing, reading, finalization,
 * and completion.
 *
 * @param[in,out] sequence Sequence state to advance.
 * @return True when a non-complete step was accepted; otherwise false.
 */
bool a71ch_authentication_sequence_accept(A71chAuthenticationSequence *sequence);

/**
 * @brief Initializes one asynchronous authentication exchange.
 *
 * Sets the exchange to its initial readiness-poll stage with a pending result.
 *
 * @param[out] exchange Exchange state to initialize.
 */
void a71ch_exchange_init(A71chExchange *exchange);

/**
 * @brief Applies a status byte to an authentication exchange.
 *
 * Advances or fails the exchange when it is waiting for readiness or command acceptance.
 *
 * @param[in,out] exchange Exchange state to update.
 * @param[in] response Device status byte.
 * @return True when exchange was waiting for this status; otherwise false.
 */
bool a71ch_exchange_status(A71chExchange *exchange, uint8_t response);

/**
 * @brief Records successful submission of an authentication command.
 *
 * Moves an exchange from command-queue stage to command-acceptance polling.
 *
 * @param[in,out] exchange Exchange state to update.
 * @return True when a command was queued; otherwise false.
 */
bool a71ch_exchange_command_queued(A71chExchange *exchange);

/**
 * @brief Finalizes an authentication exchange with response validation.
 *
 * Converts the validation result into a terminal exchange stage and result.
 *
 * @param[in,out] exchange Exchange state to update.
 * @param[in] validation Parsed response validation result.
 * @return True when exchange was waiting for its response; otherwise false.
 */
bool a71ch_exchange_finalize(A71chExchange *exchange, A71chFrameValidation validation);

/**
 * @brief Initializes the A71CH session initialization sequence.
 *
 * Selects wake-up as the first command and clears attempt, delay, and completion state.
 *
 * @param[out] session Session state to initialize.
 */
void a71ch_session_init(A71chSession *session);

/**
 * @brief Gets the next session initialization command when ready.
 *
 * Honors delayed retries and withholds a command after successful completion.
 *
 * @param[in,out] session Session state whose retry delay may expire.
 * @param[in] now_ms Current monotonic time in milliseconds.
 * @param[out] command Destination for the command to execute.
 * @return True when command is ready and command is populated; otherwise false.
 */
bool a71ch_session_current(A71chSession *session, uint32_t now_ms, A71chCommand *command);

/**
 * @brief Accepts a completed session initialization command.
 *
 * Validates the response for the current command, advances the sequence, and schedules retries or
 * a restart when the secure element does not respond as expected.
 *
 * @param[in,out] session Session state to update.
 * @param[in] command Command whose transaction completed.
 * @param[in] response Parsed response, or null only for wake-up.
 * @param[in] now_ms Current monotonic time in milliseconds.
 * @return True when the completion belongs to the current session state; otherwise false.
 */
bool a71ch_session_accept(A71chSession *session, A71chCommand command,
                          const A71chSessionResponse *response, uint32_t now_ms);

#endif
