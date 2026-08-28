#include "platform/status_led.h"

#include <stdbool.h>
#include <xc.h>

void platform_status_led_init(void) {
    TRISGbits.TRISG14 = 0;
    LATGbits.LATG14 = 1;
}

void platform_status_led_set(bool on) { LATGbits.LATG14 = on ? 0 : 1; }
