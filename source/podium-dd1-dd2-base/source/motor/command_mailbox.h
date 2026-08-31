#ifndef OPENTEC_BASE_MOTOR_COMMAND_MAILBOX_H
#define OPENTEC_BASE_MOTOR_COMMAND_MAILBOX_H

#include <stdbool.h>
#include <stdint.h>

#include "transfer/command.h"

/** @brief Defines the remote mailbox owner, record sizes, and control flags. */
enum {
    MOTOR_COMMAND_MAILBOX_OWNER = 0x20, /**< Owner identifier used for mailbox transport requests. */
    MOTOR_COMMAND_MAILBOX_CONTROL_SIZE = 4, /**< Size of the remote mailbox control record. */
    MOTOR_COMMAND_MAILBOX_LENGTH_SIZE = 2, /**< Size of the remote mailbox length record. */
    MOTOR_COMMAND_MAILBOX_STATUS_SIZE = 4, /**< Size of the remote mailbox status record. */
    MOTOR_COMMAND_MAILBOX_CONTROL_PAYLOAD_AVAILABLE = 0x40, /**< Control flag indicating a packet is available to read. */
    MOTOR_COMMAND_MAILBOX_CONTROL_STATUS_MASK = 0xc0, /**< Mask selecting the control status bits. */
    MOTOR_COMMAND_MAILBOX_CONTROL_STATUS_RETRY = 0x80, /**< Control status requesting retry handling. */
};

/** @brief Decoded fields from the remote mailbox control record. */
typedef struct {
    uint16_t payload_length; /**< Big-endian packet length reported by the mailbox. */
    uint8_t flags; /**< Availability and status flags reported by the mailbox. */
    uint8_t reserved; /**< Reserved control-record byte. */
} MotorCommandMailboxControl;

/** @brief Identifies the current phase of a remote mailbox exchange. */
typedef enum {
    MOTOR_COMMAND_MAILBOX_EXCHANGE_CONTROL_QUEUE, /**< A control-record read must be queued. */
    MOTOR_COMMAND_MAILBOX_EXCHANGE_CONTROL_WAIT, /**< A queued control-record read is in flight. */
    MOTOR_COMMAND_MAILBOX_EXCHANGE_PAYLOAD_READ_WAIT, /**< A mailbox payload read is in flight. */
    MOTOR_COMMAND_MAILBOX_EXCHANGE_PAYLOAD_WRITE_WAIT, /**< A mailbox payload write is in flight. */
    MOTOR_COMMAND_MAILBOX_EXCHANGE_STATUS_WAIT, /**< A mailbox status read is in flight. */
} MotorCommandMailboxExchangePhase;

/** @brief Reports progress or failure from a remote mailbox exchange. */
typedef enum {
    MOTOR_COMMAND_MAILBOX_EXCHANGE_NONE, /**< No exchange event was produced. */
    MOTOR_COMMAND_MAILBOX_EXCHANGE_PACKET_READ, /**< A packet was read from the mailbox. */
    MOTOR_COMMAND_MAILBOX_EXCHANGE_PACKET_WRITTEN, /**< A queued packet was written to the mailbox. */
    MOTOR_COMMAND_MAILBOX_EXCHANGE_STATUS_READ, /**< A status record was read from the mailbox. */
    MOTOR_COMMAND_MAILBOX_EXCHANGE_FAILED, /**< The exchange failed to advance or queue a transfer. */
} MotorCommandMailboxExchangeEvent;

/** @brief Reports one result produced by a mailbox exchange step. */
typedef struct {
    MotorCommandMailboxExchangeEvent event; /**< Exchange event produced by the step. */
    const uint8_t *packet; /**< Packet storage when event is PACKET_READ. */
    uint16_t packet_length; /**< Number of bytes available at packet. */
    uint32_t status; /**< Big-endian mailbox status when event is STATUS_READ. */
} MotorCommandMailboxExchangeResult;

/** @brief Maintains state for exchanging packets through the remote motor mailbox. */
typedef struct {
    uint8_t control_record[MOTOR_COMMAND_MAILBOX_CONTROL_SIZE]; /**< Raw control record read from the mailbox. */
    uint8_t status_record[MOTOR_COMMAND_MAILBOX_STATUS_SIZE]; /**< Raw status record read from the mailbox. */
    MotorCommandMailboxControl control; /**< Decoded control record. */
    uint8_t *read_buffer; /**< Caller-owned destination for mailbox packets. */
    const uint8_t *write_packet; /**< Caller-owned packet retained for mailbox writing. */
    uint16_t read_capacity; /**< Capacity of read_buffer in bytes. */
    uint16_t read_length; /**< Length of the packet currently being read. */
    uint16_t write_length; /**< Length of write_packet in bytes. */
    MotorCommandMailboxExchangePhase phase; /**< Current mailbox exchange phase. */
    uint8_t status_retry_count; /**< Consecutive retry-status count. */
} MotorCommandMailboxExchange;

