#ifndef OPENTEC_BASE_A71CH_AUTHENTICATION_SERVICE_H
#define OPENTEC_BASE_A71CH_AUTHENTICATION_SERVICE_H

#include <stdbool.h>
#include <stdint.h>

#include "secure_element/a71ch.h"
#include "secure_element/exchange.h"

typedef enum {
    A71CH_AUTHENTICATION_SERVICE_IDLE,
    A71CH_AUTHENTICATION_SERVICE_RUNNING,
    A71CH_AUTHENTICATION_SERVICE_COMPLETE,
    A71CH_AUTHENTICATION_SERVICE_FAILED,
} A71chAuthenticationServiceStatus;

typedef struct {
    A71chAuthenticationSequence sequence;
    A71chExchangeService exchange;
    A71chAuthenticationStep current_step;
    A71chAuthenticationInput current_input;
    uint8_t request[A71CH_AUTHENTICATION_WRITE_SIZE];
    uint8_t response[A71CH_AUTHENTICATION_READ_SIZE];
    uint8_t current_payload_length;
    A71chAuthenticationServiceStatus status;
    A71chExchangeResult result;
} A71chAuthenticationService;

void a71ch_authentication_service_init(A71chAuthenticationService *service);
bool a71ch_authentication_service_start(A71chAuthenticationService *service, const uint8_t *request,
                                        uint16_t request_length, bool use_lrc);
void a71ch_authentication_service_run(A71chAuthenticationService *service);
A71chAuthenticationServiceStatus
a71ch_authentication_service_status(const A71chAuthenticationService *service);
A71chExchangeResult a71ch_authentication_service_result(const A71chAuthenticationService *service);
const uint8_t *a71ch_authentication_service_response(const A71chAuthenticationService *service);

#endif
