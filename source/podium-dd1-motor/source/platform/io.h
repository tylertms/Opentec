#ifndef OPENTEC_MOTOR_IO_H
#define OPENTEC_MOTOR_IO_H

#include <fsl_device_registers.h>
#include "rtcesl.h"
#include <stdbool.h>

#include "motor/calibration.h"

/**
 * @brief Holds one synchronized current and DC-bus ADC sample.
 */
typedef struct {
    GMCLIB_3COOR_T_F16 phase_current; /**< Reconstructed three-phase current sample. */
    frac16_t dc_bus_voltage;           /**< Scaled DC-bus voltage sample. */
} MotorAdcSample;

/**
 * @brief Reads synchronized phase-current and DC-bus ADC results.
 *
 * Calibrated phase offsets are removed and the third phase current is reconstructed from the first
 * two when both current conversions are complete. The bus-voltage sample is updated on every call.
 *
 * @param[in] adc0 ADC0 register block containing phase-B and DC-bus results.
 * @param[in] adc1 ADC1 register block containing the phase-A result.
 * @param[in] offsets Calibrated phase-current zero offsets.
 * @param[out] sample Destination for converted phase currents and DC-bus voltage.
 * @return True when both phase-current conversions are complete.
 */
bool motor_adc_read(ADC_Type *adc0, ADC_Type *adc1, const MotorCurrentOffsets *offsets,
                    MotorAdcSample *sample);

/**
 * @brief Converts normalized SVM duties to complementary FTM compare values.
 *
 * The duty values receive the motor-specific scale and offset before the six phase compare
 * registers are written and the next PWM synchronization point is committed.
 *
 * @param[out] ftm FTM register block receiving the three motor-phase compare values.
 * @param[in,out] duty SVM duties to scale and convert.
 */
void motor_pwm_write(FTM_Type *ftm, GMCLIB_3COOR_T_F16 *duty);

#endif
