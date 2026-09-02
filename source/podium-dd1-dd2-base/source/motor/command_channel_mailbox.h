#ifndef OPENTEC_BASE_MOTOR_COMMAND_CHANNEL_MAILBOX_H
#define OPENTEC_BASE_MOTOR_COMMAND_CHANNEL_MAILBOX_H

#include "motor/command_channel.h"
#include "motor/command_mailbox.h"
#include "transfer/command.h"

/** @brief Combines one mailbox-exchange result with its protocol-channel result. */
typedef struct {
    MotorCommandMailboxExchangeEvent
        mailbox_event; /**< Event produced by the remote mailbox exchange. */
    MotorCommandChannelEvent
        channel_event; /**< Event produced while applying a received motor-command packet. */
    uint32_t status;   /**< Remote mailbox status value, when a status record was read. */
    MotorCommandMailboxExchangePhase
        failed_phase;                        /**< Mailbox phase active at a lower-layer refusal. */
    CommandTransportResult transport_result; /**< Lower transport result for a refusal. */
} MotorCommandChannelMailboxEvent;

/**
 * @brief Advances a motor-command channel through its remote mailbox.
 *
 * Runs the live timeout scheduler and mailbox exchange while owner 0x20 owns the shared transport,
 * passes received packets to the protocol channel, preserves reserved transmit sequences, and
 * queues channel-generated writes for the next mailbox transfer. Recoverable lower-layer refusals
 * return the exchange to its control boundary without discarding the retained packet.
 *
 * @param[in,out] channel Motor-command protocol channel to advance.
 * @param[in,out] exchange Remote mailbox exchange to advance.
 * @param[in,out] transport Shared command transport used by the mailbox.
 * @return Combined mailbox, protocol, and remote-status events from this call.
 */
MotorCommandChannelMailboxEvent
motor_command_channel_mailbox_run(MotorCommandChannel *channel,
                                  MotorCommandMailboxExchange *exchange,
                                  CommandTransport *transport);

#endif
