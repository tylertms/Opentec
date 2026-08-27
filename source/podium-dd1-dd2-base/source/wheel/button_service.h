#ifndef OPENTEC_BASE_WHEEL_BUTTON_SERVICE_H
#define OPENTEC_BASE_WHEEL_BUTTON_SERVICE_H

#include <stdint.h>

#include "wheel/transport_service.h"

enum { WHEEL_BUTTON_BANK_COUNT = 3 };

typedef struct {
    WheelTransportService transport;
    uint8_t request[WHEEL_TRANSPORT_PAYLOAD_SIZE];
    uint8_t button_banks[WHEEL_BUTTON_BANK_COUNT];
    uint8_t phase;
} WheelButtonService;

void wheel_button_service_init(WheelButtonService *service);
void wheel_button_service_run(WheelButtonService *service, uint32_t now_ms);
const uint8_t *wheel_button_service_buttons(const WheelButtonService *service);

#endif
