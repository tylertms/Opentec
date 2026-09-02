#include "motor/command_channel_mailbox.h"

#include "motor/command_channel.h"
#include "motor/command_mailbox.h"
#include "transfer/command.h"

static bool queue_pending_packet(MotorCommandChannel *channel,
                                 MotorCommandMailboxExchange *exchange) {
    if (exchange->phase != MOTOR_COMMAND_MAILBOX_EXCHANGE_CONTROL_QUEUE ||
        exchange->write_packet != 0) {
        return true;
    }
    if (channel->reset_pending) {
        return motor_command_mailbox_exchange_queue(exchange, channel->buffers.transmit,
                                                    channel->transmit_length);
    }
    if (channel->command_pending && !channel->command_sent) {
        return motor_command_mailbox_exchange_queue(exchange, channel->buffers.transmit,
                                                    channel->transmit_length);
    }
    return true;
}

static bool queue_channel_packet(MotorCommandChannel *channel,
                                 MotorCommandMailboxExchange *exchange, const uint8_t *packet,
                                 uint16_t length) {
    if (exchange->write_packet == 0 || exchange->write_packet == packet) {
        return motor_command_mailbox_exchange_queue(exchange, packet, length);
    }
    if (exchange->write_packet != channel->buffers.transmit ||
        (!channel->command_pending && !channel->reset_pending)) {
        return false;
    }
    const uint8_t *deferred_packet = exchange->write_packet;
    uint16_t deferred_length = exchange->write_length;
    exchange->write_packet = 0;
    exchange->write_length = 0;
    if (motor_command_mailbox_exchange_queue(exchange, packet, length)) {
        return true;
    }
    exchange->write_packet = deferred_packet;
    exchange->write_length = deferred_length;
    return false;
}

static void service_scheduler(MotorCommandChannel *channel) {
    MotorCommandSchedulerInput input = {
        .transmit_pending = channel->command_pending && channel->command_sent,
        .status_write_pending = false,
        .command_write_pending = channel->command_pending && !channel->command_sent,
        .link_ready = true,
        .pending_command =
            channel->pending_payload_length == 0 ? 0 : channel->buffers.pending_payload[0],
    };
    MotorCommandSchedulerDecision decision =
        motor_command_scheduler_run(&channel->scheduler, &input);
    if (decision.command_ready) {
        if (decision.command == MOTOR_COMMAND_SCHEDULER_SEQUENCE_RESET) {
            (void)motor_command_channel_queue_recovery_command(channel);
        } else if (channel->command_pending) {
            (void)motor_command_channel_requeue_pending(channel);
        }
    }
}

MotorCommandChannelMailboxEvent
motor_command_channel_mailbox_run(MotorCommandChannel *channel,
                                  MotorCommandMailboxExchange *exchange,
                                  CommandTransport *transport) {
    MotorCommandChannelMailboxEvent event = {0};
    if (channel == 0 || exchange == 0 || transport == 0 ||
        !command_transport_is_owner(transport, MOTOR_COMMAND_MAILBOX_OWNER)) {
        return event;
    }

    service_scheduler(channel);
    if (!queue_pending_packet(channel, exchange)) {
        event.mailbox_event = MOTOR_COMMAND_MAILBOX_EXCHANGE_FAILED;
        return event;
    }

    MotorCommandMailboxExchangeResult mailbox =
        motor_command_mailbox_exchange_run(exchange, transport);
    event.failed_phase = mailbox.failed_phase;
    event.transport_result = mailbox.transport_result;
    if (mailbox.event == MOTOR_COMMAND_MAILBOX_EXCHANGE_TRANSFER_FAILED) {
        if (mailbox.failed_phase == MOTOR_COMMAND_MAILBOX_EXCHANGE_PAYLOAD_WRITE_WAIT) {
            bool retry = exchange->write_packet == channel->control_packet;
            if (exchange->write_packet == channel->buffers.transmit) {
                retry = channel->reset_pending || (channel->command_pending &&
                                                   motor_command_channel_requeue_pending(channel));
            }
            if (!retry || !motor_command_mailbox_exchange_queue(exchange, exchange->write_packet,
                                                                exchange->write_length)) {
                mailbox.event = MOTOR_COMMAND_MAILBOX_EXCHANGE_FAILED;
            }
        } else if (mailbox.failed_phase == MOTOR_COMMAND_MAILBOX_EXCHANGE_PAYLOAD_READ_WAIT) {
            event.channel_event = motor_command_channel_queue_retry(channel);
            if ((event.channel_event.actions & MOTOR_COMMAND_CHANNEL_ACTION_WRITE) != 0 &&
                !queue_channel_packet(channel, exchange, event.channel_event.packet,
                                      event.channel_event.packet_length)) {
                mailbox.event = MOTOR_COMMAND_MAILBOX_EXCHANGE_FAILED;
            }
        }
    } else if (mailbox.event == MOTOR_COMMAND_MAILBOX_EXCHANGE_RECOVERED) {
        motor_command_channel_reset(channel);
        motor_command_mailbox_exchange_reset(exchange);
        command_transport_release(transport, MOTOR_COMMAND_MAILBOX_OWNER);
    }
    if (mailbox.event == MOTOR_COMMAND_MAILBOX_EXCHANGE_PACKET_WRITTEN) {
        motor_command_channel_mark_written(channel, mailbox.packet);
        if (((channel->command_pending && !channel->command_sent) || channel->reset_pending) &&
            mailbox.packet != channel->buffers.transmit) {
            (void)motor_command_mailbox_exchange_queue(exchange, channel->buffers.transmit,
                                                       channel->transmit_length);
        }
    }
    if (mailbox.event == MOTOR_COMMAND_MAILBOX_EXCHANGE_PACKET_READ) {
        event.channel_event =
            motor_command_channel_accept(channel, mailbox.packet, mailbox.packet_length);
        if ((event.channel_event.actions & MOTOR_COMMAND_CHANNEL_ACTION_WRITE) != 0 &&
            !queue_channel_packet(channel, exchange, event.channel_event.packet,
                                  event.channel_event.packet_length)) {
            mailbox.event = MOTOR_COMMAND_MAILBOX_EXCHANGE_FAILED;
        }
    }
    event.mailbox_event = mailbox.event;
    event.status = mailbox.status;
    return event;
}
