#ifndef OPENTEC_BASE_PEDAL_BRAKE_INDICATOR_H
#define OPENTEC_BASE_PEDAL_BRAKE_INDICATOR_H

#include <stdbool.h>
#include <stdint.h>

/**
 * @brief Special values used by brake-indicator configuration and updates.
 */
enum {
    PEDAL_BRAKE_INDICATOR_DISABLED = 101, /**< Configuration value that disables indication. */
    PEDAL_BRAKE_INDICATOR_NO_UPDATE =
        0x66, /**< Return value meaning the selector need not change. */
};

/**
 * @brief Stores the requested brake-indicator selector.
 */
typedef struct {
    uint8_t selector; /**< Current brake-indicator selector requested from the pedal controller. */
} PedalBrakeIndicator;

/**
 * @brief Initializes brake-indicator state.
 *
 * Starts with no active selector.
 *
 * @param[out] indicator Brake-indicator state to initialize.
 */
void pedal_brake_indicator_init(PedalBrakeIndicator *indicator);

/**
 * @brief Updates the brake-indicator selector.
 *
 * Selects the transport-specific indicator when brake position reaches the configured level and
 * emits a release when indication is no longer active.
 *
 * @param[in,out] indicator Brake-indicator state to update.
 * @param[in] level Activation level from zero through 100, or the disabled value.
 * @param[in] brake_position Calibrated sixteen-bit brake position.
 * @param[in] legacy_transport True when legacy pedal transport is active.
 * @return Selector to apply, or PEDAL_BRAKE_INDICATOR_NO_UPDATE when unchanged.
 */
uint8_t pedal_brake_indicator_update(PedalBrakeIndicator *indicator, uint8_t level,
                                     uint16_t brake_position, bool legacy_transport);

#endif
