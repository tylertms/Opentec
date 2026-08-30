#ifndef OPENTEC_BASE_WHEEL_ACCESSORY_SERVICE_H
#define OPENTEC_BASE_WHEEL_ACCESSORY_SERVICE_H

#include <stdbool.h>
#include <stdint.h>

#include "transfer/command.h"
#include "wheel/accessory.h"

/** @brief Attached wheel accessory polling and identity state. */
typedef struct {
    WheelAccessory accessory;
    uint8_t version_bytes[4];
    uint8_t status_byte;
    uint8_t accessory_type_byte;
    bool version_stage;
    bool accessory_type_stage;
    bool request_pending;
} WheelAccessoryService;

void wheel_accessory_service_init(WheelAccessoryService *service);
void wheel_accessory_service_run(WheelAccessoryService *service, CommandTransport *transport);
const WheelAccessory *wheel_accessory_service_identity(const WheelAccessoryService *service);

#endif
