#ifndef OPENTEC_BASE_MOTOR_LIVE_FRAME_H
#define OPENTEC_BASE_MOTOR_LIVE_FRAME_H

#include <stdbool.h>
#include <stdint.h>

#include "force_feedback/output_report.h"

enum {
    MOTOR_LIVE_FRAME_SIZE = 13,
    MOTOR_LIVE_PAYLOAD_SIZE = 8,
    MOTOR_LIVE_FRAME_START = 0x7b,
    MOTOR_LIVE_FRAME_END = 0x7d,
    MOTOR_LIVE_POSITION_TYPE = 0x01,
    MOTOR_LIVE_EFFECT_TYPE = 0x02,
    MOTOR_LIVE_REPLAY_FLAG = 0x80,
};

typedef enum {
    MOTOR_LIVE_FRAME_VALID,
    MOTOR_LIVE_FRAME_INVALID_BOUNDARY,
    MOTOR_LIVE_FRAME_INVALID_CHECKSUM,
} MotorLiveFrameResult;

typedef struct {
    uint8_t type;
    uint8_t payload[MOTOR_LIVE_PAYLOAD_SIZE];
} MotorLiveFrame;

typedef struct {
    bool replay;
    int32_t wheel_position;
    uint16_t motor_torque;
    bool auxiliary_negative;
    uint16_t auxiliary_position;
} MotorPositionReport;

void motor_live_frame_encode(const MotorLiveFrame *frame, uint8_t output[MOTOR_LIVE_FRAME_SIZE]);
MotorLiveFrameResult motor_live_frame_decode(const uint8_t input[MOTOR_LIVE_FRAME_SIZE],
                                             MotorLiveFrame *frame);
bool motor_position_report_decode(const MotorLiveFrame *frame, MotorPositionReport *report);
void motor_live_force_frame_init(int16_t center_position, const ForceOutputReport *report,
                                 MotorLiveFrame *frame);

#endif
