#include <freemaster.h>

#include "common/motor/foc.h"

static MotorFocState motor_foc;

void firmware_main(void) {
    motor_foc_initialize(&motor_foc);
    FMSTR_Init();

    for (;;) {
        FMSTR_Poll();
    }
}
