#ifndef OPENTEC_MOTOR_LINK_H
#define OPENTEC_MOTOR_LINK_H

#include <stddef.h>
#include <stdint.h>

#include "common/motor/link_frame.h"

uint16_t motor_link_crc_calculate(const uint8_t *data, size_t length);
MotorLinkFrameResult motor_link_frame_decode(const uint8_t input[MOTOR_LINK_FRAME_SIZE],
                                             MotorLinkFrame *frame);
void motor_link_position_frame_encode(const MotorLinkPositionReport *report,
                                      uint8_t output[MOTOR_LINK_FRAME_SIZE]);

#endif