/**
 * @brief Queues a remote mailbox payload read.
 *
 * Requests packet bytes from the mailbox payload record at offset 0x80 through owner 0x20.
 *
 * @param[in,out] transport Shared command transport receiving the read request.
 * @param[out] payload Destination for the returned packet bytes.
 * @param[in] length Number of packet bytes to read.
 * @return Command transport result from queuing the read.
 */
CommandTransportResult motor_command_mailbox_queue_payload_read(CommandTransport *transport,
                                                                uint8_t *payload, uint16_t length);

/**
 * @brief Queues a remote mailbox payload write.
 *
 * Writes packet bytes to the mailbox payload record at offset 0x80 through owner 0x20.
 *
 * @param[in,out] transport Shared command transport receiving the write request.
 * @param[in] payload Packet bytes to write.
 * @param[in] length Number of packet bytes to write.
 * @return Command transport result from queuing the write.
 */
CommandTransportResult motor_command_mailbox_queue_payload_write(CommandTransport *transport,
                                                                 const uint8_t *payload,
                                                                 uint16_t length);

/**
 * @brief Queues a remote mailbox length read.
 *
 * Requests the two-byte packet-length record at offset 0x81 through owner 0x20.
 *
 * @param[in,out] transport Shared command transport receiving the read request.
 * @param[out] length Two-byte destination record.
 * @return Command transport result from queuing the read.
 */
CommandTransportResult
motor_command_mailbox_queue_length_read(CommandTransport *transport,
                                        uint8_t length[MOTOR_COMMAND_MAILBOX_LENGTH_SIZE]);

/**
 * @brief Queues a remote mailbox control read.
 *
 * Requests the four-byte availability and status record at offset 0x82 through owner 0x20.
 *
 * @param[in,out] transport Shared command transport receiving the read request.
 * @param[out] control Four-byte destination control record.
 * @return Command transport result from queuing the read.
 */
CommandTransportResult
motor_command_mailbox_queue_control_read(CommandTransport *transport,
                                         uint8_t control[MOTOR_COMMAND_MAILBOX_CONTROL_SIZE]);

/**
 * @brief Queues a remote mailbox status read.
 *
 * Requests the four-byte status record at offset 0x90 through owner 0x20.
 *
 * @param[in,out] transport Shared command transport receiving the read request.
 * @param[out] status Four-byte destination status record.
 * @return Command transport result from queuing the read.
 */
CommandTransportResult
motor_command_mailbox_queue_status_read(CommandTransport *transport,
                                        uint8_t status[MOTOR_COMMAND_MAILBOX_STATUS_SIZE]);

/**
 * @brief Decodes a remote mailbox control record.
 *
 * Copies its flags and reserved byte and combines the two-byte big-endian payload length.
 *
 * @param[in] record Four-byte raw control record.
 * @param[out] control Decoded control fields.
 * @return true when record and control are non-null; otherwise false.
 */
bool motor_command_mailbox_control_decode(const uint8_t record[MOTOR_COMMAND_MAILBOX_CONTROL_SIZE],
                                          MotorCommandMailboxControl *control);

/**
 * @brief Initializes a motor-command mailbox exchange.
 *
 * Attaches caller-owned packet read storage and clears exchange progress so the next run queues a
 * control-record read.
 *
 * @param[out] exchange Exchange state to initialize.
 * @param[in] read_buffer Caller-owned storage for packets read from the mailbox.
 * @param[in] read_capacity Capacity of read_buffer in bytes.
 * @return true when exchange and read storage are non-null and nonempty; otherwise false.
 */
bool motor_command_mailbox_exchange_init(MotorCommandMailboxExchange *exchange,
                                         uint8_t *read_buffer, uint16_t read_capacity);

/**
 * @brief Resets a motor-command mailbox exchange.
 *
 * Discards queued packet, transfer, and retry state while retaining the caller-owned read buffer.
 *
 * @param[in,out] exchange Exchange state to reset.
 */
void motor_command_mailbox_exchange_reset(MotorCommandMailboxExchange *exchange);

/**
 * @brief Queues a packet for remote mailbox transmission.
 *
 * Retains the packet pointer and length for a later payload write, allowing replacement only before
 * that write has started and only with the same packet storage.
 *
 * @param[in,out] exchange Exchange receiving the packet reference.
 * @param[in] packet Packet bytes to write.
 * @param[in] length Number of packet bytes to write.
 * @return true when the packet reference was accepted; otherwise false.
 */
bool motor_command_mailbox_exchange_queue(MotorCommandMailboxExchange *exchange,
                                          const uint8_t *packet, uint16_t length);

/**
 * @brief Advances a motor-command mailbox exchange.
 *
 * Polls the control record, prioritizes available packets, reads status after consecutive retry
 * indications, and writes a queued packet when the mailbox is available.
 *
 * @param[in,out] exchange Exchange state to advance.
 * @param[in,out] transport Shared owner-0x20 command transport.
 * @return Event and data produced by this exchange step.
 */
MotorCommandMailboxExchangeResult
motor_command_mailbox_exchange_run(MotorCommandMailboxExchange *exchange,
                                   CommandTransport *transport);

#endif
