#include "platform/clock.h"

#include <xc.h>

enum {
    SYSTEM_PLL_FEEDBACK = 58,
    SYSTEM_PLL_INPUT_DIVIDER = 1,
    SYSTEM_PLL_OUTPUT_DIVIDER = 0,
    PRIMARY_OSCILLATOR_WITH_PLL = 3,
    CLOCK_SWITCH_REQUEST = 1,
    AUXILIARY_PLL_INPUT_DIVIDER = 2,
    AUXILIARY_PLL_OUTPUT_DIVIDER = 6,
    AUXILIARY_CLOCK_DIVIDER = 7,
};

/**
 * @brief Configures the processor and auxiliary clock trees.
 *
 * Disables the software watchdog, selects the primary oscillator with PLL feedback 58 and input
 * divider 1, waits for the requested source and lock, then enables the auxiliary PLL with input
 * divider 2, output divider 6, and clock divider 7 and waits for its lock.
 */
void platform_clock_init(void) {
    RCONbits.SWDTEN = 0;
    PLLFBD = SYSTEM_PLL_FEEDBACK;
    CLKDIVbits.PLLPRE = SYSTEM_PLL_INPUT_DIVIDER;
    CLKDIVbits.PLLPOST = SYSTEM_PLL_OUTPUT_DIVIDER;

    __builtin_write_OSCCONH(PRIMARY_OSCILLATOR_WITH_PLL);
    __builtin_write_OSCCONL(CLOCK_SWITCH_REQUEST);
    while (OSCCONbits.COSC != PRIMARY_OSCILLATOR_WITH_PLL) {
    }
    while (OSCCONbits.LOCK == 0) {
    }

    ACLKCON3 = 0;
    ACLKCON3bits.APLLPRE = AUXILIARY_PLL_INPUT_DIVIDER;
    ACLKCON3bits.APLLPOST = AUXILIARY_PLL_OUTPUT_DIVIDER;
    ACLKCON3bits.ASRCSEL = 1;
    ACLKCON3bits.SELACLK = 1;
    ACLKDIV3 = AUXILIARY_CLOCK_DIVIDER;
    ACLKCON3bits.ENAPLL = 1;
    while (ACLKCON3bits.APLLCK == 0) {
    }
}
