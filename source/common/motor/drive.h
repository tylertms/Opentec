#ifndef OPENTEC_MOTOR_DRIVE_H
#define OPENTEC_MOTOR_DRIVE_H

#include <stdbool.h>
#include <stdint.h>

typedef struct {
    int16_t primary_current;
    int16_t secondary_current;
    uint16_t controller_coefficient;
    uint16_t controller_scale;
} MotorDriveCommand;

MotorDriveCommand motor_drive_command_resolve(bool positive, uint32_t primary, int32_t secondary,
                                              uint8_t normal_output_percent, bool full_torque,
                                              bool reduced_controller, bool secondary_disabled);

#endif
