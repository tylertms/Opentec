#ifndef OPENTEC_TEST_GMCLIB_H
#define OPENTEC_TEST_GMCLIB_H

#include <gflib.h>

typedef struct {
    frac16_t f16A;
    frac16_t f16B;
    frac16_t f16C;
} GMCLIB_3COOR_T_F16;

typedef struct {
    frac16_t f16Alpha;
    frac16_t f16Beta;
} GMCLIB_2COOR_ALBE_T_F16;

typedef struct {
    frac16_t f16D;
    frac16_t f16Q;
} GMCLIB_2COOR_DQ_T_F16;

typedef struct {
    frac16_t f16Sin;
    frac16_t f16Cos;
} GMCLIB_2COOR_SINCOS_T_F16;

static inline void GMCLIB_Clark_F16(const GMCLIB_3COOR_T_F16 *input,
                                    GMCLIB_2COOR_ALBE_T_F16 *output) {
    output->f16Alpha = input->f16A;
    output->f16Beta = input->f16B;
}

static inline void GMCLIB_Park_F16(const GMCLIB_2COOR_ALBE_T_F16 *input,
                                   const GMCLIB_2COOR_SINCOS_T_F16 *angle,
                                   GMCLIB_2COOR_DQ_T_F16 *output) {
    (void)angle;
    output->f16D = input->f16Alpha;
    output->f16Q = input->f16Beta;
}

static inline void GMCLIB_ParkInv_F16(const GMCLIB_2COOR_DQ_T_F16 *input,
                                      const GMCLIB_2COOR_SINCOS_T_F16 *angle,
                                      GMCLIB_2COOR_ALBE_T_F16 *output) {
    (void)angle;
    output->f16Alpha = input->f16D;
    output->f16Beta = input->f16Q;
}

static inline void GMCLIB_ElimDcBusRipFOC_F16(frac16_t bus_voltage,
                                              const GMCLIB_2COOR_ALBE_T_F16 *input,
                                              GMCLIB_2COOR_ALBE_T_F16 *output) {
    (void)bus_voltage;
    *output = *input;
}

static inline uint16_t GMCLIB_SvmStd_F16(const GMCLIB_2COOR_ALBE_T_F16 *input,
                                         GMCLIB_3COOR_T_F16 *output) {
    (void)input;
    *output = (GMCLIB_3COOR_T_F16){0};
    return 0U;
}

#endif
