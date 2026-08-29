#include "board/identity.h"

#include <stdint.h>

/**
 * @brief Decodes the board-identity strap value.
 *
 * Selects DD1 when bit zero is set and DD2 otherwise, exposes bit one as the hardware option, and
 * retains the low five strap bits for subsystem-specific decisions.
 *
 * @param[in] mode_bits Sampled board-identity strap value.
 * @return Decoded board identity.
 */
BoardIdentity board_identity_decode(uint8_t mode_bits) {
    BoardIdentity identity = {
        .variant = (mode_bits & 1) != 0 ? BOARD_VARIANT_DD1 : BOARD_VARIANT_DD2,
        .hardware_option = (mode_bits >> 1) & 1,
        .mode_bits = mode_bits & 0x1f,
    };
    return identity;
}
