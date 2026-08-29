#ifndef OPENTEC_MOTOR_PARAMETER_H
#define OPENTEC_MOTOR_PARAMETER_H

#include <stdbool.h>
#include <stdint.h>

#define MOTOR_PARAMETER_COUNT 64U

typedef struct {
    uint32_t value;
    uint8_t width;
    bool writable;
} MotorParameter;

typedef struct {
    MotorParameter entries[MOTOR_PARAMETER_COUNT];
} MotorParameterBank;

typedef struct {
    uint32_t value;
    uint8_t width;
} MotorParameterResponse;

bool motor_parameter_read(const MotorParameterBank *bank, uint8_t index,
                          MotorParameterResponse *response);
bool motor_parameter_write(MotorParameterBank *bank, uint8_t index, uint32_t value, uint8_t width,
                           bool *control_settings_changed);

#endif
