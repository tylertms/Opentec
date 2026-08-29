#ifndef OPENTEC_MOTOR_PROTOCOL_H
#define OPENTEC_MOTOR_PROTOCOL_H

#include <stdbool.h>
#include <stdint.h>

#include "common/motor/drive.h"
#include "common/motor/force_feedback_engine.h"
#include "common/motor/link_frame.h"

typedef struct {
    MotorForceFeedbackEngine force_feedback;
    MotorDriveCommand live_drive;
    int16_t center;
    uint8_t status;
    uint8_t normal_output_percent;
    uint32_t next_force_feedback_tick;
    uint32_t next_force_ramp_tick;
    bool live_drive_updated;
} MotorProtocolState;

void motor_protocol_initialize(MotorProtocolState *state, uint8_t normal_output_percent);
bool motor_protocol_frame_apply(MotorProtocolState *state, const MotorLinkFrame *frame);
bool motor_protocol_force_feedback_service(MotorProtocolState *state, uint32_t now,
                                           int32_t centered_position, int32_t velocity);

#endif
