#include "transfer/session.h"

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

enum {
    TRANSFER_COMMAND_EMPTY = 0,
    TRANSFER_COMMAND_DATA = 1,
    TRANSFER_COMMAND_STATUS = 2,
    TRANSFER_SEQUENCE_SINGLE = 0,
    TRANSFER_SEQUENCE_START = 1,
    TRANSFER_SEQUENCE_CONTINUE = 2,
    TRANSFER_SEQUENCE_END = 3,
    TRANSFER_ERROR_CHECKSUM = 0,
    TRANSFER_ERROR_SEQUENCE = 1,
    TRANSFER_ERROR_FORMAT = 2,
    TRANSFER_MAX_ERRORS = 4,
};

static bool send_frame(TransferSession *session, uint16_t command, const uint8_t *payload,
                       uint8_t payload_length, bool remember) {
    uint16_t length =
        transfer_frame_encode_values(command, payload, payload_length, session->encoded);
    if (length == 0 || session->callbacks.ready(session->callback_context)) {
        return false;
    }

    if (remember) {
        session->pending_frame.command = command;
        session->pending_frame.payload_length = payload_length;
        if (payload_length != 0) {
            memcpy(session->pending_frame.payload, payload, payload_length);
        }
    }
    session->callbacks.send(session->callback_context, session->encoded, length);
    return true;
}

static bool send_status(TransferSession *session, uint8_t progress) {
    uint16_t command =
        progress == 0 ? transfer_status_command(session->receive_group, session->receive_parameter)
                      : transfer_progress_command(session->receive_group,
                                                  session->receive_parameter, progress);
    return send_frame(session, command, NULL, 0, false);
}

static TransferSessionResult record_error(TransferSession *session, uint8_t reason,
                                          TransferSessionResult result) {
    uint8_t previous_count = session->error_count++;
    if (previous_count > TRANSFER_MAX_ERRORS - 1) {
        send_status(session, reason);
        session->active = false;
    }
    return result;
}

static int8_t advance_receive_sequence(TransferSession *session, uint8_t sequence,
                                       uint8_t parameter) {
    if (sequence == TRANSFER_SEQUENCE_SINGLE) {
        session->receive_parameter = parameter;
        return 1;
    }
    if (sequence == TRANSFER_SEQUENCE_START) {
        if (session->receive_active) {
            return -1;
        }
        session->receive_active = true;
        session->receive_parameter = parameter;
        return 0;
    }
    if (sequence != TRANSFER_SEQUENCE_CONTINUE && sequence != TRANSFER_SEQUENCE_END) {
        return -1;
    }
    if (!session->receive_active || parameter != (uint8_t)((session->receive_parameter + 1) & 7)) {
        return -1;
    }

    session->receive_parameter = parameter;
    if (sequence == TRANSFER_SEQUENCE_END) {
        session->receive_active = false;
        return 1;
    }
    return 0;
}

static TransferSessionResult receive_data(TransferSession *session, const TransferFrame *frame) {
    session->receive_group = transfer_command_group(frame->command);
    int8_t sequence_state =
        advance_receive_sequence(session, transfer_command_sequence(frame->command),
                                 transfer_command_parameter(frame->command));
    if (sequence_state < 0) {
        return record_error(session, TRANSFER_ERROR_SEQUENCE, TRANSFER_SESSION_SEQUENCE_ERROR);
    }

    bool acknowledged = send_status(session, 0);
    session->callbacks.data(session->callback_context, frame->payload, frame->payload_length,
                            session->receive_group, sequence_state != 0);
    return acknowledged ? TRANSFER_SESSION_OK : TRANSFER_SESSION_BUSY;
}

static TransferSessionResult receive_status(TransferSession *session, uint16_t command) {
    session->receive_group = transfer_command_group(command);
    uint8_t parameter = transfer_command_parameter(command);
    uint8_t progress = transfer_command_progress(command);

    if (progress == 0) {
        if (session->outbound_pending && parameter == session->outbound_parameter) {
            session->outbound_pending = false;
            session->remote_error_count = 0;
            return TRANSFER_SESSION_OK;
        }
        return record_error(session, TRANSFER_ERROR_SEQUENCE, TRANSFER_SESSION_SEQUENCE_ERROR);
    }

    session->remote_error_count = (session->remote_error_count + 1) & 3;
    if (session->remote_error_count <= 2) {
        session->active = false;
        return TRANSFER_SESSION_REMOTE_ERROR;
    }
    if (session->outbound_pending) {
        send_frame(session, session->pending_frame.command, session->pending_frame.payload,
                   session->pending_frame.payload_length, false);
    }
    return TRANSFER_SESSION_OK;
}

