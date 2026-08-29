#include <freemaster.h>

#include "common/motor/runtime.h"

void firmware_main(void) {
    motor_runtime_initialize();
    FMSTR_Init();

    for (;;) {
        motor_runtime_poll();
        FMSTR_Poll();
    }
}
