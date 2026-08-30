#ifndef PODIUM_DD1_DD2_BASE_TRANSFER_SESSION_H
#define PODIUM_DD1_DD2_BASE_TRANSFER_SESSION_H

#include <stdbool.h>
#include <stdint.h>

#include "transfer/frame.h"

enum {
    TRANSFER_SESSION_TIMEOUT_MS = 200,
};

typedef void (*TransferSessionSend)(void *context, const uint8_t *data, uint16_t length);
typedef bool (*TransferSessionReady)(void *context);
typedef void (*TransferSessionData)(void *context, const uint8_t *data, uint8_t length,
                                    uint8_t group, bool complete);
typedef uint32_t (*TransferSessionClock)(void *context);

typedef struct {
    TransferSessionSend send;
    TransferSessionReady ready;
    TransferSessionData data;
    TransferSessionClock clock;
} TransferSessionCallbacks;

typedef enum {
    TRANSFER_SESSION_OK,
    TRANSFER_SESSION_BUSY,
    TRANSFER_SESSION_INACTIVE,
    TRANSFER_SESSION_INVALID_FRAME,
    TRANSFER_SESSION_SEQUENCE_ERROR,
    TRANSFER_SESSION_REMOTE_ERROR,
    TRANSFER_SESSION_TIMED_OUT,
} TransferSessionResult;

typedef struct {
    TransferSessionCallbacks callbacks;
    void *callback_context;
    TransferFrame receive_frame;
    TransferFrame pending_frame;
    uint8_t encoded[TRANSFER_FRAME_MAX_ENCODED_SIZE];
    uint32_t activity_deadline;
    uint32_t data_deadline;
    uint8_t receive_group;
    uint8_t receive_parameter;
    uint8_t outbound_parameter;
    uint8_t error_count;
    uint8_t remote_error_count;
    bool active;
    bool receive_active;
    bool outbound_pending;
} TransferSession;

bool transfer_session_init(TransferSession *session, const TransferSessionCallbacks *callbacks,
                           void *callback_context);
bool transfer_session_send(TransferSession *session, const uint8_t *data, uint8_t length,
                           uint8_t group);
bool transfer_session_keepalive(TransferSession *session);
TransferSessionResult transfer_session_receive(TransferSession *session, const uint8_t *data,
                                               uint16_t length);
TransferSessionResult transfer_session_poll(TransferSession *session);

#endif
