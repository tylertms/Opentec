#include "transfer/session.h"

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

/**
 * @brief Internal transfer-session command, sequence, error, and retry constants.
 *
 * These values define the session state machine's command classification and error handling.
 */
enum {
    TRANSFER_COMMAND_EMPTY = 0,     /**< Empty transfer command type. */
    TRANSFER_COMMAND_DATA = 1,      /**< Data transfer command type. */
    TRANSFER_COMMAND_STATUS = 2,    /**< Status or progress command type. */
    TRANSFER_SEQUENCE_SINGLE = 0,   /**< Single-frame data sequence value. */
    TRANSFER_SEQUENCE_START = 1,    /**< First frame of a segmented sequence. */
    TRANSFER_SEQUENCE_CONTINUE = 2, /**< Middle frame of a segmented sequence. */
    TRANSFER_SEQUENCE_END = 3,      /**< Final frame of a segmented sequence. */
    TRANSFER_ERROR_CHECKSUM = 0,    /**< Progress value for a checksum error. */
    TRANSFER_ERROR_SEQUENCE = 1,    /**< Progress value for a sequence error. */
    TRANSFER_ERROR_FORMAT = 2,      /**< Progress value for a frame-format error. */
    TRANSFER_MAX_ERRORS = 4,        /**< Number of local errors retained before deactivation. */
};

/**
 * @brief Submits one encoded frame when the lower transport is idle.
 *
 * Optionally retains the command and payload for a protocol retry. Encoding or lower-transport
 * rejection leaves the retained-frame state unchanged.
 *
 * @param[in,out] session Transfer session and retained-frame state.
 * @param[in] command Encoded transfer command fields.
 * @param[in] payload Optional frame payload.
 * @param[in] payload_length Payload length in bytes.
 * @param[in] remember True to retain the frame for a retry.
 * @return True when the encoded frame is submitted.
 */
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

/**
 * @brief Sends a status or progress response for the current inbound sequence.
 *
 * Uses progress zero for an acknowledgement and a nonzero progress value for an error response.
 * A busy transport retains the acknowledgement for remote-error recovery.
 *
 * @param[in,out] session Transfer session containing the inbound group and parameter.
 * @param[in] progress Zero for acknowledgement or the protocol error value.
 */
static void send_status(TransferSession *session, uint8_t progress) {
    uint16_t command =
        progress == 0 ? transfer_status_command(session->receive_group, session->receive_parameter)
                      : transfer_progress_command(session->receive_group,
                                                  session->receive_parameter, progress);
    if (!send_frame(session, command, NULL, 0, false)) {
        if (progress != 0) {
            return;
        }
        session->pending_acknowledgement = command;
        session->acknowledgement_pending = true;
        return;
    }
    if (progress == 0) {
        session->acknowledgement_pending = false;
    }
}

/**
 * @brief Retries a retained inbound acknowledgement.
 *
 * The lower transport can reject an inbound acknowledgement. The latest acknowledgement command
 * remains available for the protocol's remote-error retry path.
 *
 * @param[in,out] session Transfer session containing the retained acknowledgement.
 */
static void retry_pending_acknowledgement(TransferSession *session) {
    if (!session->acknowledgement_pending ||
        !send_frame(session, session->pending_acknowledgement, NULL, 0, false)) {
        return;
    }
    session->acknowledgement_pending = false;
}

/**
 * @brief Records a local frame error and stops after the fifth consecutive error.
 *
 * Attempts to send the protocol progress response that identifies the final error before
 * disabling the session.
 *
 * @param[in,out] session Transfer session and consecutive-error counter.
 * @param[in] reason Protocol progress value describing the error.
 * @param[in] result Result returned for this frame.
 * @return The supplied frame result.
 */
static TransferSessionResult record_error(TransferSession *session, uint8_t reason,
                                          TransferSessionResult result) {
    uint8_t previous_count = session->error_count++;
    if (previous_count > TRANSFER_MAX_ERRORS - 1) {
        send_status(session, reason);
        session->active = false;
    }
    return result;
}

