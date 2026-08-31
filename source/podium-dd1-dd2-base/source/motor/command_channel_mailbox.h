#ifndef OPENTEC_BASE_MOTOR_COMMAND_CHANNEL_MAILBOX_H
#define OPENTEC_BASE_MOTOR_COMMAND_CHANNEL_MAILBOX_H

#include "motor/command_channel.h"
#include "motor/command_mailbox.h"
#include "transfer/command.h"

/** @brief Combines one mailbox-exchange result with its protocol-channel result. */
typedef struct {
    MotorCommandMailboxExchangeEvent mailbox_event; /**< Event produced by the remote mailbox exchange. */
    MotorCommandChannelEvent channel_event; /**< Event produced while applying a received motor-command packet. */
    uint32_t status; /**< Remote mailbox status value, when a status record was read. */
} MotorCommandChannelMailboxEvent;

/**
 * @brief Advances a motor-command channel through its remote mailbox.
 *
 * Runs the mailbox exchange while owner 0x20 owns the shared transport, passes received packets to
 * the protocol channel, and queues channel-generated writes for the next mailbox transfer.
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
