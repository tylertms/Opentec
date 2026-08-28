#ifndef OPENTEC_BASE_WHEEL_SERVICE_H
#define OPENTEC_BASE_WHEEL_SERVICE_H

#include <stdbool.h>
#include <stdint.h>

#include "serial/service.h"
#include "wheel/display_output.h"
#include "wheel/protocol.h"

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
    SerialService *transport;
    WheelProtocol protocol;
    WheelDisplayOutput display_output;
    uint8_t request[SERIAL_PACKET_MAX_PAYLOAD_SIZE];
    uint8_t button_banks[WHEEL_BUTTON_BANK_COUNT];
    uint8_t scan_samples[WHEEL_SCAN_SAMPLE_DEPTH][WHEEL_BUTTON_BANK_COUNT];
    uint32_t protocol_deadline_ms;
    uint8_t scan_phase;
    uint8_t scan_sample_index;
    WheelServiceRequest request_kind;
    bool protocol_deadline_active;
} WheelService;

void wheel_service_init(WheelService *service, SerialService *transport);
void wheel_service_run(WheelService *service, uint32_t now_ms, bool start_allowed);
void wheel_service_set_display_output(WheelService *service, const WheelDisplayOutput *output);
void wheel_service_set_crc_adapter(WheelService *service, const WheelPacketCrcAdapter *adapter);
bool wheel_service_queue_remote_tuning_response(WheelService *service,
                                                const RemoteTuningResponse *response);
void wheel_service_apply_output_report(WheelService *service, const uint8_t *arguments,
                                       bool display_blink_active);
void wheel_service_queue_report_seventeen(
    WheelService *service, const uint8_t payload[WHEEL_OUTPUT_REPORT_SEVENTEEN_SIZE]);
const uint8_t *wheel_service_buttons(const WheelService *service);
uint8_t wheel_service_axis_limit(const WheelService *service);
const uint8_t *wheel_service_clutch_paddles(const WheelService *service);
bool wheel_service_axis_values(const WheelService *service, uint16_t values[2]);
bool wheel_service_controls(const WheelService *service, uint8_t controls[8]);
int8_t wheel_service_take_encoder_delta(WheelService *service);
bool wheel_service_acknowledgement_input_active(const WheelService *service);
bool wheel_service_adapter_connected(const WheelService *service);
uint8_t wheel_service_mode(const WheelService *service);
WheelProtocolPhase wheel_service_protocol_phase(const WheelService *service);

#endif
