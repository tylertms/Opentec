#include "platform/time.h"

#include <stdint.h>
#include <xc.h>

enum {
    TIMER_PERIOD = 7499,
    TIMER_PRIORITY = 6,
    TIMER_PRESCALER_1_TO_8 = 1,
};

static volatile uint32_t system_time_ms;

void platform_time_init(void) {
    T1CON = 0;
    TMR1 = 0;
    PR1 = TIMER_PERIOD;
    T1CONbits.TCKPS = TIMER_PRESCALER_1_TO_8;
    IPC0bits.T1IP = TIMER_PRIORITY;
    IFS0bits.T1IF = 0;
    IEC0bits.T1IE = 1;
    T1CONbits.TON = 1;
}

uint32_t platform_time_ms(void) {
    uint8_t interrupt_enabled = IEC0bits.T1IE;
    IEC0bits.T1IE = 0;
    uint32_t time_ms = system_time_ms;
    IEC0bits.T1IE = interrupt_enabled;
    return time_ms;
}

void __attribute__((interrupt, no_auto_psv)) _T1Interrupt(void) {
    system_time_ms++;
    IFS0bits.T1IF = 0;
}
