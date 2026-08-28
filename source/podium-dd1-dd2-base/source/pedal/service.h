#ifndef OPENTEC_BASE_PEDAL_SERVICE_H
#define OPENTEC_BASE_PEDAL_SERVICE_H

#include <stdbool.h>
#include <stdint.h>

#include "pedal/analog.h"
#include "pedal/frame.h"
#include "pedal/input.h"
#include "pedal/protocol.h"

typedef enum {
    PEDAL_SERVICE_DETECT_REQUEST,
    PEDAL_SERVICE_DETECT_RESPONSE,
    PEDAL_SERVICE_PROTOCOL_REQUEST,
    PEDAL_SERVICE_PROTOCOL_RESPONSE,
    PEDAL_SERVICE_SELECT_PROTOCOL,
    PEDAL_SERVICE_LEGACY_REQUEST,
    PEDAL_SERVICE_LEGACY_RESPONSE,
    PEDAL_SERVICE_V3_SWITCH_WAIT,
    PEDAL_SERVICE_V3_START,
    PEDAL_SERVICE_V3_STREAM,
    PEDAL_SERVICE_RECONNECT_WAIT,
    PEDAL_SERVICE_ANALOG,
    PEDAL_SERVICE_V4_UNSUPPORTED,
} PedalServicePhase;

typedef struct {
    PedalInput input;
    PedalV3State v3;
    PedalAnalog analog;
    PedalServicePhase phase;
    PedalDevice device;
    uint32_t deadline_ms;
    uint32_t next_status_ms;
    uint32_t next_input_command_ms;
    uint32_t next_keepalive_ms;
    PedalFrame transmit_frame;
    PedalFrame receive_frame;
    uint8_t frame_buffer[PEDAL_FRAME_SIZE];
    uint8_t response;
    uint8_t brake_force_percent;
    uint8_t startup_frame_count;
    uint8_t pending_control;
    uint8_t input_command[PEDAL_INPUT_AXIS_COUNT];
    uint8_t configuration_brake_force;
    uint8_t legacy_retries[PEDAL_LEGACY_CHANNEL_COUNT];
    PedalLegacyChannel legacy_channel;
    PedalProtocolStatus protocol_status;
    PedalProtocolStatus transmitted_status;
    uint16_t analog_samples[PEDAL_INPUT_AXIS_COUNT];
    bool analog_samples_ready;
    bool connected;
    bool auxiliary_locked;
    bool input_command_pending;
    bool configuration_pending;
    bool configuration_reset_pending;
    bool recovery_handshake;
    bool status_transmitted;
} PedalService;

void pedal_service_init(PedalService *service);
void pedal_service_set_analog_samples(PedalService *service,
                                      const uint16_t samples[PEDAL_INPUT_AXIS_COUNT]);
void pedal_service_set_brake_force(PedalService *service, uint8_t force_percent);
void pedal_service_set_auxiliary_locked(PedalService *service, bool locked);
void pedal_service_set_protocol_status(PedalService *service, const PedalProtocolStatus *status);
void pedal_service_request_control(PedalService *service, PedalV3Control control);
void pedal_service_request_input_command(PedalService *service,
                                         const uint8_t values[PEDAL_INPUT_AXIS_COUNT]);
void pedal_service_request_configuration(PedalService *service, uint8_t brake_force, bool reset);
void pedal_service_run(PedalService *service, uint32_t now_ms);
const PedalInput *pedal_service_input(const PedalService *service);
const PedalV3State *pedal_service_v3_state(const PedalService *service);

#endif
