#include "platform/led_pattern.h"

#include <stdint.h>
#include <xc.h>

/**
 * @brief LED PWM and Timer 3 configuration values.
 */
enum {
    LED_PATTERN_PWM_PERIOD = 0x03ff,    /**< LED PWM period and maximum compare value. */
    LED_PATTERN_TIMER_PRESCALER = 2,    /**< Timer 3 prescaler encoding. */
    LED_PATTERN_INTERRUPT_PRIORITY = 1, /**< Timer 3 interrupt priority. */
    LED_PATTERN_OUTPUT_CLOCK = 7,       /**< Output Compare 2 clock-source encoding. */
    LED_PATTERN_OUTPUT_MODE = 6,        /**< Output Compare 2 operating-mode encoding. */
    LED_PATTERN_OUTPUT_SYNC = 0x1f,     /**< Output Compare 2 synchronization-source encoding. */
};

/**
 * @brief LED duty value waiting for the next Timer 3 boundary.
 */
static volatile uint16_t led_pattern_duty;

/**
 * @brief Initializes the board LED PWM output.
 *
 * Configures OC2 on RD1 with a 10-bit period and starts Timer3 with the 1:64 prescaler used to
 * synchronize duty updates.
 */
void platform_led_pattern_init(void) {
    TRISDbits.TRISD1 = 0;
    led_pattern_duty = 0;

    OC2CON1 = 0;
    OC2CON2 = 0;
    OC2R = 0;
    OC2RS = LED_PATTERN_PWM_PERIOD;
    OC2CON1bits.OCTSEL = LED_PATTERN_OUTPUT_CLOCK;
    OC2CON1bits.OCM = LED_PATTERN_OUTPUT_MODE;
    OC2CON2bits.SYNCSEL = LED_PATTERN_OUTPUT_SYNC;

    TMR3 = 0;
    PR3 = LED_PATTERN_PWM_PERIOD;
    T3CON = 0;
    T3CONbits.TCKPS = LED_PATTERN_TIMER_PRESCALER;
    T3CONbits.TON = 1;

    IFS0bits.T3IF = 0;
    IEC0bits.T3IE = 1;
    IPC2bits.T3IP = LED_PATTERN_INTERRUPT_PRIORITY;
}

/**
 * @brief Queues a board LED PWM duty.
 *
 * Retains the requested duty for transfer to OC2 at the following Timer3 boundary.
 *
 * @param[in] duty PWM duty to apply.
 */
void platform_led_pattern_set_duty(uint16_t duty) { led_pattern_duty = duty; }

/**
 * @brief Applies the pending board LED duty at a PWM boundary.
 *
 * Clears the Timer3 interrupt flag and transfers the retained duty to OC2.
 */
void __attribute__((interrupt, no_auto_psv)) _T3Interrupt(void) {
    IFS0bits.T3IF = 0;
    OC2R = led_pattern_duty;
}
