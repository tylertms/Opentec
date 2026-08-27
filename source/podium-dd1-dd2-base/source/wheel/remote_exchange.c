#include "wheel/remote_exchange.h"

#include <string.h>

static bool exchange_active(const wheel_remote_exchange *exchange) {
    return exchange->state == WHEEL_REMOTE_REQUEST_READY ||
           exchange->state == WHEEL_REMOTE_ACK_READY ||
           exchange->state == WHEEL_REMOTE_NACK_READY || exchange->state == WHEEL_REMOTE_WAITING;
}

static void fail(wheel_remote_exchange *exchange, wheel_remote_error error) {
    exchange->state = WHEEL_REMOTE_FAILED;
    exchange->error = error;
}

static uint8_t request_flags(const wheel_remote_exchange *exchange, size_t remaining) {
    if (exchange->request_length <= WQR_FRAME_PAYLOAD_SIZE) {
        return 0;
    }
    if (exchange->request_offset == 0) {
        return WQR_FRAME_FIRST;
    }
    return remaining > WQR_FRAME_PAYLOAD_SIZE ? WQR_FRAME_MORE : WQR_FRAME_LAST;
}

static bool build_request(wheel_remote_exchange *exchange, uint8_t frame[WQR_FRAME_SIZE]) {
    size_t remaining = exchange->request_length - exchange->request_offset;
    size_t length = remaining < WQR_FRAME_PAYLOAD_SIZE ? remaining : WQR_FRAME_PAYLOAD_SIZE;
    uint8_t type_flags = exchange->payload_type | request_flags(exchange, remaining);

    if (!wqr_frame_build(frame, type_flags, exchange->sequence,
                         exchange->request + exchange->request_offset, length)) {
        fail(exchange, WHEEL_REMOTE_ERROR_ARGUMENT);
        return false;
    }

    exchange->request_fragment_length = length;
    exchange->request_fragment_final = remaining <= WQR_FRAME_PAYLOAD_SIZE;
    exchange->wait =
        exchange->request_fragment_final ? WHEEL_REMOTE_WAIT_RESPONSE : WHEEL_REMOTE_WAIT_ACK;
    exchange->elapsed_ms = 0;
    exchange->state = WHEEL_REMOTE_WAITING;
    return true;
}

static bool build_control(wheel_remote_exchange *exchange, uint8_t frame[WQR_FRAME_SIZE],
                          uint8_t type) {
    uint8_t sequence = type == WQR_FRAME_ACK || exchange->response_open
                           ? exchange->response_sequence
                           : exchange->sequence;

    if (!wqr_frame_build(frame, type, sequence, NULL, 0)) {
        fail(exchange, WHEEL_REMOTE_ERROR_ARGUMENT);
        return false;
    }

    exchange->elapsed_ms = 0;
    if (type == WQR_FRAME_ACK && exchange->ack_completes) {
        exchange->state = WHEEL_REMOTE_COMPLETE;
    } else {
        exchange->wait = WHEEL_REMOTE_WAIT_RESPONSE;
        if (type == WQR_FRAME_ACK) {
            exchange->sequence = exchange->response_sequence;
        }
        exchange->state = WHEEL_REMOTE_WAITING;
    }
    return true;
}

static bool retry(wheel_remote_exchange *exchange, wheel_remote_error exhausted_error) {
    if (exchange->retries == exchange->retry_limit) {
        fail(exchange, exhausted_error);
        return false;
    }

    ++exchange->retries;
    exchange->elapsed_ms = 0;
    exchange->state = exchange->wait == WHEEL_REMOTE_WAIT_ACK ? WHEEL_REMOTE_REQUEST_READY
                                                              : WHEEL_REMOTE_NACK_READY;
    return true;
}

static bool accept_request_ack(wheel_remote_exchange *exchange, const wqr_frame_view *view) {
    if ((view->type_flags & WQR_FRAME_FRAGMENT_MASK) != 0 ||
        (view->type_flags & WQR_FRAME_TYPE_MASK) != WQR_FRAME_ACK ||
        view->sequence != (uint8_t)(exchange->sequence + 1) || view->payload_length != 1 ||
        view->payload[0] != exchange->payload_type) {
        return retry(exchange, WHEEL_REMOTE_ERROR_PROTOCOL);
    }

    exchange->request_offset += exchange->request_fragment_length;
    exchange->sequence = view->sequence;
    exchange->retries = 0;
    exchange->state = WHEEL_REMOTE_REQUEST_READY;
    return true;
}

static bool response_fragment_valid(const wheel_remote_exchange *exchange,
                                    const wqr_frame_view *view) {
    uint8_t type = view->type_flags & WQR_FRAME_TYPE_MASK;
    uint8_t fragments = view->type_flags & WQR_FRAME_FRAGMENT_MASK;

    if (type != exchange->payload_type || view->sequence != (uint8_t)(exchange->sequence + 1)) {
        return false;
    }
    if (exchange->response_open) {
        return fragments == WQR_FRAME_MORE || fragments == WQR_FRAME_LAST;
    }
    return fragments == 0 || fragments == WQR_FRAME_FIRST;
}

