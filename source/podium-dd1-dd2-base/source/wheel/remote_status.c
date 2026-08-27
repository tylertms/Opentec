#include "wheel/remote_status.h"

static uint16_t read_u16(const uint8_t *data) {
    return (uint16_t)data[0] | (uint16_t)((uint16_t)data[1] << 8);
}

static uint32_t read_u32(const uint8_t *data) {
    return (uint32_t)data[0] | (uint32_t)data[1] << 8 | (uint32_t)data[2] << 16 |
           (uint32_t)data[3] << 24;
}

bool wheel_remote_status_begin(wheel_remote_exchange *exchange, uint8_t sequence,
                               bool request_reset) {
    static const uint8_t reset_marker = 0xaa;
    const uint8_t *request = request_reset ? &reset_marker : NULL;
    size_t request_length = request_reset ? 1 : 0;

    return wheel_remote_exchange_start(exchange, WQR_PAYLOAD_STATUS, sequence, request,
                                       request_length);
}

bool wheel_remote_status_decode(wheel_remote_status *status, const uint8_t *payload,
                                size_t payload_length) {
    if (status == NULL || payload == NULL || payload_length < WHEEL_REMOTE_STATUS_SIZE) {
        return false;
    }

    status->firmware_revision = payload[0];
    status->hardware_revision = payload[1];
    status->temperature_c = (int16_t)read_u16(payload + 2);
    status->uptime_seconds = read_u32(payload + 4);
    status->communication_errors = read_u32(payload + 8);
    status->link_state = payload[12];
    status->transfer_mode = payload[13];
    status->reset_acknowledged = payload[14] == 0xaa;
    return true;
}

bool wheel_remote_status_finish(const wheel_remote_exchange *exchange,
                                wheel_remote_status *status) {
    const uint8_t *response = wheel_remote_exchange_response(exchange);

    return response != NULL &&
           wheel_remote_status_decode(status, response,
                                      wheel_remote_exchange_response_length(exchange));
}
