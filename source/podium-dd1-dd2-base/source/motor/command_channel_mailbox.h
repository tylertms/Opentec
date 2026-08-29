#ifndef OPENTEC_BASE_MOTOR_COMMAND_CHANNEL_MAILBOX_H
#define OPENTEC_BASE_MOTOR_COMMAND_CHANNEL_MAILBOX_H

#include "motor/command_channel.h"
#include "motor/command_mailbox.h"
#include "transfer/command.h"

typedef struct {
    MotorCommandMailboxExchangeEvent mailbox_event;
    MotorCommandChannelEvent channel_event;
    uint32_t status;
} MotorCommandChannelMailboxEvent;

MotorCommandChannelMailboxEvent
motor_command_channel_mailbox_run(MotorCommandChannel *channel,
                                  MotorCommandMailboxExchange *exchange,
                                  CommandTransport *transport);

#endif