static bool accept_response(wheel_remote_exchange *exchange, const wqr_frame_view *view) {
    uint8_t fragments;

    if (!response_fragment_valid(exchange, view)) {
        return retry(exchange, WHEEL_REMOTE_ERROR_PROTOCOL);
    }
    if (view->payload_length > WHEEL_REMOTE_EXCHANGE_CAPACITY - exchange->response_length) {
        fail(exchange, WHEEL_REMOTE_ERROR_OVERFLOW);
        return false;
    }

    memcpy(exchange->response + exchange->response_length, view->payload, view->payload_length);
    exchange->response_length += view->payload_length;
    exchange->response_sequence = view->sequence;
    exchange->retries = 0;
    fragments = view->type_flags & WQR_FRAME_FRAGMENT_MASK;
    exchange->ack_completes = fragments == 0 || fragments == WQR_FRAME_LAST;
    exchange->response_open = !exchange->ack_completes;
    exchange->state = WHEEL_REMOTE_ACK_READY;
    return true;
}

void wheel_remote_exchange_init(wheel_remote_exchange *exchange, uint32_t timeout_ms,
                                uint8_t retry_limit) {
    memset(exchange, 0, sizeof(*exchange));
    exchange->timeout_ms = timeout_ms;
    exchange->retry_limit = retry_limit;
}

bool wheel_remote_exchange_start(wheel_remote_exchange *exchange, uint8_t payload_type,
                                 uint8_t initial_sequence, const uint8_t *request,
                                 size_t request_length) {
    if (exchange_active(exchange)) {
        exchange->error = WHEEL_REMOTE_ERROR_BUSY;
        return false;
    }
    if (payload_type < WQR_PAYLOAD_PRIMARY_SPI || payload_type > WQR_PAYLOAD_STATUS ||
        request_length > WHEEL_REMOTE_EXCHANGE_CAPACITY ||
        (request == NULL && request_length != 0)) {
        exchange->error = WHEEL_REMOTE_ERROR_ARGUMENT;
        return false;
    }

    if (request_length != 0) {
        memcpy(exchange->request, request, request_length);
    }
    exchange->request_length = request_length;
    exchange->request_offset = 0;
    exchange->request_fragment_length = 0;
    exchange->response_length = 0;
    exchange->elapsed_ms = 0;
    exchange->payload_type = payload_type;
    exchange->sequence = initial_sequence;
    exchange->response_sequence = initial_sequence;
    exchange->retries = 0;
    exchange->error = WHEEL_REMOTE_ERROR_NONE;
    exchange->wait = WHEEL_REMOTE_WAIT_RESPONSE;
    exchange->request_fragment_final = false;
    exchange->response_open = false;
    exchange->ack_completes = false;
    exchange->state = WHEEL_REMOTE_REQUEST_READY;
    return true;
}

bool wheel_remote_exchange_next_frame(wheel_remote_exchange *exchange,
                                      uint8_t frame[WQR_FRAME_SIZE]) {
    if (exchange->state == WHEEL_REMOTE_REQUEST_READY) {
        return build_request(exchange, frame);
    }
    if (exchange->state == WHEEL_REMOTE_ACK_READY) {
        return build_control(exchange, frame, WQR_FRAME_ACK);
    }
    if (exchange->state == WHEEL_REMOTE_NACK_READY) {
        return build_control(exchange, frame, WQR_FRAME_NACK);
    }
    return false;
}

bool wheel_remote_exchange_receive(wheel_remote_exchange *exchange,
                                   const uint8_t frame[WQR_FRAME_SIZE]) {
    if (exchange->state != WHEEL_REMOTE_WAITING) {
        return false;
    }
    if (!wqr_frame_parse(frame, &exchange->incoming)) {
        return retry(exchange, WHEEL_REMOTE_ERROR_PROTOCOL);
    }
    if (exchange->wait == WHEEL_REMOTE_WAIT_ACK) {
        return accept_request_ack(exchange, &exchange->incoming);
    }
    if ((exchange->incoming.type_flags & WQR_FRAME_TYPE_MASK) == WQR_FRAME_NACK) {
        return retry(exchange, WHEEL_REMOTE_ERROR_PROTOCOL);
    }
    return accept_response(exchange, &exchange->incoming);
}

void wheel_remote_exchange_tick(wheel_remote_exchange *exchange, uint32_t elapsed_ms) {
    if (exchange->state != WHEEL_REMOTE_WAITING || exchange->timeout_ms == 0) {
        return;
    }
    if (elapsed_ms >= exchange->timeout_ms - exchange->elapsed_ms) {
        exchange->elapsed_ms = exchange->timeout_ms;
        retry(exchange, WHEEL_REMOTE_ERROR_TIMEOUT);
        return;
    }
    exchange->elapsed_ms += elapsed_ms;
}

const uint8_t *wheel_remote_exchange_response(const wheel_remote_exchange *exchange,
                                              size_t *response_length) {
    if (exchange->state != WHEEL_REMOTE_COMPLETE || response_length == NULL) {
        return NULL;
    }

    *response_length = exchange->response_length;
    return exchange->response;
}
