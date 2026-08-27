#include "board/identity.h"

#include <stdint.h>

BoardIdentity board_identity_decode(uint8_t mode_bits) {
    BoardIdentity identity = {
        .variant = (mode_bits & 1) != 0 ? BOARD_VARIANT_DD1 : BOARD_VARIANT_DD2,
        .hardware_option = (mode_bits >> 1) & 1,
        .mode_bits = mode_bits & 0x1f,
    };
    return identity;
}
