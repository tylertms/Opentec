#ifndef OPENTEC_MOTOR_STORAGE_H
#define OPENTEC_MOTOR_STORAGE_H

#include <stdbool.h>

#include "common/motor/encoder_calibration.h"
#include "fsl_common.h"

status_t motor_calibration_storage_initialize(void);
bool motor_calibration_storage_load(MotorEncoderCalibrationRecord *record);
status_t motor_calibration_storage_erase(void);
status_t motor_calibration_storage_program(const MotorEncoderCalibrationRecord *record);

#endif
