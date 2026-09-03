#ifndef OPENTEC_BASE_MOTOR_OUTPUT_TRANSPORT_H
#define OPENTEC_BASE_MOTOR_OUTPUT_TRANSPORT_H

#include <stdbool.h>
#include <stdint.h>

#include "force_feedback/output_report.h"
#include "motor/live_frame.h"

/** @brief Motor output command and status protocol constants. */
enum {
    MOTOR_OUTPUT_COMMAND_SIZE = 7,     /**< Number of bytes in one queued output command. */
    MOTOR_OUTPUT_QUEUE_CAPACITY = 100, /**< Number of complete commands retained in the queue. */
    MOTOR_OUTPUT_REPLAY_CAPACITY = 2,  /**< Number of outbound frames retained for replay. */
    MOTOR_OUTPUT_STATUS_REMOTE_EFFECTS = 1 << 0,    /**< Status bit for remote force effects. */
    MOTOR_OUTPUT_STATUS_ENABLED = 1 << 1,           /**< Status bit for enabled force output. */
    MOTOR_OUTPUT_STATUS_OVERRIDE_ACTIVE = 1 << 2,   /**< Status bit for an active override. */
    MOTOR_OUTPUT_STATUS_PROTOCOL_INTERLOCKED =
        1 << 3, /**< Status bit for a protocol output interlock. */
    MOTOR_OUTPUT_STATUS_PRIMARY_DISABLED = 1 << 4,  /**< Status bit for disabled primary output. */
    MOTOR_OUTPUT_STATUS_SECONDARY_DISABLED = 1
                                             << 5, /**< Status bit for disabled secondary output. */
    MOTOR_OUTPUT_STATUS_CONNECTION_INTERLOCKED =
        1 << 6, /**< Status bit for the wheel connection interlock. */
    MOTOR_OUTPUT_STATUS_FULL_TORQUE = 1 << 7, /**< Status bit for full-torque acknowledgement. */
};

/**
 * @brief Queued motor output commands and replay history.
 *
 * Owns the command ring, the last status byte used for status-only frames, and the retained live
 * frames used to answer replay requests.
 */
typedef struct {
    uint8_t commands[MOTOR_OUTPUT_QUEUE_CAPACITY]
                    [MOTOR_OUTPUT_COMMAND_SIZE]; /**< Queued command records. */
    uint8_t read_index;                          /**< Index of the oldest queued command. */
    uint8_t write_index;                         /**< Index where the next command is queued. */
    uint8_t count;                               /**< Number of queued command records. */
    uint8_t previous_status; /**< Status byte sent by the most recent status frame. */
    MotorLiveFrame replay_frames[MOTOR_OUTPUT_REPLAY_CAPACITY]; /**< Retained frames for replay. */
    uint8_t replay_write_index; /**< Index where the next replay frame is retained. */
} MotorOutputTransport;

/**
 * @brief Initializes motor output transport state.
 *
 * Clears queued commands, status history, and replay frames before output scheduling begins.
 *
 * @param[out] transport Output transport state to initialize.
 */
void motor_output_transport_init(MotorOutputTransport *transport);

/**
 * @brief Queues one complete motor output command.
 *
 * Copies the fixed-width command into the ring buffer when space is available.
 *
 * @param[in,out] transport Output transport state to update.
 * @param[in] command Fixed-width motor output command.
 * @return True when the command was queued; otherwise false when the queue is full or a pointer is
 * null.
 */
bool motor_output_transport_enqueue_command(MotorOutputTransport *transport,
                                            const uint8_t command[MOTOR_OUTPUT_COMMAND_SIZE]);

/**
 * @brief Queues a motor output opcode.
 *
 * Creates a fixed-width command with the opcode in its first byte and zero in the remaining bytes.
 *
 * @param[in,out] transport Output transport state to update.
 * @param[in] opcode Motor output command opcode.
 * @return True when the opcode was queued; otherwise false when the queue is full or transport is
 * null.
 */
bool motor_output_transport_enqueue_opcode(MotorOutputTransport *transport, uint8_t opcode);

/**
 * @brief Queues clear commands for all host force-effect slots.
 *
 * Appends one clear record for every host effect slot to the ordinary command FIFO. Existing
 * commands remain ahead of the appended records, and a full queue accepts only the records that
 * fit.
 *
 * @param[in,out] transport Output transport state receiving the clear commands.
 * @return Number of slot-clear commands scheduled, or zero when transport is null.
 */
uint8_t motor_output_transport_enqueue_host_effect_clears(MotorOutputTransport *transport);

/**
 * @brief Retains one transmitted motor live frame.
 *
 * Adds the frame to the bounded replay history used to answer later replay requests.
 *
 * @param[in,out] transport Output transport state and replay history.
 * @param[in] frame Transmitted motor live frame to retain.
 */
void motor_output_transport_remember_frame(MotorOutputTransport *transport,
                                           const MotorLiveFrame *frame);

/**
 * @brief Retrieves the retained motor frame selected for replay.
 *
 * Copies the replay-history slot selected by the reference ring index without changing command or
 * status scheduling state. An empty or single-entry history therefore returns its zero-initialized
 * fallback slot.
 *
 * @param[in] transport Output transport state and replay history.
 * @param[out] frame Destination for the retained replay frame.
 * @return True when transport and frame are non-null; otherwise false.
 */
bool motor_output_transport_replay_frame(const MotorOutputTransport *transport,
                                         MotorLiveFrame *frame);

/**
 * @brief Builds the next motor live frame.
 *
 * Selects the oldest queued command, a changed-status frame, or the current live force frame in
 * the transport priority order.
 *
 * @param[in,out] transport Output transport queue and status history.
 * @param[in] status Current motor output status byte.
 * @param[in] center_position Stored wheel-center position.
 * @param[in] report Current live force output report.
 * @param[out] frame Destination for the selected motor live frame.
 */
void motor_output_transport_build_frame(MotorOutputTransport *transport, uint8_t status,
                                        int16_t center_position, const ForceOutputReport *report,
                                        MotorLiveFrame *frame);

#endif
