#ifndef OPENTEC_BASE_WHEEL_REMOTE_STATUS_H
#define OPENTEC_BASE_WHEEL_REMOTE_STATUS_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "wheel/remote_exchange.h"

enum { WHEEL_REMOTE_STATUS_SIZE = 15 };

typedef enum {
    WHEEL_REMOTE_LINK_WAITING = 1,
    WHEEL_REMOTE_LINK_DETECTED = 2,
    WHEEL_REMOTE_LINK_READY = 4
} wheel_remote_link_state;

typedef struct {
    uint32_t uptime_seconds;
    uint32_t communication_errors;
    int16_t temperature_c;
    uint8_t firmware_revision;
    uint8_t hardware_revision;
    uint8_t link_state;
    uint8_t transfer_mode;
    bool reset_acknowledged;
} wheel_remote_status;

bool wheel_remote_status_begin(wheel_remote_exchange *exchange, uint8_t sequence,
                               bool request_reset);
bool wheel_remote_status_decode(wheel_remote_status *status, const uint8_t *payload,
                                size_t payload_length);
bool wheel_remote_status_finish(const wheel_remote_exchange *exchange, wheel_remote_status *status);

#endif
