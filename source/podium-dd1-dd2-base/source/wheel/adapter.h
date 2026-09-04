#ifndef OPENTEC_BASE_WHEEL_ADAPTER_H
#define OPENTEC_BASE_WHEEL_ADAPTER_H

#include <stdbool.h>
#include <stdint.h>

/**
 * @brief Number of rotary response bytes retained from an attached adapter.
 *
 * The standard endpoint returns one packed primary and secondary position byte. The extended
 * endpoint adds one auxiliary position byte.
 */
enum { WHEEL_ADAPTER_ROTARY_COUNT = 2 /**< Maximum retained rotary response length. */ };

/**
 * @brief Logical buttons, axes, selectors, mode, and queued motion from an attached adapter.
 *
 * The command service updates this state from adapter responses, and packet encoders consume it
 * when the selected wheel protocol exposes adapter input.
 */
typedef struct {
    uint8_t buttons[3]; /**< Three raw adapter button bytes. */
    uint8_t axes[2];    /**< Two raw adapter axis bytes in adapter report order. */
    uint8_t rotary_positions[WHEEL_ADAPTER_ROTARY_COUNT]; /**< Packed rotary response bytes. */
    uint8_t firmware_version[3]; /**< Three firmware-version bytes from the extended probe. */
    uint8_t information[4];      /**< Four information bytes from the extended probe. */
    uint16_t mode; /**< Selected adapter endpoint mode, zero for standard or one for extended. */
    int8_t primary_delta;  /**< Pending signed primary rotary steps to merge into wheel input. */
    uint8_t profile_flags; /**< Latest adapter status and profile-request flags. */
    bool connected;        /**< True when the latest endpoint probe reports an attached adapter. */
    bool buttons_active;   /**< True when any retained adapter button bit is active. */
} WheelAdapterInput;

#endif
