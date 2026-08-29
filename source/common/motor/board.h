#ifndef OPENTEC_MOTOR_BOARD_H
#define OPENTEC_MOTOR_BOARD_H

#include <stdint.h>

void motor_pins_initialize(void);
uint8_t motor_board_identity_read(void);

#endif
