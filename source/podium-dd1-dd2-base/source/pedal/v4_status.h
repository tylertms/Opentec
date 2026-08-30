#ifndef PODIUM_DD1_DD2_BASE_PEDAL_V4_STATUS_H
#define PODIUM_DD1_DD2_BASE_PEDAL_V4_STATUS_H

#include <stdint.h>

enum {
    PEDAL_V4_STATUS_AXIS_COUNT = 3,
    PEDAL_V4_STATUS_ENVELOPE_SIZE = 25,
    PEDAL_V4_STATUS_REQUEST_SIZE = 21,
};

const uint8_t *pedal_v4_status_request(void);
void pedal_v4_status_parse(const uint8_t *data, uint16_t length,
                           uint16_t axes[PEDAL_V4_STATUS_AXIS_COUNT]);

#endif
