#ifndef OPENTEC_BASE_PLATFORM_TORQUE_KEY_H
#define OPENTEC_BASE_PLATFORM_TORQUE_KEY_H

#include <stdbool.h>

/**
 * @brief Initializes the Torque Key input.
 *
 * Configures the board input used to detect whether the physical Torque Key is inserted.
 */
void platform_torque_key_init(void);

/**
 * @brief Reports whether the Torque Key is inserted.
 *
 * Reads the active-low Torque Key input.
 *
 * @return True when the Torque Key is inserted; otherwise false.
 */
bool platform_torque_key_inserted(void);

#endif
