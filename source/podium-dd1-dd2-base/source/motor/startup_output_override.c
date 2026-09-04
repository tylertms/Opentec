#include "motor/startup_output_override.h"

#include <stddef.h>
#include <stdint.h>

#include "platform/aux_bus.h"

enum {
    MOTOR_STARTUP_AUX_ADDRESS = 0x78,
    MOTOR_STARTUP_OUTPUT_OVERRIDE_REGISTER = 0x23,
    MOTOR_STARTUP_OUTPUT_OVERRIDE_VALUE = 0xff,
};

/**
 * @brief Services the shared auxiliary bus until its completion state is published.
 *
 * The reference repeatedly calls its shared transfer service at 0x0426e8 and tests the completion
 * byte at 0x0426ec, returning at 0x0426ee after the byte becomes nonzero.
 */
static void service_until_completion(void) {
    while (platform_aux_bus_status() == PLATFORM_AUX_BUS_BUSY) {
        platform_aux_bus_service();
    }
}

bool motor_startup_output_override_write(const MotorIdentity *identity) {
    if (identity == NULL || identity->protocol == MOTOR_PROTOCOL_LEGACY) {
        return false;
    }

    uint8_t value = MOTOR_STARTUP_OUTPUT_OVERRIDE_VALUE;
    service_until_completion();
    platform_aux_bus_clear();
    if (!platform_aux_bus_start_write(MOTOR_STARTUP_AUX_ADDRESS,
                                      MOTOR_STARTUP_OUTPUT_OVERRIDE_REGISTER, &value, 1)) {
        return false;
    }
    service_until_completion();
    platform_aux_bus_clear();
    return true;
}
