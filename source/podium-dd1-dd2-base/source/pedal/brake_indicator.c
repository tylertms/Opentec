#include "pedal/brake_indicator.h"

#include <stdbool.h>
#include <stdint.h>

enum {
    PEDAL_BRAKE_INDICATOR_FRAMED_SELECTOR = 0x3f,
    PEDAL_BRAKE_INDICATOR_LEGACY_SELECTOR = 0xff,
    PEDAL_BRAKE_INDICATOR_LEVEL_MAX = 100,
};

/**
 * @brief Converts a brake-indicator percentage to an eight-bit pedal threshold.
 *
 * Scales levels from zero through 100 across the complete high byte of the calibrated brake axis.
 *
 * @param[in] level Brake-indicator activation level.
 * @return Eight-bit brake-axis threshold.
 */
static uint8_t activation_threshold(uint8_t level) {
    return (uint8_t)((uint16_t)level * UINT8_MAX / PEDAL_BRAKE_INDICATOR_LEVEL_MAX);
}

/**
 * @brief Initializes brake-indicator activation state.
 *
 * Starts with the pedal protocol selector released.
 *
 * @param[out] indicator Brake-indicator state to initialize.
 */
void pedal_brake_indicator_init(PedalBrakeIndicator *indicator) { indicator->selector = 0; }

/**
 * @brief Updates the pedal brake-indicator selector from the current brake position.
 *
 * Activates at or above the configured threshold, selects the transport-specific protocol value,
 * and emits one release when the brake drops below the threshold or the setting is disabled.
 * Active selectors are returned on every update so an intervening protocol command cannot mask
 * the brake indicator.
 *
 * @param[in,out] indicator Retained selector state.
 * @param[in] level Brake-indicator activation level, with 101 disabling the behavior.
 * @param[in] brake_position Calibrated sixteen-bit brake position.
 * @param[in] legacy_transport True while the byte-oriented legacy pedal protocol is active.
 * @return Pedal protocol selector to apply, or 0x66 when no update is needed.
 */
uint8_t pedal_brake_indicator_update(PedalBrakeIndicator *indicator, uint8_t level,
                                     uint16_t brake_position, bool legacy_transport) {
    uint8_t desired = 0;
    if (level <= PEDAL_BRAKE_INDICATOR_LEVEL_MAX &&
        (uint8_t)(brake_position >> 8) >= activation_threshold(level)) {
        desired = legacy_transport ? PEDAL_BRAKE_INDICATOR_LEGACY_SELECTOR
                                   : PEDAL_BRAKE_INDICATOR_FRAMED_SELECTOR;
    }

    bool apply = desired != 0 || desired != indicator->selector;
    indicator->selector = desired;
    return apply ? desired : PEDAL_BRAKE_INDICATOR_NO_UPDATE;
}