/**
 * @brief Advances the inbound transfer sequence.
 *
 * Single frames finish immediately and cancel an earlier segmented receive. Segmented parameters
 * advance from seven to one, and reserved sequence values remain incomplete without changing the
 * active sequence.
 *
 * @param[in,out] session Inbound sequence state to update.
 * @param[in] sequence Three-bit transfer sequence value.
 * @param[in] parameter Three-bit frame parameter.
 * @return One for a completed message, zero for an incomplete message, or negative one for an
 * invalid transition.
 */
static int8_t advance_receive_sequence(TransferSession *session, uint8_t sequence,
                                       uint8_t parameter) {
    if (sequence == TRANSFER_SEQUENCE_SINGLE) {
        session->receive_active = false;
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
        return 0;
    }
    uint8_t expected_parameter =
        session->receive_parameter == 7 ? 1 : (uint8_t)(session->receive_parameter + 1);
    if (!session->receive_active || parameter != expected_parameter) {
        return -1;
    }

    session->receive_parameter = parameter;
    if (sequence == TRANSFER_SEQUENCE_END) {
        session->receive_active = false;
        return 1;
    }
    return 0;
}

/**
 * @brief Handles one inbound data frame.
 *
 * Validates its sequence transition, acknowledges the current parameter, and delivers the payload
 * with the message-completion state. A busy lower transport retains the acknowledgement for the
 * protocol's remote-error retry path without changing the receive result.
 *
 * @param[in,out] session Transfer session receiving the frame.
 * @param[in] frame Decoded data frame.
 * @return Okay after delivery, or a sequence error.
 */
static TransferSessionResult receive_data(TransferSession *session, const TransferFrame *frame) {
    session->receive_group = transfer_command_group(frame->command);
    int8_t sequence_state =
        advance_receive_sequence(session, transfer_command_sequence(frame->command),
                                 transfer_command_parameter(frame->command));
    if (sequence_state < 0) {
        return record_error(session, TRANSFER_ERROR_SEQUENCE, TRANSFER_SESSION_SEQUENCE_ERROR);
    }

    send_status(session, 0);
    session->callbacks.data(session->callback_context, frame->payload, frame->payload_length,
                            session->receive_group, sequence_state != 0);
    return TRANSFER_SESSION_OK;
}

/**
 * @brief Handles one inbound status or progress command.
 *
 * A matching zero-progress status completes the retained outbound frame. A mismatched
 * acknowledgement follows the local error threshold, while the first two remote progress errors
 * report the remote failure and leave the session active. Later errors retry retained transfer
 * state when the session remains active.
 *
 * @param[in,out] session Transfer session awaiting a status response.
 * @param[in] command Decoded status command.
 * @return Result of the acknowledgement, sequence error, or remote error handling.
 */
static TransferSessionResult receive_status(TransferSession *session, uint16_t command) {
    session->receive_group = transfer_command_group(command);
    uint8_t parameter = transfer_command_parameter(command);
    uint8_t progress = transfer_command_progress(command);

    if (progress == 0) {
        if (parameter == session->outbound_parameter) {
            session->outbound_pending = false;
            session->remote_error_count = 0;
            return TRANSFER_SESSION_OK;
        }
        return record_error(session, TRANSFER_ERROR_SEQUENCE, TRANSFER_SESSION_SEQUENCE_ERROR);
    }

    session->remote_error_count = (session->remote_error_count + 1) & 3;
    if (session->remote_error_count <= 2) {
        return TRANSFER_SESSION_REMOTE_ERROR;
    }
    if (session->acknowledgement_pending) {
        retry_pending_acknowledgement(session);
    } else if (session->outbound_pending) {
        send_frame(session, session->pending_frame.command, session->pending_frame.payload,
                   session->pending_frame.payload_length, false);
    }
    return TRANSFER_SESSION_OK;
}

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

bool transfer_session_keepalive(TransferSession *session) {
    if (session == NULL || !session->active) {
        return false;
    }
    bool sent = send_frame(session, transfer_empty_command(), NULL, 0, false);
    uint32_t now = session->callbacks.clock(session->callback_context);
    session->activity_deadline = now + TRANSFER_SESSION_TIMEOUT_MS;
    session->data_deadline = now + TRANSFER_SESSION_TIMEOUT_MS;
    return sent;
}

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
        } else if (session->receive_frame.payload_length == 0) {
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
