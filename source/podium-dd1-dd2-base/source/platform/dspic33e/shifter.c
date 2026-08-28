#include "platform/shifter.h"

#include <stdint.h>
#include <xc.h>

#include "shifter/input.h"

static ShifterInputMode primary_mode;
static ShifterInputMode secondary_mode;

static ShifterInputMode sample_primary_mode(void) {
    uint8_t high_samples = PORTDbits.RD0;
    high_samples += PORTDbits.RD0;
    high_samples += PORTDbits.RD0;
    return high_samples == 3 ? SHIFTER_INPUT_SEQUENTIAL : SHIFTER_INPUT_H_PATTERN;
}

static ShifterInputMode sample_secondary_mode(void) {
    uint8_t high_samples = PORTDbits.RD11;
    high_samples += PORTDbits.RD11;
    high_samples += PORTDbits.RD11;
    return high_samples == 3 ? SHIFTER_INPUT_SEQUENTIAL : SHIFTER_INPUT_H_PATTERN;
}

static void configure_primary_mode(ShifterInputMode mode) {
    bool analog = mode == SHIFTER_INPUT_H_PATTERN;
    ANSELBbits.ANSB9 = analog;
    ANSELBbits.ANSB10 = analog;
    primary_mode = mode;
}

static void configure_secondary_mode(ShifterInputMode mode) {
    bool analog = mode == SHIFTER_INPUT_H_PATTERN;
    ANSELBbits.ANSB11 = analog;
    ANSELBbits.ANSB12 = analog;
    secondary_mode = mode;
}

void platform_shifter_init(void) {
    TRISDbits.TRISD0 = 1;
    TRISDbits.TRISD11 = 1;
    TRISBbits.TRISB9 = 1;
    TRISBbits.TRISB10 = 1;
    TRISBbits.TRISB11 = 1;
    TRISBbits.TRISB12 = 1;
    configure_primary_mode(sample_primary_mode());
    configure_secondary_mode(sample_secondary_mode());
}

void platform_shifter_read(ShifterInputState *state) {
    ShifterInputMode next_primary_mode = sample_primary_mode();
    ShifterInputMode next_secondary_mode = sample_secondary_mode();
    if (next_primary_mode != primary_mode) {
        configure_primary_mode(next_primary_mode);
    }
    if (next_secondary_mode != secondary_mode) {
        configure_secondary_mode(next_secondary_mode);
    }

    state->primary_mode = primary_mode;
    state->secondary_mode = secondary_mode;
    state->primary_transition =
        primary_mode == SHIFTER_INPUT_SEQUENTIAL && (PORTBbits.RB9 == 0 || PORTBbits.RB10 == 0);
    state->secondary_transition =
        secondary_mode == SHIFTER_INPUT_SEQUENTIAL && (PORTBbits.RB11 == 0 || PORTBbits.RB12 == 0);
}
