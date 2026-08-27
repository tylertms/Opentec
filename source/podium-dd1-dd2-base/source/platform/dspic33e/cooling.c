#include "platform/cooling.h"

#include <stdbool.h>
#include <stdint.h>
#include <xc.h>

#include "platform/time.h"

enum {
    FAN_PWM_PERIOD = 3192,
    FAN_PWM_REGISTER_PERIOD = FAN_PWM_PERIOD - 1,
    FAN_CAPTURE_INTERVAL_MS = 50,
    FAN_CAPTURE_INTERRUPT_PRIORITY = 5,
};

typedef struct {
    volatile uint32_t previous;
    volatile uint32_t current;
    volatile bool ready;
    volatile bool present;
    volatile bool captured;
    bool armed;
} FanCaptureState;

static FanCaptureState captures[2];
static bool pwm_active_low;
static PlatformFan next_fan;
static uint32_t next_capture_ms;

static uint16_t duty_compare(uint8_t percent) {
    if (percent > 100) {
        percent = 100;
    }
    uint16_t active_counts = (uint32_t)percent * FAN_PWM_PERIOD / 100;
    return pwm_active_low ? FAN_PWM_PERIOD - active_counts : active_counts;
}

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

static void arm_primary_capture(void) {
    IC1CON2bits.TRIGSTAT = 1;
    IC2CON2bits.TRIGSTAT = 1;
    IC1CON1bits.ICM = 3;
    IC2CON1bits.ICM = 3;
}

static void arm_secondary_capture(void) {
    IC3CON2bits.TRIGSTAT = 1;
    IC4CON2bits.TRIGSTAT = 1;
    IC3CON1bits.ICM = 3;
    IC4CON1bits.ICM = 3;
}

static void set_capture_interrupt(PlatformFan fan, bool enabled) {
    if (fan == PLATFORM_FAN_PRIMARY) {
        IEC0bits.IC1IE = enabled;
    } else {
        IEC2bits.IC3IE = enabled;
    }
}

void platform_cooling_init(bool active_low) {
    captures[PLATFORM_FAN_PRIMARY] = (FanCaptureState){0};
    captures[PLATFORM_FAN_SECONDARY] = (FanCaptureState){0};
    pwm_active_low = active_low;
    next_fan = PLATFORM_FAN_PRIMARY;
    next_capture_ms = 0;
    configure_pwm();
    configure_primary_capture();
    configure_secondary_capture();
    platform_cooling_set_duty(100, 100);
}

void platform_cooling_set_duty(uint8_t primary_percent, uint8_t secondary_percent) {
    OC5R = duty_compare(primary_percent);
    OC1R = duty_compare(secondary_percent);
}

void platform_cooling_service(uint32_t now_ms) {
    if (!platform_time_reached(now_ms, next_capture_ms)) {
        return;
    }

    PlatformFan fan = next_fan;
    set_capture_interrupt(fan, false);
    FanCaptureState *capture = &captures[fan];
    if (capture->armed && !capture->captured) {
        capture->present = false;
        capture->ready = true;
    }
    capture->armed = true;
    capture->captured = false;

    if (fan == PLATFORM_FAN_PRIMARY) {
        arm_primary_capture();
        next_fan = PLATFORM_FAN_SECONDARY;
    } else {
        arm_secondary_capture();
        next_fan = PLATFORM_FAN_PRIMARY;
    }
    set_capture_interrupt(fan, true);
    next_capture_ms = now_ms + FAN_CAPTURE_INTERVAL_MS;
}

bool platform_cooling_take_tachometer(PlatformFan fan, PlatformFanTachometer *tachometer) {
    if (fan != PLATFORM_FAN_PRIMARY && fan != PLATFORM_FAN_SECONDARY) {
        return false;
    }

    set_capture_interrupt(fan, false);

    FanCaptureState *capture = &captures[fan];
    bool ready = capture->ready;
    if (ready) {
        tachometer->previous_capture = capture->previous;
        tachometer->current_capture = capture->current;
        tachometer->present = capture->present;
        capture->ready = false;
    }

    set_capture_interrupt(fan, true);
    return ready;
}

void __attribute__((interrupt, no_auto_psv)) _IC1Interrupt(void) {
    uint16_t previous_low = IC1BUF;
    uint16_t previous_high = IC2BUF;
    uint16_t current_low = IC1BUF;
    uint16_t current_high = IC2BUF;
    FanCaptureState *capture = &captures[PLATFORM_FAN_PRIMARY];

    capture->previous = (uint32_t)previous_low | (uint32_t)previous_high << 16;
    capture->current = (uint32_t)current_low | (uint32_t)current_high << 16;
    capture->present = true;
    capture->captured = true;
    capture->ready = true;
    IC1CON2bits.TRIGSTAT = 0;
    IC2CON2bits.TRIGSTAT = 0;
    IC1CON1bits.ICM = 0;
    IC2CON1bits.ICM = 0;
    IFS0bits.IC1IF = 0;
}

void __attribute__((interrupt, no_auto_psv)) _IC3Interrupt(void) {
    uint16_t previous_low = IC3BUF;
    uint16_t previous_high = IC4BUF;
    uint16_t current_low = IC3BUF;
    uint16_t current_high = IC4BUF;
    FanCaptureState *capture = &captures[PLATFORM_FAN_SECONDARY];

    capture->previous = (uint32_t)previous_low | (uint32_t)previous_high << 16;
    capture->current = (uint32_t)current_low | (uint32_t)current_high << 16;
    capture->present = true;
    capture->captured = true;
    capture->ready = true;
    IC3CON2bits.TRIGSTAT = 0;
    IC4CON2bits.TRIGSTAT = 0;
    IC3CON1bits.ICM = 0;
    IC4CON1bits.ICM = 0;
    IFS2bits.IC3IF = 0;
}
