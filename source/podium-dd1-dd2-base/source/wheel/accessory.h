#ifndef OPENTEC_BASE_WHEEL_ACCESSORY_H
#define OPENTEC_BASE_WHEEL_ACCESSORY_H

#include <stdbool.h>
#include <stdint.h>

/** @brief Protocol kind detected from the attached wheel accessory processor. */
typedef enum {
    WHEEL_ACCESSORY_DISCONNECTED,
    WHEEL_ACCESSORY_LEGACY,
    WHEEL_ACCESSORY_STANDARD,
    WHEEL_ACCESSORY_EXTENDED,
} WheelAccessoryKind;

/** @brief Attached wheel accessory identity and protocol state. */
typedef struct {
    uint32_t version;
    int8_t initial_status;
    uint8_t model;
    WheelAccessoryKind kind;
} WheelAccessory;

void wheel_accessory_init(WheelAccessory *accessory);
bool wheel_accessory_apply_probe(WheelAccessory *accessory, int8_t status, uint32_t version);
uint8_t wheel_accessory_transfer_code(const WheelAccessory *accessory);
uint8_t wheel_accessory_mode_code(const WheelAccessory *accessory);
uint8_t wheel_accessory_mode_flags(const WheelAccessory *accessory);

#endif
