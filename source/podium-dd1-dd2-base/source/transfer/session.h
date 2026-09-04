#ifndef PODIUM_DD1_DD2_BASE_TRANSFER_SESSION_H
#define PODIUM_DD1_DD2_BASE_TRANSFER_SESSION_H

#include <stdbool.h>
#include <stdint.h>

#include "transfer/frame.h"

/**
 * @brief Transfer-session timeout interval.
 *
 * The session initializes and refreshes its activity and data deadlines using this interval.
 */
enum {
    TRANSFER_SESSION_TIMEOUT_MS = 200, /**< Session timeout in milliseconds. */
};

/**
 * @brief Callback that submits an encoded transfer frame.
 *
 * The callback owns the lower transport submission and returns no status to the session.
 *
 * @param[in] context Opaque callback context supplied at session initialization.
 * @param[in] data Encoded transfer frame bytes.
 * @param[in] length Number of encoded bytes.
 */
typedef void (*TransferSessionSend)(void *context, const uint8_t *data, uint16_t length);

/**
 * @brief Callback that reports lower-transport availability.
 *
 * The session checks this callback before submission and retains inbound acknowledgements when the
 * lower transport is occupied.
 *
 * @param[in] context Opaque callback context supplied at session initialization.
 * @return True when the lower transport is occupied; otherwise false.
 */
typedef bool (*TransferSessionReady)(void *context);

/**
 * @brief Callback that consumes decoded transfer payload data.
 *
 * The callback receives each payload fragment and whether it completes the inbound message.
 *
 * @param[in] context Opaque callback context supplied at session initialization.
 * @param[in] data Decoded payload bytes.
 * @param[in] length Number of payload bytes.
 * @param[in] group Transfer group associated with the payload.
 * @param[in] complete True when this fragment completes the message.
 */
typedef void (*TransferSessionData)(void *context, const uint8_t *data, uint8_t length,
                                    uint8_t group, bool complete);

/**
 * @brief Callback that provides the session clock.
 *
 * The callback supplies the monotonic millisecond value used for session deadlines.
 *
 * @param[in] context Opaque callback context supplied at session initialization.
 * @return Current monotonic time in milliseconds.
 */
typedef uint32_t (*TransferSessionClock)(void *context);

/**
 * @brief Callback set used by one transfer session.
 *
 * All callbacks are required by transfer_session_init() and receive callback_context on every
 * invocation.
 */
typedef struct {
    TransferSessionSend send;   /**< Encoded-frame submission callback. */
    TransferSessionReady ready; /**< Lower-transport availability callback. */
    TransferSessionData data;   /**< Decoded-payload delivery callback. */
    TransferSessionClock clock; /**< Monotonic-clock callback. */
} TransferSessionCallbacks;

/**
 * @brief Result of a transfer-session operation.
 *
 * The result identifies frame validity, sequence handling, remote errors, and timeout state.
 */
typedef enum {
    TRANSFER_SESSION_OK,             /**< Operation completed without a session error. */
    TRANSFER_SESSION_INACTIVE,       /**< Session is not active. */
    TRANSFER_SESSION_INVALID_FRAME,  /**< Received frame failed validation. */
    TRANSFER_SESSION_SEQUENCE_ERROR, /**< Received sequence or acknowledgement was unexpected. */
    TRANSFER_SESSION_REMOTE_ERROR,   /**< Remote endpoint reported a transfer error. */
    TRANSFER_SESSION_TIMED_OUT,      /**< Session deadline expired. */
} TransferSessionResult;

/**
 * @brief State for one bidirectional transfer session.
 *
 * The session retains callbacks, decoded and pending frames, encoded storage, deadline state,
 * sequence tracking, and retry counters for one transfer conversation. Inbound acknowledgements
 * remain pending when the lower transport is busy and are retried during remote-error recovery.
 */
typedef struct {
    TransferSessionCallbacks callbacks; /**< Lower-transport and delivery callbacks. */
    void *callback_context;             /**< Opaque context passed to every callback. */
    TransferFrame receive_frame;        /**< Storage for the most recently decoded frame. */
    TransferFrame pending_frame;        /**< Retained outbound frame used for retry. */
    uint16_t pending_acknowledgement;   /**< Retained inbound acknowledgement command. */
    uint8_t encoded[TRANSFER_FRAME_MAX_ENCODED_SIZE]; /**< Working encoded-frame storage. */
    uint32_t activity_deadline;                       /**< Deadline for any session activity. */
    uint32_t data_deadline;                           /**< Deadline for the next data frame. */
    uint8_t receive_group;                            /**< Group of the current inbound sequence. */
    uint8_t receive_parameter;  /**< Parameter of the current inbound sequence. */
    uint8_t outbound_parameter; /**< Parameter awaited for the outbound acknowledgement. */
    uint8_t error_count;        /**< Consecutive local frame-error count. */
    uint8_t remote_error_count; /**< Remote error retry count. */
    bool active;                /**< Whether the session accepts transfer operations. */
    bool receive_active;        /**< Whether a segmented inbound message is open. */
    bool outbound_pending;      /**< Whether a retained outbound frame awaits acknowledgement. */
    bool acknowledgement_pending; /**< Whether an inbound acknowledgement awaits retry. */
} TransferSession;

/**
 * @brief Starts a transfer session with transport, data, and clock callbacks.
 *
 * Clears session state, installs the callbacks, marks the session active, and initializes both
 * deadlines from the callback clock.
 *
 * @param[out] session Session state to initialize.
 * @param[in] callbacks Required send, ready, data, and clock callbacks.
 * @param[in] callback_context Opaque value passed to every callback.
 * @return True when all callbacks are present and the session starts; otherwise false.
 */
bool transfer_session_init(TransferSession *session, const TransferSessionCallbacks *callbacks,
                           void *callback_context);

/**
 * @brief Sends one complete transfer data message and waits for its status response.
 *
 * Encodes and submits one single-frame payload when the session is active and the lower transport
 * is available.
 *
 * @param[in,out] session Active transfer session.
 * @param[in] data Payload bytes, or null when length is zero.
 * @param[in] length Payload length from zero through 124 bytes.
 * @param[in] group Two-bit transfer group.
 * @return True when the frame is accepted by the transport; otherwise false.
 */
bool transfer_session_send(TransferSession *session, const uint8_t *data, uint8_t length,
                           uint8_t group);

/**
 * @brief Keeps an active transfer operation alive.
 *
 * Attempts to submit an empty command and refreshes both activity and data deadlines from the
 * session clock.
 *
 * @param[in,out] session Active transfer session to keep alive.
 * @return True when the empty command is submitted; otherwise false.
 */
bool transfer_session_keepalive(TransferSession *session);

/**
 * @brief Processes one complete encoded frame and advances transfer state.
 *
 * Validates the frame, handles data or status sequencing, delivers payloads, and refreshes active
 * and data deadlines for the received frame.
 *
 * @param[in,out] session Active transfer session.
 * @param[in] data Encoded frame including boundary markers.
 * @param[in] length Encoded frame length.
 * @return Processing result for delivery, sequencing, remote errors, or invalid data.
 */
TransferSessionResult transfer_session_receive(TransferSession *session, const uint8_t *data,
                                               uint16_t length);

/**
 * @brief Checks transfer activity and data deadlines.
 *
 * Deactivates the session after either deadline expires and reports the resulting session state.
 *
 * @param[in,out] session Transfer session to service.
 * @return Okay while active, timed out after a deadline, or inactive.
 */
TransferSessionResult transfer_session_poll(TransferSession *session);

#endif
