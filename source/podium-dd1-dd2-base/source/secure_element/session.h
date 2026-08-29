#ifndef OPENTEC_BASE_A71CH_SESSION_SERVICE_H
#define OPENTEC_BASE_A71CH_SESSION_SERVICE_H

#include <stdbool.h>
#include <stdint.h>

#include "secure_element/a71ch.h"

typedef enum {
    A71CH_SESSION_SERVICE_IDLE,
    A71CH_SESSION_SERVICE_RUNNING,
    A71CH_SESSION_SERVICE_COMPLETE,
} A71chSessionServiceStatus;

enum {
    A71CH_SESSION_RESPONSE_CAPACITY = 0x1f,
};

typedef struct {
    A71chSession session;
    A71chCommand active_command;
    uint8_t response[A71CH_SESSION_RESPONSE_CAPACITY];
    A71chSessionResponse response_view;
    A71chSessionServiceStatus status;
    bool transfer_active;
} A71chSessionService;

void a71ch_session_service_init(A71chSessionService *service);
void a71ch_session_service_start(A71chSessionService *service);
void a71ch_session_service_run(A71chSessionService *service, uint32_t now_ms);
A71chSessionServiceStatus a71ch_session_service_status(const A71chSessionService *service);

#endif
