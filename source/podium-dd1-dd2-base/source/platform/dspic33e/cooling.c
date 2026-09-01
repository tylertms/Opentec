#include "platform/cooling.h"

#include <stdbool.h>
#include <stdint.h>
#include <xc.h>

#include "cooling/pwm.h"
#include "platform/time.h"

/**
 * @brief Fan PWM and tachometer timing configuration values.
 */
enum {
    FAN_PWM_PERIOD = 3192,                        /**< Fan PWM period count. */
    FAN_PWM_REGISTER_PERIOD = FAN_PWM_PERIOD - 1, /**< Fan PWM period-register value. */
    FAN_CAPTURE_INTERVAL_MS = 50,       /**< Interval between alternating fan capture windows. */
    FAN_CAPTURE_INTERRUPT_PRIORITY = 4, /**< Fan capture interrupt priority. */
};

/**
 * @brief Internal state for one fan tachometer capture channel.
 */
typedef struct {
    volatile uint32_t previous; /**< Previous captured edge timestamp. */
    volatile uint32_t current;  /**< Current captured edge timestamp. */
    volatile bool ready;        /**< True when a result awaits foreground retrieval. */
    volatile bool present;      /**< True when the capture window received a tachometer signal. */
    volatile bool active;       /**< True when a capture window has completed with an edge pair. */
} FanCaptureState;

/**
 * @brief Internal capture state for the primary and secondary fans.
 */
static FanCaptureState captures[2];

/**
 * @brief True when fan PWM compare polarity is inverted.
 */
static bool pwm_inverted;

/**
 * @brief Fan whose capture window is serviced next.
 */
static PlatformFan next_fan;

/**
 * @brief Deadline for the next alternating fan capture service.
 */
static uint32_t next_capture_ms;

/**
 * @brief Configures both fan PWM channels.
 *
 * Drives the primary fan from OC5 on RF12 and the secondary fan from OC1 on RF13. Each channel
 * uses a 3192-count self-synchronized PWM period.
 *
 */
static void configure_pwm(void) {
    LATFbits.LATF12 = 0;
    LATFbits.LATF13 = 0;
    TRISFbits.TRISF12 = 0;
    TRISFbits.TRISF13 = 0;

    OC5CON1 = 0;
    OC5CON2 = 0;
    OC5R = 0;
    OC5RS = FAN_PWM_REGISTER_PERIOD;
    OC5CON1bits.OCTSEL = 7;
    OC5CON1bits.OCM = 6;
    OC5CON2bits.SYNCSEL = 0x1f;

    OC1CON1 = 0;
    OC1CON2 = 0;
    OC1R = 0;
    OC1RS = FAN_PWM_REGISTER_PERIOD;
    OC1CON1bits.OCTSEL = 7;
    OC1CON1bits.OCM = 6;
    OC1CON2bits.SYNCSEL = 0x1f;
}

/**
 * @brief Configures the primary fan tachometer channel.
 *
 * Couples IC1 and IC2 into a triggered 32-bit capture pair on RA1 and enables the IC1 interrupt
 * at priority four.
 *
 */
static void configure_primary_capture(void) {
    TRISAbits.TRISA1 = 1;
    IC1CON1 = 0;
    IC2CON1 = 0;
    IC1CON2 = 0;
    IC2CON2 = 0;
    IC1CON1bits.ICTSEL = 7;
    IC2CON1bits.ICTSEL = 7;
    IC1CON1bits.ICI = 1;
    IC2CON1bits.ICI = 1;
    IC1CON2bits.IC32 = 1;
    IC2CON2bits.IC32 = 1;
    IC1CON2bits.ICTRIG = 1;
    IC2CON2bits.ICTRIG = 1;
    IPC0bits.IC1IP = FAN_CAPTURE_INTERRUPT_PRIORITY;
    IFS0bits.IC1IF = 0;
    IEC0bits.IC1IE = 1;
}

/**
 * @brief Configures the secondary fan tachometer channel.
 *
 * Couples IC3 and IC4 into a triggered 32-bit capture pair on RD14 and enables the IC3 interrupt
 * at priority four.
 *
 */
static void configure_secondary_capture(void) {
    TRISDbits.TRISD14 = 1;
    IC3CON1 = 0;
    IC4CON1 = 0;
    IC3CON2 = 0;
    IC4CON2 = 0;
    IC3CON1bits.ICTSEL = 7;
    IC4CON1bits.ICTSEL = 7;
    IC3CON1bits.ICI = 1;
    IC4CON1bits.ICI = 1;
    IC3CON2bits.IC32 = 1;
    IC4CON2bits.IC32 = 1;
    IC3CON2bits.ICTRIG = 1;
    IC4CON2bits.ICTRIG = 1;
    IPC9bits.IC3IP = FAN_CAPTURE_INTERRUPT_PRIORITY;
    IFS2bits.IC3IF = 0;
    IEC2bits.IC3IE = 1;
}

/**
 * @brief Starts a primary fan capture window.
 *
 * Triggers the IC1 and IC2 pair and enables capture on every rising edge.
 *
 */
static void arm_primary_capture(void) {
    IC1CON2bits.TRIGSTAT = 1;
    IC2CON2bits.TRIGSTAT = 1;
    IC1CON1bits.ICM = 3;
    IC2CON1bits.ICM = 3;
}

/**
 * @brief Starts a secondary fan capture window.
 *
 * Triggers the IC3 and IC4 pair and enables capture on every rising edge.
 *
 */
static void arm_secondary_capture(void) {
    IC3CON2bits.TRIGSTAT = 1;
    IC4CON2bits.TRIGSTAT = 1;
    IC3CON1bits.ICM = 3;
    IC4CON1bits.ICM = 3;
}

