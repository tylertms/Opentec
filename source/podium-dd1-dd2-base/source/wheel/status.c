#include "wheel/status.h"

#include <stdbool.h>
#include <stdint.h>

#include "wheel/transport_frame.h"

enum { WHEEL_STATUS_MARKER = 0xaa };

static uint16_t read_u16(const uint8_t *data) { return (uint16_t)data[0] | (uint16_t)data[1] << 8; }

static uint32_t read_u32(const uint8_t *data) {
    return (uint32_t)data[0] | (uint32_t)data[1] << 8 | (uint32_t)data[2] << 16 |
           (uint32_t)data[3] << 24;
}

bool wheel_status_decode(WheelStatus *status, const WheelTransportFrame *response) {
    if (status == 0 || response == 0 || response->length < WHEEL_STATUS_RESPONSE_SIZE) {
        return false;
    }
    status->status_high = response->data[0];
    status->status_low = response->data[1];
    status->accessory_value = read_u16(response->data + 2);
    status->runtime_seconds = read_u32(response->data + 4);
    status->runtime_counter = read_u32(response->data + 8);
    status->trailing_status = response->data[12];
    status->marker_acknowledged = response->data[14] == WHEEL_STATUS_MARKER;
    return true;
}