/**
 * @brief Starts a transfer session with transport, data, and clock callbacks.
 * @param session Session state to initialize.
 * @param callbacks Required send, ready, data, and clock callbacks.
 * @param callback_context Opaque value passed to every callback.
 * @return True when all callbacks are present and the session starts.
 */
bool transfer_session_init(TransferSession *session, const TransferSessionCallbacks *callbacks,
                           void *callback_context) {
    if (session == NULL || callbacks == NULL || callbacks->send == NULL ||
        callbacks->ready == NULL || callbacks->data == NULL || callbacks->clock == NULL) {
        return false;
    }

    memset(session, 0, sizeof(*session));
    session->callbacks = *callbacks;
    session->callback_context = callback_context;
    session->active = true;
    uint32_t now = callbacks->clock(callback_context);
    session->activity_deadline = now + TRANSFER_SESSION_TIMEOUT_MS;
    session->data_deadline = now + TRANSFER_SESSION_TIMEOUT_MS;
    return true;
}

/**
 * @brief Sends one complete transfer data message and waits for its status response.
 * @param session Active transfer session.
 * @param data Payload bytes.
 * @param length Payload length from zero through 124 bytes.
 * @param group Two-bit transfer group.
 * @return True when the frame is accepted by the transport.
 */
bool transfer_session_send(TransferSession *session, const uint8_t *data, uint8_t length,
                           uint8_t group) {
    if (session == NULL || !session->active || session->outbound_pending ||
        length > TRANSFER_FRAME_MAX_SEND_PAYLOAD_SIZE || (length != 0 && data == NULL)) {
        return false;
    }

    uint16_t command = transfer_data_command(group, TRANSFER_SEQUENCE_SINGLE, 0);
    if (!send_frame(session, command, data, length, true)) {
        return false;
    }

    session->outbound_parameter = transfer_command_parameter(command);
    session->outbound_pending = true;
    return true;
}

/**
 * @brief Processes one complete encoded frame and advances transfer state.
 * @param session Active transfer session.
 * @param data Encoded frame including boundary markers.
 * @param length Encoded frame length.
 * @return Processing result for delivery, sequencing, remote errors, or invalid data.
 */
TransferSessionResult transfer_session_receive(TransferSession *session, const uint8_t *data,
                                               uint16_t length) {
    if (session == NULL || !session->active) {
        return TRANSFER_SESSION_INACTIVE;
    }

    TransferFrameResult decoded = transfer_frame_decode(data, length, &session->receive_frame);
    TransferSessionResult result;
    if (decoded != TRANSFER_FRAME_VALID) {
        uint8_t reason = decoded == TRANSFER_FRAME_INVALID_CHECKSUM ? TRANSFER_ERROR_CHECKSUM
                                                                    : TRANSFER_ERROR_FORMAT;
        result = record_error(session, reason, TRANSFER_SESSION_INVALID_FRAME);
    } else {
        session->error_count = 0;
        uint8_t type = transfer_command_type(session->receive_frame.command);
        if (type == TRANSFER_COMMAND_DATA) {
            result = receive_data(session, &session->receive_frame);
        } else if (type == TRANSFER_COMMAND_STATUS) {
            result = receive_status(session, session->receive_frame.command);
        } else if (type == TRANSFER_COMMAND_EMPTY && session->receive_frame.payload_length == 0) {
            result = TRANSFER_SESSION_OK;
        } else {
            session->callbacks.data(session->callback_context, session->receive_frame.payload,
                                    session->receive_frame.payload_length,
                                    transfer_command_group(session->receive_frame.command), true);
            result = TRANSFER_SESSION_OK;
        }
    }

    if (session->active) {
        uint32_t now = session->callbacks.clock(session->callback_context);
        session->activity_deadline = now + TRANSFER_SESSION_TIMEOUT_MS;
        if (length > 5) {
            session->data_deadline = now + TRANSFER_SESSION_TIMEOUT_MS;
        }
    }
    return result;
}

/**
 * @brief Checks the transfer activity and data deadlines.
 * @param session Transfer session to service.
 * @return Okay while active, timed out after either 200 ms deadline, or inactive.
 */
TransferSessionResult transfer_session_poll(TransferSession *session) {
    if (session == NULL || !session->active) {
        return TRANSFER_SESSION_INACTIVE;
    }

    uint32_t now = session->callbacks.clock(session->callback_context);
    if (now > session->data_deadline || now > session->activity_deadline) {
        session->active = false;
        return TRANSFER_SESSION_TIMED_OUT;
    }
    return TRANSFER_SESSION_OK;
}
