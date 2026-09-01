#include "platform/io.h"

bool motor_adc_read(ADC_Type *adc0, ADC_Type *adc1, const MotorCurrentOffsets *offsets,
                    MotorAdcSample *sample) {
    bool currents_ready =
        (adc0->SC1[0] & ADC_SC1_COCO_MASK) != 0U && (adc1->SC1[0] & ADC_SC1_COCO_MASK) != 0U;

    if (currents_ready) {
        frac16_t phase_a_raw = (frac16_t)((frac16_t)adc1->R[0] - offsets->phase_a);
        frac16_t phase_b_raw = (frac16_t)((frac16_t)adc0->R[0] - offsets->phase_b);

        sample->phase_current.f16A = MLIB_ShLSat_F16(phase_a_raw, 4U);
        sample->phase_current.f16B = MLIB_ShLSat_F16(phase_b_raw, 4U);
        sample->phase_current.f16C =
            MLIB_Neg_F16(MLIB_AddSat_F16(sample->phase_current.f16A, sample->phase_current.f16B));
    }

    sample->dc_bus_voltage = MLIB_ShLSat_F16((frac16_t)adc0->R[1], 3U);
    return currents_ready;
}

void motor_pwm_write(FTM_Type *ftm, GMCLIB_3COOR_T_F16 *duty) {
    duty->f16A = MLIB_AddSat_F16(MLIB_Mul_F16(0x7eb8, duty->f16A), 0x00a3);
    duty->f16B = MLIB_AddSat_F16(MLIB_Mul_F16(0x7eb8, duty->f16B), 0x00a3);
    duty->f16C = MLIB_AddSat_F16(MLIB_Mul_F16(0x7eb8, duty->f16C), 0x00a3);

    uint16_t phase_a_count = (uint16_t)MLIB_Mul_F16(0x08ca, duty->f16A);
    uint16_t phase_b_count = (uint16_t)MLIB_Mul_F16(0x08ca, duty->f16B);
    uint16_t phase_c_count = (uint16_t)MLIB_Mul_F16(0x08ca, duty->f16C);

    ftm->CONTROLS[0].CnV = 0U - phase_a_count;
    ftm->CONTROLS[1].CnV = phase_a_count;
    ftm->CONTROLS[2].CnV = 0U - phase_b_count;
    ftm->CONTROLS[3].CnV = phase_b_count;
    ftm->CONTROLS[4].CnV = 0U - phase_c_count;
    ftm->CONTROLS[5].CnV = phase_c_count;
    ftm->PWMLOAD = FTM_PWMLOAD_LDOK_MASK;
}
