#ifndef OPENTEC_MOTOR_PARAMETER_H
#define OPENTEC_MOTOR_PARAMETER_H

#include <stdbool.h>
#include <stdint.h>

#define MOTOR_PARAMETER_COUNT 64U
#define MOTOR_PARAMETER_RESPONSE_SIZE 5U
#define MOTOR_PARAMETER_REQUEST_SIZE 5U

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
void motor_parameter_response_encode(const MotorParameterResponse *response,
                                     uint8_t output[MOTOR_PARAMETER_RESPONSE_SIZE]);
bool motor_parameter_request_apply(MotorParameterBank *bank,
                                   const uint8_t input[MOTOR_PARAMETER_REQUEST_SIZE],
                                   uint8_t received_size, bool *control_settings_changed);

#endif
