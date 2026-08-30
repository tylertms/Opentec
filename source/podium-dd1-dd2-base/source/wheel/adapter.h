#ifndef OPENTEC_BASE_WHEEL_ADAPTER_H
#define OPENTEC_BASE_WHEEL_ADAPTER_H

#include <stdbool.h>
#include <stdint.h>

enum { WHEEL_ADAPTER_ROTARY_COUNT = 3 };

/** @brief Logical buttons, axes, selectors, mode, and queued motion from an attached adapter. */
typedef struct {
    uint8_t buttons[3];
    uint8_t axes[2];
    uint8_t rotary_positions[WHEEL_ADAPTER_ROTARY_COUNT];
    uint8_t firmware_version[3];
    uint8_t information[4];
    uint16_t mode;
    int8_t primary_delta;
    uint8_t profile_flags;
    bool connected;
    bool buttons_active;
} WheelAdapterInput;

#endif
