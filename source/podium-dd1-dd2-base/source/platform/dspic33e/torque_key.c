#include "platform/torque_key.h"

#include <stdbool.h>
#include <xc.h>

/**
 * @brief Configures the Torque Key input.
 *
 * Selects RB2 as the digital input used by the active-low Torque Key circuit.
 */
void platform_torque_key_init(void) { TRISBbits.TRISB2 = 1; }

/**
 * @brief Reads the physical Torque Key state.
 *
 * Samples the active-low RB2 input.
 *
 * @return True while the Torque Key is inserted.
 */
bool platform_torque_key_inserted(void) { return PORTBbits.RB2 == 0; }