/**
 * @brief Configures both fan PWM outputs and their paired tachometer capture inputs.
 *
 * Resets the two capture states, selects the PWM polarity, and schedules the first primary fan
 * capture after 50 milliseconds.
 *
 * @param[in] inverted_pwm True when increasing duty requires a decreasing compare value.
 */
void platform_cooling_init(bool inverted_pwm) {
    captures[PLATFORM_FAN_PRIMARY] = (FanCaptureState){0};
    captures[PLATFORM_FAN_SECONDARY] = (FanCaptureState){0};
    pwm_inverted = inverted_pwm;
    next_fan = PLATFORM_FAN_PRIMARY;
    next_capture_ms = platform_time_ms() + FAN_CAPTURE_INTERVAL_MS;
    configure_pwm();
    configure_primary_capture();
    configure_secondary_capture();
}

/**
 * @brief Applies clamped duty percentages to the two fan PWM outputs.
 *
 * Converts each requested duty to the configured PWM polarity and applies the inactive level when
 * the outputs are disabled.
 *
 * @param[in] primary_percent Primary output duty from 0 through 100 percent.
 * @param[in] secondary_percent Secondary output duty from 0 through 100 percent.
 * @param[in] outputs_disabled True to force both outputs to their inactive compare value.
 */
void platform_cooling_set_duty(uint16_t primary_percent, uint16_t secondary_percent,
                               bool outputs_disabled) {
    OC5R = fan_pwm_compare(primary_percent, pwm_inverted, outputs_disabled);
    OC1R = fan_pwm_compare(secondary_percent, pwm_inverted, outputs_disabled);
}

/**
 * @brief Alternately checks and rearms one fan tachometer capture every 50 milliseconds.
 *
 * Publishes a missing result when the preceding capture window produced no pair, then starts the
 * next window. Each fan is therefore sampled once every 100 milliseconds.
 *
 * @param[in] now_ms Current monotonic time in milliseconds.
 */
void platform_cooling_service(uint32_t now_ms) {
    if (!platform_time_reached(now_ms, next_capture_ms)) {
        return;
    }

    FanCaptureState *capture = &captures[next_fan];
    if (!capture->active) {
        capture->present = false;
        capture->ready = true;
    }
    capture->active = false;

    if (next_fan == PLATFORM_FAN_PRIMARY) {
        arm_primary_capture();
        next_fan = PLATFORM_FAN_SECONDARY;
    } else {
        arm_secondary_capture();
        next_fan = PLATFORM_FAN_PRIMARY;
    }
    next_capture_ms = now_ms + FAN_CAPTURE_INTERVAL_MS;
}

/**
 * @brief Consumes the newest completed or missing tachometer result for one fan.
 *
 * Copies a pending result to the caller and clears its ready state. Invalid channels and output
 * pointers are rejected without changing either capture state.
 *
 * @param[in] fan Fan tachometer channel to inspect.
 * @param[out] tachometer Consecutive timestamps and signal-presence state.
 * @return True when a new capture result was consumed.
 */
bool platform_cooling_take_tachometer(PlatformFan fan, PlatformFanTachometer *tachometer) {
    if ((fan != PLATFORM_FAN_PRIMARY && fan != PLATFORM_FAN_SECONDARY) || tachometer == 0) {
        return false;
    }

    FanCaptureState *state = &captures[fan];
    if (!state->ready) {
        return false;
    }

    tachometer->previous_capture = state->previous;
    tachometer->current_capture = state->current;
    tachometer->present = state->present;
    state->ready = false;
    return true;
}

/**
 * @brief Captures two consecutive primary input timestamps and stops the paired capture units.
 *
 * Combines two low/high word pairs from IC1 and IC2 into consecutive 32-bit timestamps, publishes
 * the result, and marks the primary input active.
 *
 */
void __attribute__((interrupt, no_auto_psv)) _IC1Interrupt(void) {
    FanCaptureState *capture = &captures[PLATFORM_FAN_PRIMARY];

    IFS0bits.IC1IF = 0;
    uint16_t previous_low = IC1BUF;
    uint16_t previous_high = IC2BUF;
    uint16_t current_low = IC1BUF;
    uint16_t current_high = IC2BUF;
    capture->previous = (uint32_t)previous_low | (uint32_t)previous_high << 16;
    capture->current = (uint32_t)current_low | (uint32_t)current_high << 16;
    IC1CON2bits.TRIGSTAT = 0;
    IC2CON2bits.TRIGSTAT = 0;
    IC1CON1bits.ICM = 0;
    IC2CON1bits.ICM = 0;
    capture->ready = true;
    capture->present = true;
    capture->active = true;
}

/**
 * @brief Captures two consecutive secondary input timestamps and stops the paired capture units.
 *
 * Combines two low/high word pairs from IC3 and IC4 into consecutive 32-bit timestamps, publishes
 * the result, and marks the secondary input active.
 *
 */
void __attribute__((interrupt, no_auto_psv)) _IC3Interrupt(void) {
    FanCaptureState *capture = &captures[PLATFORM_FAN_SECONDARY];

    IFS2bits.IC3IF = 0;
    uint16_t previous_low = IC3BUF;
    uint16_t previous_high = IC4BUF;
    uint16_t current_low = IC3BUF;
    uint16_t current_high = IC4BUF;
    capture->previous = (uint32_t)previous_low | (uint32_t)previous_high << 16;
    capture->current = (uint32_t)current_low | (uint32_t)current_high << 16;
    IC3CON2bits.TRIGSTAT = 0;
    IC4CON2bits.TRIGSTAT = 0;
    IC3CON1bits.ICM = 0;
    IC4CON1bits.ICM = 0;
    capture->ready = true;
    capture->present = true;
    capture->active = true;
}
