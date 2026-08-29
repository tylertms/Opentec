#ifndef OPENTEC_MOTOR_LINK_FRAME_H
#define OPENTEC_MOTOR_LINK_FRAME_H

#include <stdbool.h>
#include <stdint.h>

#define MOTOR_LINK_FRAME_SIZE 13U
#define MOTOR_LINK_PAYLOAD_SIZE 8U
#define MOTOR_LINK_CHECKSUM_INPUT_SIZE 9U

typedef enum {
    MOTOR_LINK_FRAME_VALID,
    MOTOR_LINK_FRAME_INVALID_BOUNDARY,
    MOTOR_LINK_FRAME_INVALID_CHECKSUM,
} MotorLinkFrameResult;

typedef enum {
    MOTOR_LINK_FORCE_TYPE = 1,
    MOTOR_LINK_STATUS_TYPE = 2,
} MotorLinkFrameType;

typedef struct {
    uint8_t type;
    uint8_t payload[MOTOR_LINK_PAYLOAD_SIZE];
} MotorLinkFrame;

typedef struct {
    int16_t center;
    bool positive;
    uint16_t primary;
    int16_t secondary;
} MotorLinkForceCommand;

typedef struct {
    uint8_t status;
    uint8_t command[7];
} MotorLinkStatusCommand;

MotorLinkFrameResult motor_link_frame_decode_checked(const uint8_t input[MOTOR_LINK_FRAME_SIZE],
                                                     uint16_t checksum, MotorLinkFrame *frame);
bool motor_link_force_command_decode(const MotorLinkFrame *frame, MotorLinkForceCommand *command);
bool motor_link_status_command_decode(const MotorLinkFrame *frame, MotorLinkStatusCommand *command);

#endif
