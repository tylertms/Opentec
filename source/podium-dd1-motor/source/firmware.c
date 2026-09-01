#include "system/runtime.h"

/**
 * @brief Starts and continuously services the motor firmware runtime.
 *
 * Hardware and control state are initialized once before deferred work is polled forever.
 */
void firmware_main(void) {
    motor_runtime_initialize();

    for (;;) {
        motor_runtime_poll();
    }
}
