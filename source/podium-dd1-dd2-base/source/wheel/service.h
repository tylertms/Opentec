#ifndef OPENTEC_BASE_WHEEL_SERVICE_H
#define OPENTEC_BASE_WHEEL_SERVICE_H

#include <stdbool.h>
#include <stdint.h>

#include "wheel/display_output.h"
#include "wheel/protocol.h"
#include "wheel/transport_service.h"

enum {
    WHEEL_BUTTON_BANK_COUNT = 3,
    WHEEL_SCAN_SAMPLE_DEPTH = 3,
};

typedef enum {
    WHEEL_SERVICE_REQUEST_NONE,
    WHEEL_SERVICE_REQUEST_PROTOCOL,
    WHEEL_SERVICE_REQUEST_BUTTONS,
} WheelServiceRequest;

typedef struct {
    WheelTransportService transport;
    WheelProtocol protocol;
    WheelDisplayOutput display_output;
    uint8_t request[WHEEL_TRANSPORT_PAYLOAD_SIZE];
    uint8_t button_banks[WHEEL_BUTTON_BANK_COUNT];
    uint8_t scan_samples[WHEEL_SCAN_SAMPLE_DEPTH][WHEEL_BUTTON_BANK_COUNT];
    uint32_t protocol_deadline_ms;
    uint8_t scan_phase;
    uint8_t scan_sample_index;
    WheelServiceRequest request_kind;
    bool protocol_deadline_active;
} WheelService;

void wheel_service_init(WheelService *service);
void wheel_service_run(WheelService *service, uint32_t now_ms);
void wheel_service_set_display_output(WheelService *service, const WheelDisplayOutput *output);
const uint8_t *wheel_service_buttons(const WheelService *service);
bool wheel_service_acknowledgement_input_active(const WheelService *service);
uint8_t wheel_service_mode(const WheelService *service);
WheelProtocolPhase wheel_service_protocol_phase(const WheelService *service);

#endif
