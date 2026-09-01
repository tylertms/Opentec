#include "platform/time.h"

#include <stdint.h>
#include <xc.h>

/**
 * @brief Hardware settings for the platform millisecond timer.
 */
enum {
    TIMER_PERIOD = 7500,        /**< Timer 1 period register value. */
    TIMER_PRIORITY = 6,         /**< Timer 1 interrupt priority. */
    TIMER_PRESCALER_1_TO_8 = 1, /**< Timer 1 encoding for a 1:8 prescaler. */
};

/**
 * @brief Monotonic millisecond counter updated by the Timer 1 interrupt.
 */
static volatile uint32_t system_time_ms;

/**
 * @brief Starts the millisecond system timebase.
 *
 * Configures Timer 1 with period 7500, a 1:8 prescaler, and interrupt priority 6, clears its
 * pending interrupt, and starts the timer with the counter at zero.
 */
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

/**
 * @brief Reads the current system time.
 *
 * Temporarily blocks the Timer 1 interrupt so the two-word counter is returned coherently.
 *
 * @return Elapsed system time in milliseconds.
 */
uint32_t platform_time_ms(void) {
    uint8_t interrupt_enabled = IEC0bits.T1IE;
    IEC0bits.T1IE = 0;
    uint32_t time_ms = system_time_ms;
    IEC0bits.T1IE = interrupt_enabled;
    return time_ms;
}

/**
 * @brief Advances the system timebase.
 *
 * Increments the millisecond counter and clears the Timer 1 interrupt request.
 */
void __attribute__((interrupt, no_auto_psv)) _T1Interrupt(void) {
    system_time_ms++;
    IFS0bits.T1IF = 0;
}
