#include "system/runtime.h"

void firmware_main(void) {
    motor_runtime_initialize();

    for (;;) {
        motor_runtime_poll();
    }
}
