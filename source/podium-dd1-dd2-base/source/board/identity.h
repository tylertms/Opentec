#ifndef OPENTEC_BASE_BOARD_IDENTITY_H
#define OPENTEC_BASE_BOARD_IDENTITY_H

#include <stdint.h>

/**
 * @brief Supported wheel-base hardware variants.
 *
 * The decoded board variant selects the product-specific identity exposed by higher-level
 * protocols.
 */
typedef enum {
    BOARD_VARIANT_DD1, /**< DD1 wheel-base hardware. */
    BOARD_VARIANT_DD2, /**< DD2 wheel-base hardware. */
} BoardVariant;

/**
 * @brief Decoded board-identity strap information.
 *
 * Contains the selected hardware variant, the dedicated hardware option bit, and the retained
 * low five strap bits used by subsystem-specific decisions.
 */
typedef struct {
    BoardVariant variant;    /**< Decoded DD1 or DD2 hardware variant. */
    uint8_t hardware_option; /**< Strap bit one exposed as a hardware option. */
    uint8_t mode_bits;       /**< Low five sampled strap bits. */
} BoardIdentity;

/**
 * @brief Decodes a board-identity strap value.
 *
 * Selects the board variant from bit zero, exposes bit one as hardware_option, and masks the
 * retained mode_bits value to five bits.
 *
 * @param[in] mode_bits Sampled board-identity strap value.
 * @return Decoded board identity.
 */
BoardIdentity board_identity_decode(uint8_t mode_bits);

#endif
