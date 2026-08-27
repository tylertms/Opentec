#ifndef OPENTEC_BASE_BOARD_IDENTITY_H
#define OPENTEC_BASE_BOARD_IDENTITY_H

#include <stdint.h>

typedef enum {
    BOARD_VARIANT_DD1,
    BOARD_VARIANT_DD2,
} BoardVariant;

typedef struct {
    BoardVariant variant;
    uint8_t hardware_option;
    uint8_t mode_bits;
} BoardIdentity;

BoardIdentity board_identity_decode(uint8_t mode_bits);

#endif
