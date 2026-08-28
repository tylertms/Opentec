#ifndef OPENTEC_BASE_WHEEL_STATUS_SERVICE_H
#define OPENTEC_BASE_WHEEL_STATUS_SERVICE_H

#include <stdbool.h>
#include <stdint.h>

#include "serial/service.h"

typedef struct {
    uint8_t status_high;
    uint8_t status_low;
    uint16_t accessory_value;
    uint32_t runtime_seconds;
    uint32_t runtime_counter;
    uint8_t trailing_status;
} WheelStatusSnapshot;

typedef struct {
    SerialService *transport;
    WheelStatusSnapshot snapshot;
    uint32_t next_poll_ms;
    uint8_t request_marker;
    bool poll_deadline_active;
    bool marked_response_ready;
} WheelStatusService;

void wheel_status_service_init(WheelStatusService *service, SerialService *transport);
void wheel_status_service_run(WheelStatusService *service, uint32_t now_ms, bool start_allowed);
void wheel_status_service_mark_next_request(WheelStatusService *service);
bool wheel_status_service_take_marked_response(WheelStatusService *service);
const WheelStatusSnapshot *wheel_status_service_snapshot(const WheelStatusService *service);

#endif
