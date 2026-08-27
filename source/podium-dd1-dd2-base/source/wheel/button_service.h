#ifndef OPENTEC_BASE_WHEEL_BUTTON_SERVICE_H
#define OPENTEC_BASE_WHEEL_BUTTON_SERVICE_H

#include <stdbool.h>
#include <stdint.h>

#include "wheel/status.h"
#include "wheel/transport_service.h"

enum { WHEEL_BUTTON_BANK_COUNT = 3 };

typedef enum {
    WHEEL_SERVICE_REQUEST_NONE,
    WHEEL_SERVICE_REQUEST_STATUS,
    WHEEL_SERVICE_REQUEST_BUTTONS,
} WheelServiceRequest;

typedef struct {
    WheelTransportService transport;
    uint8_t request[WHEEL_TRANSPORT_PAYLOAD_SIZE];
    uint8_t button_banks[WHEEL_BUTTON_BANK_COUNT];
    uint8_t phase;
    WheelStatus status;
    WheelServiceRequest request_kind;
    bool status_ready;
    bool status_requested;
} WheelButtonService;

void wheel_button_service_init(WheelButtonService *service);
void wheel_button_service_run(WheelButtonService *service, uint32_t now_ms);
const uint8_t *wheel_button_service_buttons(const WheelButtonService *service);
const WheelStatus *wheel_button_service_status(const WheelButtonService *service);

#endif
