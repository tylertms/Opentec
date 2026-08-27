#ifndef OPENTEC_BASE_WHEEL_STATUS_H
#define OPENTEC_BASE_WHEEL_STATUS_H

#include <stdbool.h>
#include <stdint.h>

#include "wheel/transport_frame.h"

enum { WHEEL_STATUS_RESPONSE_SIZE = 15 };

typedef struct {
    uint8_t status_high;
    uint8_t status_low;
    uint16_t accessory_value;
    uint32_t runtime_seconds;
    uint32_t runtime_counter;
    uint8_t trailing_status;
    bool marker_acknowledged;
} WheelStatus;

bool wheel_status_decode(WheelStatus *status, const WheelTransportFrame *response);

#endif
