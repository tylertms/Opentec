#ifndef OPENTEC_BASE_WHEEL_TRANSPORT_SERVICE_H
#define OPENTEC_BASE_WHEEL_TRANSPORT_SERVICE_H

#include <stdbool.h>
#include <stdint.h>

#include "wheel/transport_frame.h"

typedef enum {
    WHEEL_TRANSPORT_IDLE,
    WHEEL_TRANSPORT_PENDING,
    WHEEL_TRANSPORT_SUCCEEDED,
    WHEEL_TRANSPORT_FAILED,
} WheelTransportStatus;

typedef struct {
    WheelTransportFrame request;
    WheelTransportFrame response;
    uint8_t encoded[WHEEL_TRANSPORT_FRAME_SIZE];
    uint32_t deadline_ms;
    uint8_t node;
    WheelTransportStatus status;
} WheelTransportService;

void wheel_transport_service_init(WheelTransportService *service);
bool wheel_transport_service_start(WheelTransportService *service, uint8_t command,
                                   const uint8_t *data, uint8_t length, uint32_t now_ms);
void wheel_transport_service_run(WheelTransportService *service, uint32_t now_ms);
const WheelTransportFrame *wheel_transport_service_response(const WheelTransportService *service);

#endif
