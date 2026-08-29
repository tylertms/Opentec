#ifndef OPENTEC_TEST_GDFLIB_H
#define OPENTEC_TEST_GDFLIB_H

#include <gflib.h>

typedef struct {
    frac32_t f32B0;
    frac32_t f32B1;
    frac32_t f32A1;
} GDFLIB_FILTER_IIR1_COEFF_T_F32;

typedef struct {
    GDFLIB_FILTER_IIR1_COEFF_T_F32 sFltCoeff;
} GDFLIB_FILTER_IIR1_T_F32;

static inline void GDFLIB_FilterIIR1Init_F16(GDFLIB_FILTER_IIR1_T_F32 *filter) { (void)filter; }

static inline frac16_t GDFLIB_FilterIIR1_F16(frac16_t input, GDFLIB_FILTER_IIR1_T_F32 *filter) {
    (void)filter;
    return input;
}

#endif
