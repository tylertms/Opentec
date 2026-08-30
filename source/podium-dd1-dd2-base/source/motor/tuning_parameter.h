#ifndef OPENTEC_BASE_MOTOR_TUNING_PARAMETER_H
#define OPENTEC_BASE_MOTOR_TUNING_PARAMETER_H

#include <stdint.h>

#include "profile/tuning.h"

typedef enum {
    MOTOR_TUNING_SENSITIVITY,
    MOTOR_TUNING_FORCE_FEEDBACK_STRENGTH,
    MOTOR_TUNING_FORCE_FEEDBACK_SCALE,
    MOTOR_TUNING_NATURAL_DAMPER,
    MOTOR_TUNING_NATURAL_FRICTION,
    MOTOR_TUNING_NATURAL_INERTIA,
    MOTOR_TUNING_INTERPOLATION_FILTER,
    MOTOR_TUNING_FORCE_EFFECT_INTENSITY,
    MOTOR_TUNING_FORCE_EFFECT_STRENGTH,
    MOTOR_TUNING_SPRING_EFFECT_STRENGTH,
    MOTOR_TUNING_DAMPER_EFFECT_STRENGTH,
    MOTOR_TUNING_PARAMETER_COUNT,
} MotorTuningParameter;

typedef struct {
    uint16_t automatic_rotation_degrees;
    uint8_t ramp_percent;
    uint8_t strength_percent;
    uint8_t xbox_mode;
    uint8_t calibration_active;
    uint8_t extended_parameters;
} MotorTuningContext;

typedef struct {
    uint8_t address;
    uint8_t length;
    uint8_t data[2];
} MotorParameterWrite;

uint8_t motor_tuning_parameter_encode(MotorTuningParameter parameter, const TuningProfile *profile,
                                      const MotorTuningContext *context,
                                      MotorParameterWrite *write);

#endif
