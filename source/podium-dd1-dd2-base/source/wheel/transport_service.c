#include "wheel/transport_service.h"

#include <stdbool.h>
#include <stdint.h>

#include "platform/time.h"
#include "platform/wheel_link.h"
#include "wheel/transport_frame.h"

enum {
    WHEEL_TRANSPORT_COMMAND_MASK = 0x0f,
    WHEEL_TRANSPORT_TIMEOUT_MS = 10,
};

void wheel_transport_service_init(WheelTransportService *service) {
    service->deadline_ms = 0;
    service->node = 0;
    service->status = WHEEL_TRANSPORT_IDLE;
}

bool wheel_transport_service_start(WheelTransportService *service, uint8_t command,
                                   const uint8_t *data, uint8_t length, uint32_t now_ms) {
    if (service->status == WHEEL_TRANSPORT_PENDING || length > WHEEL_TRANSPORT_PAYLOAD_SIZE ||
        (data == 0 && length != 0)) {
        return false;
    }

    service->request.command = command & WHEEL_TRANSPORT_COMMAND_MASK;
    service->request.node = service->node;
    service->request.length = length;
    for (uint8_t index = 0; index < length; index++) {
        service->request.data[index] = data[index];
    }
    if (wheel_transport_frame_encode(&service->request, service->encoded) !=
            WHEEL_TRANSPORT_FRAME_VALID ||
        !platform_wheel_link_start(service->encoded)) {
        return false;
    }

    service->deadline_ms = now_ms + WHEEL_TRANSPORT_TIMEOUT_MS;
    service->status = WHEEL_TRANSPORT_PENDING;
    return true;
}

void wheel_transport_service_run(WheelTransportService *service, uint32_t now_ms) {
    if (service->status != WHEEL_TRANSPORT_PENDING) {
        return;
    }
    if (platform_wheel_link_take_received(service->encoded)) {
        if (wheel_transport_frame_decode(service->encoded, &service->response) ==
                WHEEL_TRANSPORT_FRAME_VALID &&
            (service->response.command & WHEEL_TRANSPORT_COMMAND_MASK) ==
                service->request.command) {
            service->node++;
            service->status = WHEEL_TRANSPORT_SUCCEEDED;
        } else {
            service->status = WHEEL_TRANSPORT_FAILED;
        }
        return;
    }
    if (platform_time_reached(now_ms, service->deadline_ms)) {
        platform_wheel_link_reset();
        service->status = WHEEL_TRANSPORT_FAILED;
    }
}

const WheelTransportFrame *wheel_transport_service_response(const WheelTransportService *service) {
    return service->status == WHEEL_TRANSPORT_SUCCEEDED ? &service->response : 0;
}
