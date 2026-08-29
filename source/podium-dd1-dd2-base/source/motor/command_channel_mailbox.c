#include "motor/command_channel_mailbox.h"

#include "motor/command_channel.h"
#include "motor/command_mailbox.h"
#include "transfer/command.h"

/**
 * @brief Advances a motor-command channel through its remote mailbox.
 *
 * Polls the mailbox while owner 0x20 holds the shared transport, passes received packets through
 * the protocol channel, and queues any acknowledgement, retry, or rebuilt request produced by the
 * channel.
 *
 * @param[in,out] channel Motor-command protocol channel.
 * @param[in,out] exchange Remote mailbox exchange.
 * @param[in,out] transport Shared command transport.
 * @return Mailbox progress, protocol event, and any reported remote status.
 */
MotorCommandChannelMailboxEvent
motor_command_channel_mailbox_run(MotorCommandChannel *channel,
                                  MotorCommandMailboxExchange *exchange,
                                  CommandTransport *transport) {
    MotorCommandChannelMailboxEvent event = {0};
    if (channel == 0 || exchange == 0 || transport == 0 ||
        !command_transport_is_owner(transport, MOTOR_COMMAND_MAILBOX_OWNER)) {
        return event;
    }

    MotorCommandMailboxExchangeResult mailbox =
        motor_command_mailbox_exchange_run(exchange, transport);
    if (mailbox.event == MOTOR_COMMAND_MAILBOX_EXCHANGE_PACKET_READ) {
        event.channel_event =
            motor_command_channel_accept(channel, mailbox.packet, mailbox.packet_length);
        if ((event.channel_event.actions & MOTOR_COMMAND_CHANNEL_ACTION_WRITE) != 0 &&
            !motor_command_mailbox_exchange_queue(exchange, event.channel_event.packet,
                                                  event.channel_event.packet_length)) {
            mailbox.event = MOTOR_COMMAND_MAILBOX_EXCHANGE_FAILED;
        }
    }
    event.mailbox_event = mailbox.event;
    event.status = mailbox.status;
    return event;
}
