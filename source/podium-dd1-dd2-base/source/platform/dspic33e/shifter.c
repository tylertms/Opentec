#include "platform/shifter.h"

#include <stdint.h>
#include <xc.h>

#include "shifter/input.h"

/**
 * @brief Currently configured primary shifter input mode.
 */
static ShifterInputMode primary_mode;

/**
 * @brief Currently configured secondary shifter input mode.
 */
static ShifterInputMode secondary_mode;

/**
 * @brief Samples the primary shifter identification input.
 *
 * Classifies the port as sequential only when all three reads of RD0 are high; any low sample
 * selects H-pattern mode.
 *
 * @return Detected primary shifter mode.
 */
static ShifterInputMode sample_primary_mode(void) {
    uint8_t high_samples = PORTDbits.RD0;
    high_samples += PORTDbits.RD0;
    high_samples += PORTDbits.RD0;
    return high_samples == 3 ? SHIFTER_INPUT_SEQUENTIAL : SHIFTER_INPUT_H_PATTERN;
}

/**
 * @brief Samples the secondary shifter identification input.
 *
 * Classifies the port as sequential only when all three reads of RD11 are high; any low sample
 * selects H-pattern mode.
 *
 * @return Detected secondary shifter mode.
 */
static ShifterInputMode sample_secondary_mode(void) {
    uint8_t high_samples = PORTDbits.RD11;
    high_samples += PORTDbits.RD11;
    high_samples += PORTDbits.RD11;
    return high_samples == 3 ? SHIFTER_INPUT_SEQUENTIAL : SHIFTER_INPUT_H_PATTERN;
}

/**
 * @brief Applies the primary shifter input mode.
 *
 * Enables analog input on RB9 and RB10 for an H-pattern shifter and selects digital input for a
 * sequential shifter.
 *
 * @param[in] mode Shifter mode to apply.
 */
static void configure_primary_mode(ShifterInputMode mode) {
    bool analog = mode == SHIFTER_INPUT_H_PATTERN;
    ANSELBbits.ANSB9 = analog;
    ANSELBbits.ANSB10 = analog;
    primary_mode = mode;
}

/**
 * @brief Applies the secondary shifter input mode.
 *
 * Enables analog input on RB11 and RB12 for an H-pattern shifter and selects digital input for a
 * sequential shifter.
 *
 * @param[in] mode Shifter mode to apply.
 */
static void configure_secondary_mode(ShifterInputMode mode) {
    bool analog = mode == SHIFTER_INPUT_H_PATTERN;
    ANSELBbits.ANSB11 = analog;
    ANSELBbits.ANSB12 = analog;
    secondary_mode = mode;
}

/**
 * @brief Initializes both shifter ports and detects their input modes.
 *
 * Configures the two identification pins and four shared analog or sequential inputs as inputs,
 * then applies the mode reported by three samples from each identification pin.
 */
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

/**
 * @brief Reads the connected shifter modes and sequential transition inputs.
 *
 * Reconfigures a port when its identification input changes. In sequential mode, either active-low
 * switch on that port produces its transition signal.
 *
 * @param[out] state Destination for both port modes and transition signals.
 */
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
