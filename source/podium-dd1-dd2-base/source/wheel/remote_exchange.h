#ifndef OPENTEC_BASE_WHEEL_REMOTE_EXCHANGE_H
#define OPENTEC_BASE_WHEEL_REMOTE_EXCHANGE_H

#include <common/wqr_frame.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

enum { WHEEL_REMOTE_EXCHANGE_CAPACITY = 512 };

typedef enum {
    WHEEL_REMOTE_IDLE,
    WHEEL_REMOTE_REQUEST_READY,
    WHEEL_REMOTE_ACK_READY,
    WHEEL_REMOTE_NACK_READY,
    WHEEL_REMOTE_WAITING,
    WHEEL_REMOTE_COMPLETE,
    WHEEL_REMOTE_FAILED
} wheel_remote_state;

typedef enum {
    WHEEL_REMOTE_ERROR_NONE,
    WHEEL_REMOTE_ERROR_BUSY,
    WHEEL_REMOTE_ERROR_ARGUMENT,
    WHEEL_REMOTE_ERROR_OVERFLOW,
    WHEEL_REMOTE_ERROR_PROTOCOL,
    WHEEL_REMOTE_ERROR_TIMEOUT
} wheel_remote_error;

typedef enum { WHEEL_REMOTE_WAIT_ACK, WHEEL_REMOTE_WAIT_RESPONSE } wheel_remote_wait;

typedef struct {
    uint8_t request[WHEEL_REMOTE_EXCHANGE_CAPACITY];
    uint8_t response[WHEEL_REMOTE_EXCHANGE_CAPACITY];
    wqr_frame_view incoming;
    size_t request_length;
    size_t request_offset;
    size_t request_fragment_length;
    size_t response_length;
    uint32_t elapsed_ms;
    uint32_t timeout_ms;
    uint8_t payload_type;
    uint8_t sequence;
    uint8_t response_sequence;
    uint8_t retries;
    uint8_t retry_limit;
    wheel_remote_state state;
    wheel_remote_error error;
    wheel_remote_wait wait;
    bool request_fragment_final;
    bool response_open;
    bool ack_completes;
} wheel_remote_exchange;

void wheel_remote_exchange_init(wheel_remote_exchange *exchange, uint32_t timeout_ms,
                                uint8_t retry_limit);
bool wheel_remote_exchange_start(wheel_remote_exchange *exchange, uint8_t payload_type,
                                 uint8_t initial_sequence, const uint8_t *request,
                                 size_t request_length);
bool wheel_remote_exchange_next_frame(wheel_remote_exchange *exchange,
                                      uint8_t frame[WQR_FRAME_SIZE]);
bool wheel_remote_exchange_receive(wheel_remote_exchange *exchange,
                                   const uint8_t frame[WQR_FRAME_SIZE]);
void wheel_remote_exchange_tick(wheel_remote_exchange *exchange, uint32_t elapsed_ms);
const uint8_t *wheel_remote_exchange_response(const wheel_remote_exchange *exchange,
                                              size_t *response_length);

#endif
