#ifndef OPENTEC_BASE_PEDAL_SERVICE_H
#define OPENTEC_BASE_PEDAL_SERVICE_H

#include <stdbool.h>
#include <stdint.h>

#include "pedal/frame.h"
#include "pedal/input.h"

typedef enum {
    PEDAL_SERVICE_DETECT_REQUEST,
    PEDAL_SERVICE_DETECT_RESPONSE,
    PEDAL_SERVICE_PROTOCOL_REQUEST,
    PEDAL_SERVICE_PROTOCOL_RESPONSE,
    PEDAL_SERVICE_V3_START,
    PEDAL_SERVICE_V3_STREAM,
    PEDAL_SERVICE_RECONNECT_WAIT,
    PEDAL_SERVICE_V4_UNSUPPORTED,
} PedalServicePhase;

typedef enum {
    PEDAL_DEVICE_NONE,
    PEDAL_DEVICE_V3 = 0x1a,
    PEDAL_DEVICE_V4 = 0x2a,
} PedalDevice;

typedef struct {
    PedalInput input;
    PedalServicePhase phase;
    PedalDevice device;
    uint32_t deadline_ms;
    uint32_t next_status_ms;
    PedalFrame transmit_frame;
    PedalFrame receive_frame;
    uint8_t frame_buffer[PEDAL_FRAME_SIZE];
    uint8_t response;
    bool connected;
} PedalService;

void pedal_service_init(PedalService *service);
void pedal_service_run(PedalService *service, uint32_t now_ms);
const PedalInput *pedal_service_input(const PedalService *service);

#endif
