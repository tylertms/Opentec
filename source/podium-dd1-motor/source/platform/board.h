#ifndef OPENTEC_MOTOR_BOARD_H
#define OPENTEC_MOTOR_BOARD_H

#include <stdbool.h>
#include <stdint.h>

/**
 * @brief Configures the motor-controller pins.
 *
 * Port clocks, pin multiplexing, pull devices, filters, and safe output levels are initialized for
 * the motor board. Each output pin changes to output before its latch level is written.
 */
void motor_pins_initialize(void);

/**
 * @brief Reads the hardware straps that identify the motor board.
 *
 * The five strap levels are packed into the board-identity byte used by the parameter service.
 *
 * @return Packed board-identity byte.
 */
uint8_t motor_board_identity_read(void);

/**
 * @brief Applies the two active-low startup interlock outputs.
 *
 * Each boolean selects whether its corresponding interlock line is driven low or released high.
 *
 * @param[in] interlock_a True to drive the first interlock low; false to drive it high.
 * @param[in] interlock_b True to drive the second interlock low; false to drive it high.
 */
void motor_startup_interlock_outputs_apply(bool interlock_a, bool interlock_b);

#endif
