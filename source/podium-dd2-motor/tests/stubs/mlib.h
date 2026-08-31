#ifndef VELOCITY_HOST_STUB_MLIB_H
#define VELOCITY_HOST_STUB_MLIB_H

#include <stdint.h>

typedef int16_t frac16_t;
typedef int32_t frac32_t;

static inline frac16_t MLIB_AbsSat_F16(frac16_t value) {
    return value == INT16_MIN ? INT16_MAX : value < 0 ? (frac16_t)-value : value;
}

static inline frac32_t MLIB_Conv_F32s(frac16_t value) { return (frac32_t)value * 65536; }

static inline frac16_t MLIB_Conv_F16l(frac32_t value) { return (frac16_t)(value / 65536); }

static inline frac16_t MLIB_SubSat_F16(frac16_t left, frac16_t right) {
    int32_t difference = (int32_t)left - right;
    return difference > INT16_MAX   ? INT16_MAX
           : difference < INT16_MIN ? INT16_MIN
                                    : (frac16_t)difference;
}

#endif
