#include "platform/force_feedback_timer.h"

#include <xc.h>

/**
 * @brief Force-feedback timer configuration values.
 */
enum {
    FORCE_FEEDBACK_TIMER_PERIOD = 0x16a8, /**< Timer 2 period register value. */
    FORCE_FEEDBACK_TIMER_PRIORITY = 4,    /**< Timer 2 interrupt priority. */
};

/**
 * @brief Runtime callback invoked for each force-feedback timer tick.
 */
static PlatformForceFeedbackTickHandler tick_handler;

/**
 * @brief Caller-owned context passed to the runtime tick callback.
 */
static void *tick_context;

/**
 * @brief Start the force-feedback runtime timer.
 *
 * Configures Timer 2 with its internal clock, a 0x16A8 period, and interrupt priority four. Each
 * interrupt invokes the supplied runtime tick handler before acknowledging the timer event.
 *
 * @param[in] handler Function invoked for each force-feedback timer interrupt.
 * @param[in,out] context State passed unchanged to the tick handler.
 */
void platform_force_feedback_timer_init(PlatformForceFeedbackTickHandler handler, void *context) {
    T2CON = 0;
    TMR2 = 0;
    PR2 = FORCE_FEEDBACK_TIMER_PERIOD;
    tick_handler = handler;
    tick_context = context;
    IPC1bits.T2IP = FORCE_FEEDBACK_TIMER_PRIORITY;
    IFS0bits.T2IF = 0;
    IEC0bits.T2IE = 1;
    T2CONbits.TON = 1;
}

/**
 * @brief Dispatch one force-feedback timer interrupt.
 *
 * Invokes the configured runtime tick handler and then clears the Timer 2 interrupt flag.
 */
void __attribute__((interrupt, no_auto_psv)) _T2Interrupt(void) {
    if (tick_handler != 0) {
        tick_handler(tick_context);
    }
    IFS0bits.T2IF = 0;
}
