#ifndef OPENTEC_BASE_A71CH_EXCHANGE_SERVICE_H
#define OPENTEC_BASE_A71CH_EXCHANGE_SERVICE_H

#include <stdbool.h>
#include <stdint.h>

#include "secure_element/a71ch.h"

typedef enum {
    A71CH_EXCHANGE_SERVICE_IDLE,
    A71CH_EXCHANGE_SERVICE_RUNNING,
    A71CH_EXCHANGE_SERVICE_COMPLETE,
    A71CH_EXCHANGE_SERVICE_FAILED,
} A71chExchangeServiceStatus;

enum {
    A71CH_EXCHANGE_RESPONSE_CAPACITY = A71CH_AUTHENTICATION_CHUNK_CAPACITY + 4,
};

typedef struct {
    A71chExchange exchange;
    A71chAuthenticationFrame frame;
    uint8_t status_response[2];
    uint8_t response[A71CH_EXCHANGE_RESPONSE_CAPACITY];
    A71chAuthenticationResponse parsed_response;
    A71chExchangeServiceStatus status;
    bool transfer_active;
} A71chExchangeService;

void a71ch_exchange_service_init(A71chExchangeService *service);
bool a71ch_exchange_service_start(A71chExchangeService *service,
                                  const A71chAuthenticationFrame *frame);
void a71ch_exchange_service_run(A71chExchangeService *service);
const uint8_t *a71ch_exchange_service_payload(const A71chExchangeService *service, uint8_t *length);
A71chExchangeServiceStatus a71ch_exchange_service_status(const A71chExchangeService *service);
A71chExchangeResult a71ch_exchange_service_result(const A71chExchangeService *service);

#endif
