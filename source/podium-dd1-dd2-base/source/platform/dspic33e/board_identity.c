#include "platform/board_identity.h"

#include <stdint.h>
#include <xc.h>

/**
 * @brief Prepares the board-identity strap inputs.
 *
 * Disables Output Compare 2 and analog input on port D, selects RD1, RD12, RD13, RE9, and RE8 as
 * inputs, and applies the strap-specific pull-downs and pull-ups.
 */
static void enable_mode_inputs(void) {
    OC2CON1 = 0;
    OC2CON2 = 0;
    ANSELD = 0;

    TRISDbits.TRISD1 = 1;
    CNPUDbits.CNPUD1 = 1;
    TRISDbits.TRISD12 = 1;
    CNPUDbits.CNPUD12 = 1;
    TRISDbits.TRISD13 = 1;
    CNPUDbits.CNPUD13 = 1;
    TRISEbits.TRISE9 = 1;
    CNPUEbits.CNPUE9 = 1;
    TRISEbits.TRISE8 = 1;
    CNPUEbits.CNPUE8 = 1;
}

/**
 * @brief Releases the board-identity strap inputs.
 *
 * Removes the temporary pulls and restores the five strap pins as outputs after sampling.
 */
static void disable_mode_inputs(void) {
    CNPUDbits.CNPUD1 = 0;
    TRISDbits.TRISD1 = 0;
    CNPUDbits.CNPUD12 = 0;
    TRISDbits.TRISD12 = 0;
    CNPUDbits.CNPUD13 = 0;
    TRISDbits.TRISD13 = 0;
    CNPUEbits.CNPUE9 = 0;
    TRISEbits.TRISE9 = 0;
    CNPUEbits.CNPUE8 = 0;
    TRISEbits.TRISE8 = 0;
}

/**
 * @brief Reads the wheel-base board identity straps.
 *
 * Samples RD1, RD12, RD13, RE9, and RE8 into a five-bit identity value after two instruction
 * settling cycles, releases the temporary inputs, and decodes the board variant and options.
 *
 * @return Decoded board identity.
 */
BoardIdentity platform_board_identity_read(void) {
    enable_mode_inputs();
    __builtin_nop();
    __builtin_nop();

    uint8_t mode_bits = PORTDbits.RD1 | (uint8_t)PORTDbits.RD12 << 1 |
                        (uint8_t)PORTDbits.RD13 << 2 | (uint8_t)PORTEbits.RE9 << 3 |
                        (uint8_t)PORTEbits.RE8 << 4;

    disable_mode_inputs();
    return board_identity_decode(mode_bits);
}
