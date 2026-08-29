#ifndef OPENTEC_MOTOR_BOARD_H
#define OPENTEC_MOTOR_BOARD_H

#include <stdbool.h>
#include <stdint.h>

void motor_pins_initialize(void);
uint8_t motor_board_identity_read(void);
void motor_startup_interlock_outputs_apply(bool interlock_a, bool interlock_b);

#endif
