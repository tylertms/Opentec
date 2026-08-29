#ifndef OPENTEC_BASE_A71CH_H
#define OPENTEC_BASE_A71CH_H

#include <stdbool.h>
#include <stdint.h>

typedef enum {
    A71CH_STATUS_RETRY,
    A71CH_STATUS_BUSY,
    A71CH_STATUS_ACCEPTED,
    A71CH_STATUS_REJECTED,
} A71chStatusResult;

typedef enum {
    A71CH_VALID,
    A71CH_LRC_ERROR,
    A71CH_MALFORMED_RESPONSE,
} A71chFrameValidation;

typedef struct {
    uint8_t retry_count;
} A71chStatusPoll;

typedef enum {
    A71CH_WAKE_UP = 1,
    A71CH_SOFT_RESET = 2,
    A71CH_READ_ANSWER_TO_RESET = 3,
    A71CH_PARAMETER_EXCHANGE = 4,
    A71CH_READ_STATUS = 5,
    A71CH_AUTHENTICATION_WRITE,
    A71CH_AUTHENTICATION_WRITE_LRC,
    A71CH_AUTHENTICATION_READ,
    A71CH_AUTHENTICATION_READ_LRC,
    A71CH_AUTHENTICATION_FINALIZE,
    A71CH_AUTHENTICATION_FINALIZE_LRC,
} A71chCommand;

typedef struct {
    uint8_t selector;
    uint8_t response_length;
} A71chControlRequest;

enum {
    A71CH_AUTHENTICATION_CHUNK_CAPACITY = 64,
    A71CH_AUTHENTICATION_WRITE_CAPACITY = A71CH_AUTHENTICATION_CHUNK_CAPACITY + 8,
    A71CH_AUTHENTICATION_WRITE_SIZE = 0x100,
    A71CH_AUTHENTICATION_READ_SIZE = 0x410,
};

typedef struct {
    uint8_t phase;
    uint8_t chunk_index;
    const uint8_t *chunk;
    uint8_t chunk_length;
} A71chAuthenticationInput;

typedef struct {
    uint8_t selector;
    uint8_t write_data[A71CH_AUTHENTICATION_WRITE_CAPACITY];
    uint8_t write_length;
    uint8_t response_length;
    uint8_t response_payload_offset;
    uint8_t response_payload_length;
    uint8_t response_integrity_offset;
    uint8_t response_integrity_length;
} A71chAuthenticationFrame;

typedef struct {
    const uint8_t *payload;
    uint8_t payload_length;
} A71chAuthenticationResponse;

typedef enum {
    A71CH_AUTHENTICATION_WRITING,
    A71CH_AUTHENTICATION_READING,
    A71CH_AUTHENTICATION_FINISHING,
    A71CH_AUTHENTICATION_COMPLETE,
} A71chAuthenticationStage;

typedef struct {
    A71chAuthenticationStage stage;
    uint8_t phase;
    uint8_t chunk_index;
    bool use_lrc;
} A71chAuthenticationSequence;

typedef struct {
    A71chCommand command;
    uint16_t buffer_offset;
    uint8_t phase;
    uint8_t chunk_index;
    uint8_t chunk_length;
} A71chAuthenticationStep;

typedef enum {
    A71CH_EXCHANGE_WAIT_READY,
    A71CH_EXCHANGE_QUEUE_COMMAND,
    A71CH_EXCHANGE_WAIT_ACCEPTANCE,
    A71CH_EXCHANGE_WAIT_RESPONSE,
    A71CH_EXCHANGE_COMPLETE,
    A71CH_EXCHANGE_FAILED,
} A71chExchangeStage;

typedef enum {
    A71CH_EXCHANGE_PENDING,
    A71CH_EXCHANGE_SUCCEEDED,
    A71CH_EXCHANGE_COMMAND_ERROR,
    A71CH_EXCHANGE_LRC_ERROR,
    A71CH_EXCHANGE_RESPONSE_ERROR,
} A71chExchangeResult;

typedef struct {
    A71chExchangeStage stage;
    A71chExchangeResult result;
    A71chStatusPoll readiness;
} A71chExchange;

typedef struct {
    uint8_t declared_length;
    uint8_t status;
    const uint8_t *payload;
    uint8_t payload_length;
} A71chSessionResponse;

typedef struct {
    A71chCommand command;
    uint8_t completed_attempts;
    uint32_t retry_after_ms;
    bool waiting;
    bool complete;
} A71chSession;

void a71ch_status_poll_init(A71chStatusPoll *poll);
A71chStatusResult a71ch_status_poll_evaluate(A71chStatusPoll *poll, uint8_t response);
A71chStatusResult a71ch_command_response_evaluate(uint8_t response);
uint8_t a71ch_lrc(const uint8_t *payload, uint8_t payload_length);
bool a71ch_control_request_encode(A71chCommand command, A71chControlRequest *request);
const A71chControlRequest *a71ch_control_request_lookup(A71chCommand command);
bool a71ch_authentication_encode(A71chCommand command, const A71chAuthenticationInput *input,
                                 A71chAuthenticationFrame *frame);
A71chFrameValidation
a71ch_authentication_response_parse(const A71chAuthenticationFrame *frame, const uint8_t *response,
                                    uint8_t response_length,
                                    A71chAuthenticationResponse *parsed_response);
void a71ch_authentication_sequence_init(A71chAuthenticationSequence *sequence, bool use_lrc);
bool a71ch_authentication_sequence_current(const A71chAuthenticationSequence *sequence,
                                           A71chAuthenticationStep *step);
bool a71ch_authentication_sequence_accept(A71chAuthenticationSequence *sequence);
void a71ch_exchange_init(A71chExchange *exchange);
bool a71ch_exchange_status(A71chExchange *exchange, uint8_t response);
bool a71ch_exchange_command_queued(A71chExchange *exchange);
bool a71ch_exchange_finalize(A71chExchange *exchange, A71chFrameValidation validation);
void a71ch_session_init(A71chSession *session);
bool a71ch_session_current(A71chSession *session, uint32_t now_ms, A71chCommand *command);
bool a71ch_session_accept(A71chSession *session, A71chCommand command,
                          const A71chSessionResponse *response, uint32_t now_ms);

#endif
