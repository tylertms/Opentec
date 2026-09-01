#include "motor/command_channel_mailbox.h"

#include "motor/command_channel.h"
#include "motor/command_mailbox.h"
#include "transfer/command.h"

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
    if (mailbox.event == MOTOR_COMMAND_MAILBOX_EXCHANGE_PACKET_WRITTEN) {
        motor_command_channel_mark_written(channel, mailbox.packet);
        if (channel->command_pending && !channel->command_sent &&
            mailbox.packet != channel->buffers.transmit) {
            (void)motor_command_mailbox_exchange_queue(exchange, channel->buffers.transmit,
                                                       channel->transmit_length);
        }
    }
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
