#ifndef OPENTEC_BASE_MOTOR_COMMAND_FRAGMENT_H
#define OPENTEC_BASE_MOTOR_COMMAND_FRAGMENT_H

#include <stdint.h>

enum {
    MOTOR_COMMAND_FRAGMENT_MAX_SIZE = 1009,
};

typedef enum {
    MOTOR_COMMAND_FRAGMENT_INVALID,
    MOTOR_COMMAND_FRAGMENT_WAITING,
    MOTOR_COMMAND_FRAGMENT_COMPLETE,
} MotorCommandFragmentResult;

typedef struct {
    uint8_t *data;
    uint16_t capacity;
    uint16_t length;
    uint16_t content_length;
} MotorCommandFragment;

void motor_command_fragment_init(MotorCommandFragment *fragment, uint8_t *data, uint16_t capacity);
MotorCommandFragmentResult motor_command_fragment_accept(MotorCommandFragment *fragment,
                                                         const uint8_t *packet, uint16_t length);

#endif
