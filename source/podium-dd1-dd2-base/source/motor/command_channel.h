#ifndef OPENTEC_BASE_MOTOR_COMMAND_CHANNEL_H
#define OPENTEC_BASE_MOTOR_COMMAND_CHANNEL_H

#include <stdbool.h>
#include <stdint.h>

#include "motor/command_application.h"
#include "motor/command_message.h"
#include "motor/command_receiver.h"

typedef enum {
    MOTOR_COMMAND_CHANNEL_ACTION_NONE = 0,
    MOTOR_COMMAND_CHANNEL_ACTION_WRITE = 1 << 0,
} MotorCommandChannelAction;

typedef struct {
    uint8_t *receive_assembly;
    uint16_t receive_assembly_capacity;
    uint8_t *transmit;
    uint16_t transmit_capacity;
    uint8_t *pending_payload;
    uint16_t pending_payload_capacity;
} MotorCommandChannelBuffers;

typedef struct {
    MotorCommandChannelAction actions;
    MotorCommandReceiveResult receive_result;
    MotorCommandApplicationEvent application;
    const uint8_t *packet;
    uint16_t packet_length;
} MotorCommandChannelEvent;

typedef struct {
    MotorCommandReceiver receiver;
    MotorCommandApplication application;
    MotorCommandMessage message;
    MotorCommandChannelBuffers buffers;
    uint16_t transmit_length;
    uint16_t pending_payload_length;
    bool command_pending;
} MotorCommandChannel;

bool motor_command_channel_init(MotorCommandChannel *channel,
                                const MotorCommandChannelBuffers *buffers);
void motor_command_channel_reset(MotorCommandChannel *channel);
bool motor_command_channel_queue_payload(MotorCommandChannel *channel, const uint8_t *payload,
                                         uint16_t payload_length);
bool motor_command_channel_queue_sequence_reset(MotorCommandChannel *channel);
bool motor_command_channel_queue_digest_request(MotorCommandChannel *channel);
bool motor_command_channel_queue_information_request(MotorCommandChannel *channel,
                                                     uint8_t selector);
MotorCommandChannelEvent motor_command_channel_accept(MotorCommandChannel *channel,
                                                      const uint8_t *packet, uint16_t length);
const MotorCommandApplication *
motor_command_channel_application(const MotorCommandChannel *channel);

#endif
