#include <freemaster.h>

void firmware_main(void) {
    FMSTR_Init();

    for (;;) {
        FMSTR_Poll();
    }
}
