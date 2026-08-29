#ifndef OPENTEC_MOTOR_SERVICE_H
#define OPENTEC_MOTOR_SERVICE_H

#include <stdbool.h>
#include <stdint.h>

#define MOTOR_SERVICE_COUNTDOWN_COUNT 5U

typedef enum {
    kMotorCountdownTelemetry,
    kMotorCountdownStartupInterlockB,
    kMotorCountdownStartupRamp,
    kMotorCountdownEncoderIndex,
    kMotorCountdownStartupInterlockA,
} MotorCountdownIndex;

typedef struct {
    uint16_t ticks;
    uint16_t active;
} MotorCountdown;

typedef struct {
    MotorCountdown countdowns[MOTOR_SERVICE_COUNTDOWN_COUNT];
    uint16_t velocity_controller_ticks;
} MotorServiceTiming;

void motor_service_timing_initialize(MotorServiceTiming *timing);
bool motor_service_timing_tick(MotorServiceTiming *timing);

#endif
