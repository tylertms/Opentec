#ifndef OPENTEC_TEST_MLIB_H
#define OPENTEC_TEST_MLIB_H

#include <gflib.h>
#include <limits.h>
#include <stdint.h>

static inline frac32_t MLIB_Conv_F32s(frac16_t value) {
    return (frac32_t)((uint32_t)(uint16_t)value << 16U);
}

static inline frac16_t MLIB_Conv_F16l(frac32_t value) {
    return (frac16_t)(uint16_t)((uint32_t)value >> 16U);
}

static inline frac16_t MLIB_SubSat_F16(frac16_t minuend, frac16_t subtrahend) {
    int32_t result = (int32_t)minuend - subtrahend;
    if (result > INT16_MAX) {
        return INT16_MAX;
    }
    if (result < INT16_MIN) {
        return INT16_MIN;
    }
    return (frac16_t)result;
}

#endif
