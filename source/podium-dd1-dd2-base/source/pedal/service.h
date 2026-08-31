#ifndef OPENTEC_BASE_PEDAL_SERVICE_H
#define OPENTEC_BASE_PEDAL_SERVICE_H

#include <stdbool.h>
#include <stdint.h>

#include "pedal/adjustment_probe.h"
#include "pedal/analog.h"
#include "pedal/frame.h"
#include "pedal/input.h"
#include "pedal/protocol.h"
#include "pedal/protocol_command.h"
#include "pedal/transfer_queue.h"
#include "pedal/v4_tuning.h"
#include "transfer/session.h"

enum { PEDAL_ALTERNATE_BRAKE_FORCE_NO_UPDATE = UINT8_MAX };

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
    PEDAL_SERVICE_V4_START,
    PEDAL_SERVICE_V4_STREAM,
    PEDAL_SERVICE_RECONNECT_WAIT,
    PEDAL_SERVICE_ANALOG,
} PedalServicePhase;

typedef enum {
    PEDAL_V4_PHASE_STATUS,
    PEDAL_V4_PHASE_SELECT,
    PEDAL_V4_PHASE_ADJUSTMENT_START,
    PEDAL_V4_PHASE_ADJUSTMENT_WAIT,
    PEDAL_V4_PHASE_HOST_TRANSFER,
    PEDAL_V4_PHASE_BRAKE_FORCE,
    PEDAL_V4_PHASE_CLUTCH_CURVE,
    PEDAL_V4_PHASE_BRAKE_CURVE,
    PEDAL_V4_PHASE_THROTTLE_CURVE,
} PedalV4Phase;

/**
 * @brief Identifies who requested the active V4 pedal adjustment.
 *
 * Separates host operations, whose responses return over USB, from wheel-button operations that
 * only update the local display.
 */
typedef enum {
    PEDAL_ADJUSTMENT_SOURCE_NONE,
    PEDAL_ADJUSTMENT_SOURCE_HOST,
    PEDAL_ADJUSTMENT_SOURCE_BUTTON,
} PedalAdjustmentSource;

/**
 * @brief Identifies the operation that produced a host pedal response.
 *
 * Associates generic responses with their retained request so request completion follows the final
 * USB fragment.
 */
typedef enum {
    PEDAL_TRANSFER_RESPONSE_NONE,
    PEDAL_TRANSFER_RESPONSE_ADJUSTMENT,
    PEDAL_TRANSFER_RESPONSE_HOST_REQUEST,
} PedalTransferResponseSource;

/**
 * @brief Stores one completed host pedal response.
 *
 * Keeps the payload and its length together while the USB service forwards the response.
 */
typedef struct {
    uint8_t data[TRANSFER_FRAME_MAX_SEND_PAYLOAD_SIZE];
    uint8_t length;
    PedalTransferResponseSource source;
} PedalTransferResponse;

typedef struct {
    PedalInput input;
    PedalV3State v3;
    TransferSession v4;
    PedalAnalog analog;
    PedalServicePhase phase;
    PedalDevice device;
    uint32_t deadline_ms;
    uint32_t next_status_ms;
    uint32_t next_input_command_ms;
    uint32_t next_keepalive_ms;
    uint32_t v4_response_deadline_ms;
    uint32_t v4_operation_deadline_ms;
    uint32_t next_v4_keepalive_ms;
    uint32_t next_service_ms;
    uint32_t clock_ms;
    PedalFrame transmit_frame;
    PedalFrame receive_frame;
    uint8_t frame_buffer[PEDAL_FRAME_SIZE];
    uint8_t transfer_buffer[TRANSFER_FRAME_MAX_RECEIVED_SIZE];
    uint8_t v4_tuning_request[PEDAL_V4_TUNING_REQUEST_SIZE];
    uint8_t response;
    uint8_t brake_force_percent;
    uint8_t startup_frame_count;
    uint8_t pending_control;
    uint8_t input_command[PEDAL_INPUT_AXIS_COUNT];
    uint8_t configuration_brake_force;
    uint8_t v4_tuning_pending;
    uint8_t v4_sent_value;
    uint8_t remote_auxiliary;
    uint8_t auxiliary_override;
    uint8_t legacy_retries[PEDAL_LEGACY_CHANNEL_COUNT];
    PedalLegacyChannel legacy_channel;
    PedalProtocolStatus protocol_status;
    PedalProtocolStatus transmitted_status;
    PedalV4Tuning v4_tuning;
    PedalTransferQueue host_transfer_queue;
    PedalTransferResponse transfer_response;
    PedalAdjustmentDisplay adjustment_display;
    PedalAdjustmentSource adjustment_source;
    PedalV4Phase v4_phase;
    uint16_t analog_samples[PEDAL_INPUT_AXIS_COUNT];
    bool analog_samples_ready;
    bool connected;
    bool digital_activity;
    bool auxiliary_override_active;
    bool input_command_pending;
    bool configuration_pending;
    bool configuration_reset_pending;
    bool recovery_handshake;
    bool status_transmitted;
    bool v4_request_active;
    bool v4_response_received;
    bool alternate_brake_force_received;
    bool host_adjustment_pending;
    bool button_adjustment_pending;
    bool adjustment_display_pending;
} PedalService;

void pedal_service_init(PedalService *service);
void pedal_service_request_startup(PedalService *service);
void pedal_service_set_analog_samples(PedalService *service,
                                      const uint16_t samples[PEDAL_INPUT_AXIS_COUNT]);
void pedal_service_set_brake_force(PedalService *service, uint8_t force_percent);
void pedal_service_set_v4_tuning(PedalService *service, PedalV4Tuning tuning);
void pedal_service_set_auxiliary_override(PedalService *service, bool active, uint8_t value);
bool pedal_service_calibration_active(const PedalService *service);
bool pedal_service_auxiliary_automatic_calibration(const PedalService *service);
void pedal_service_set_protocol_status(PedalService *service, const PedalProtocolStatus *status);
void pedal_service_reset_protocol_status(PedalService *service);
void pedal_service_set_brake_indicator_selector(PedalService *service, uint8_t selector);
bool pedal_service_legacy_transport_active(const PedalService *service);
bool pedal_service_handshake_active(const PedalService *service);
void pedal_service_apply_protocol_command(PedalService *service,
                                          const PedalProtocolCommand *command);
void pedal_service_request_control(PedalService *service, PedalV3Control control);
bool pedal_service_control_pending(const PedalService *service);
void pedal_service_request_input_command(PedalService *service,
                                         const uint8_t values[PEDAL_INPUT_AXIS_COUNT]);
void pedal_service_request_configuration(PedalService *service, uint8_t brake_force, bool reset);
bool pedal_service_adjustment_available(const PedalService *service);
void pedal_service_request_host_adjustment(PedalService *service);
void pedal_service_request_button_adjustment(PedalService *service);
void pedal_service_queue_host_transfer(PedalService *service, const uint8_t *data, uint8_t length);
const PedalTransferResponse *pedal_service_transfer_response(const PedalService *service);
void pedal_service_release_transfer_response(PedalService *service);
PedalAdjustmentDisplay pedal_service_take_adjustment_display(PedalService *service);
uint8_t pedal_service_take_alternate_brake_force(PedalService *service);
void pedal_service_run(PedalService *service, uint32_t now_ms);
const PedalInput *pedal_service_input(const PedalService *service);
const PedalV3State *pedal_service_v3_state(const PedalService *service);

#endif
