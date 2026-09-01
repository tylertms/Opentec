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

/**
 * @brief Sentinel returned when no new alternate brake-force report is available.
 */
enum { PEDAL_ALTERNATE_BRAKE_FORCE_NO_UPDATE = UINT8_MAX /**< No alternate brake-force update. */ };

/**
 * @brief Identifies the current pedal service phase.
 *
 * Phases cover discovery, protocol selection, transport operation, reconnection, and analog
 * fallback.
 */
typedef enum {
    PEDAL_SERVICE_DETECT_REQUEST,    /**< Detection request is ready to send. */
    PEDAL_SERVICE_DETECT_RESPONSE,   /**< Waiting for a device response. */
    PEDAL_SERVICE_PROTOCOL_REQUEST,  /**< Protocol request is ready to send. */
    PEDAL_SERVICE_PROTOCOL_RESPONSE, /**< Waiting for a protocol response. */
    PEDAL_SERVICE_SELECT_PROTOCOL,   /**< Selecting a transport from discovery responses. */
    PEDAL_SERVICE_LEGACY_REQUEST,    /**< Legacy channel request is ready to send. */
    PEDAL_SERVICE_LEGACY_RESPONSE,   /**< Waiting for a legacy channel response. */
    PEDAL_SERVICE_V3_SWITCH_WAIT,    /**< Waiting for the V3 baud-rate switch. */
    PEDAL_SERVICE_V3_START,          /**< V3 handshake is ready to send. */
    PEDAL_SERVICE_V3_STREAM,         /**< Processing framed V3 reports. */
    PEDAL_SERVICE_V4_START,          /**< Initializing the V4 transfer session. */
    PEDAL_SERVICE_V4_STREAM,         /**< Processing V4 transfers and status polls. */
    PEDAL_SERVICE_RECONNECT_WAIT,    /**< Waiting before restarting discovery. */
    PEDAL_SERVICE_ANALOG,            /**< Publishing local analog pedal input. */
} PedalServicePhase;

/**
 * @brief Identifies the current V4 operation phase.
 *
 * Tuning phases select individual settings; other phases select status, adjustment, or host
 * transfers.
 */
typedef enum {
    PEDAL_V4_PHASE_STATUS,           /**< Periodic V4 status request. */
    PEDAL_V4_PHASE_SELECT,           /**< Select the next pending V4 operation. */
    PEDAL_V4_PHASE_ADJUSTMENT_START, /**< Send the adjustment probe request. */
    PEDAL_V4_PHASE_ADJUSTMENT_WAIT,  /**< Wait for asynchronous adjustment completion. */
    PEDAL_V4_PHASE_HOST_TRANSFER,    /**< Send a queued host transfer. */
    PEDAL_V4_PHASE_BRAKE_FORCE,      /**< Send the pending brake-force setting. */
    PEDAL_V4_PHASE_CLUTCH_CURVE,     /**< Send the pending clutch-curve setting. */
    PEDAL_V4_PHASE_BRAKE_CURVE,      /**< Send the pending brake-curve setting. */
    PEDAL_V4_PHASE_THROTTLE_CURVE,   /**< Send the pending throttle-curve setting. */
} PedalV4Phase;

/**
 * @brief Identifies who requested the active V4 pedal adjustment.
 *
 * Separates host operations, whose responses return over USB, from wheel-button operations that
 * only update the local display.
 */
typedef enum {
    PEDAL_ADJUSTMENT_SOURCE_NONE,   /**< No adjustment operation is active. */
    PEDAL_ADJUSTMENT_SOURCE_HOST,   /**< Adjustment requested by the host. */
    PEDAL_ADJUSTMENT_SOURCE_BUTTON, /**< Adjustment requested by a wheel button. */
} PedalAdjustmentSource;

/**
 * @brief Identifies the operation that produced a host pedal response.
 *
 * Associates generic responses with their retained request so request completion follows the final
 * USB fragment.
 */
typedef enum {
    PEDAL_TRANSFER_RESPONSE_NONE,         /**< Response has no tracked source. */
    PEDAL_TRANSFER_RESPONSE_ADJUSTMENT,   /**< Response came from an adjustment operation. */
    PEDAL_TRANSFER_RESPONSE_HOST_REQUEST, /**< Response came from a queued host request. */
} PedalTransferResponseSource;

/**
 * @brief Stores one completed host pedal response.
 *
 * Keeps the payload and its length together while the USB service forwards the response.
 */
typedef struct {
    uint8_t data[TRANSFER_FRAME_MAX_SEND_PAYLOAD_SIZE]; /**< Completed response payload. */
    uint8_t length;                     /**< Number of valid response bytes in data. */
    PedalTransferResponseSource source; /**< Operation that produced the response. */
} PedalTransferResponse;

/**
 * @brief Retains all pedal transports, protocol state, timing, and published input.
 *
 * The service state is owned by the firmware loop and is advanced by pedal_service_run.
 */
typedef struct {
    PedalInput input;                 /**< Published pedal and auxiliary input. */
    PedalV3State v3;                  /**< Retained V3 report state. */
    TransferSession v4;               /**< V4 transfer-session state. */
    PedalAnalog analog;               /**< Local analog calibration state. */
    PedalServicePhase phase;          /**< Current service phase. */
    PedalDevice device;               /**< Detected pedal device identity. */
    uint32_t deadline_ms;             /**< Current phase timeout or reconnect deadline. */
    uint32_t next_status_ms;          /**< Next status request deadline for the active transport. */
    uint32_t next_input_command_ms;   /**< Next V3 input-command deadline. */
    uint32_t next_keepalive_ms;       /**< Next V3 calibration keepalive deadline. */
    uint32_t v4_response_deadline_ms; /**< V4 initial-response deadline. */
    uint32_t v4_operation_deadline_ms;      /**< V4 asynchronous-operation deadline. */
    uint32_t next_v4_keepalive_ms;          /**< Next V4 adjustment keepalive deadline. */
    uint32_t next_service_ms;               /**< Next one-millisecond service deadline. */
    uint32_t clock_ms;                      /**< Latest monotonic time supplied to the service. */
    PedalFrame transmit_frame;              /**< V3 frame being prepared for transmission. */
    PedalFrame receive_frame;               /**< Most recently decoded V3 frame. */
    uint8_t frame_buffer[PEDAL_FRAME_SIZE]; /**< Encoded V3 frame transport buffer. */
    uint8_t transfer_buffer[TRANSFER_FRAME_MAX_RECEIVED_SIZE]; /**< V4 transfer receive buffer. */
    uint8_t
        v4_tuning_request[PEDAL_V4_TUNING_REQUEST_SIZE]; /**< Encoded V4 tuning request buffer. */
    uint8_t response;            /**< Most recently received pedal-link response byte. */
    uint8_t brake_force_percent; /**< Encoded V3 brake-force scaling value. */
    uint8_t startup_frame_count; /**< Number of accepted V3 startup reports. */
    uint8_t pending_control;     /**< Pending V3 calibration-control bit mask. */
    uint8_t input_command[PEDAL_INPUT_AXIS_COUNT]; /**< Pending V3 input-command values. */
    uint8_t configuration_brake_force;             /**< Pending V3 brake-force configuration. */
    uint8_t v4_tuning_pending;                     /**< Pending V4 tuning-setting bit mask. */
    uint8_t v4_sent_value;                         /**< V4 tuning value in the active request. */
    uint8_t remote_auxiliary;   /**< Most recent auxiliary value from a digital pedal source. */
    uint8_t auxiliary_override; /**< Local auxiliary override value. */
    uint8_t legacy_retries[PEDAL_LEGACY_CHANNEL_COUNT]; /**< Retry counts for legacy channels. */
    PedalLegacyChannel legacy_channel;         /**< Legacy channel currently being polled. */
    PedalProtocolStatus protocol_status;       /**< Requested protocol status values. */
    PedalProtocolStatus transmitted_status;    /**< Last V3 protocol status sent. */
    PedalV4Tuning v4_tuning;                   /**< Requested V4 tuning values. */
    PedalTransferQueue host_transfer_queue;    /**< Queued generic V4 host requests. */
    PedalTransferResponse transfer_response;   /**< Response waiting for host forwarding. */
    PedalAdjustmentDisplay adjustment_display; /**< Latest adjustment display result. */
    PedalAdjustmentSource adjustment_source;   /**< Source of the active adjustment operation. */
    PedalV4Phase v4_phase;                     /**< Current V4 operation phase. */
    uint16_t analog_samples[PEDAL_INPUT_AXIS_COUNT]; /**< Latest local analog samples. */
    bool analog_samples_ready;      /**< True after local analog samples have been supplied. */
    bool connected;                 /**< True while an input source is considered connected. */
    bool digital_activity;          /**< True after digital pedal traffic has been accepted. */
    bool auxiliary_override_active; /**< True while auxiliary_override owns the auxiliary input. */
    bool input_command_pending;     /**< True when a V3 input command awaits transmission. */
    bool configuration_pending; /**< True when V3 brake-force configuration awaits transmission. */
    bool configuration_reset_pending; /**< True when V3 configuration reset must be sent. */
    bool recovery_handshake;   /**< True when the next V3 handshake is a recovery handshake. */
    bool status_transmitted;   /**< True after the current V3 status has been sent. */
    bool v4_request_active;    /**< True while a V4 request awaits completion. */
    bool v4_response_received; /**< True after a complete V4 response was accepted. */
    bool alternate_brake_force_received; /**< True while a new V3 brake-force report awaits
                                            consumption. */
    bool host_adjustment_pending;        /**< True while a host adjustment awaits selection. */
    bool button_adjustment_pending;      /**< True while a button adjustment awaits selection. */
    bool adjustment_display_pending;     /**< True while adjustment_display awaits consumption. */
} PedalService;

/**
 * @brief Initializes the pedal service.
 *
 * Resets transport, protocol, timing, pending-operation, and published-input state.
 *
 * @param[out] service Pedal service state to initialize.
 */
void pedal_service_init(PedalService *service);

/**
 * @brief Requests immediate pedal discovery.
 *
 * Releases the current source and returns the transport to the discovery request phase.
 *
 * @param[in,out] service Pedal service state to restart.
 */
void pedal_service_request_startup(PedalService *service);

/**
 * @brief Supplies local analog pedal samples.
 *
 * Retains the samples for fallback detection and updates the active analog source when selected.
 *
 * @param[in,out] service Pedal service state to update.
 * @param[in] samples Three local analog samples in primary, secondary, and tertiary order.
 */
void pedal_service_set_analog_samples(PedalService *service,
                                      const uint16_t samples[PEDAL_INPUT_AXIS_COUNT]);

/**
 * @brief Sets the encoded V3 brake-force scaling value.
 *
 * Retains force_percent for scaling subsequent raw V3 brake reports.
 *
 * @param[in,out] service Pedal service state to update.
 * @param[in] force_percent Encoded brake-force scaling value.
 */
void pedal_service_set_brake_force(PedalService *service, uint8_t force_percent);

/**
 * @brief Sets requested V4 tuning values.
 *
 * Marks settings whose values differ from the retained values for transmission.
 *
 * @param[in,out] service Pedal service state to update.
 * @param[in] tuning Requested V4 tuning values.
 */
void pedal_service_set_v4_tuning(PedalService *service, PedalV4Tuning tuning);

/**
 * @brief Selects the local or remote auxiliary input.
 *
 * Publishes value while active and restores the latest remote value when released.
 *
 * @param[in,out] service Pedal service state to update.
 * @param[in] active True to select the local auxiliary override.
 * @param[in] value Local auxiliary value to publish while active.
 */
void pedal_service_set_auxiliary_override(PedalService *service, bool active, uint8_t value);

/**
 * @brief Reports whether pedal calibration commands are accepted.
 *
 * Returns true during legacy transport or active V3 calibration.
 *
 * @param[in] service Pedal service state to inspect.
 * @return True when pedal calibration is active.
 */
bool pedal_service_calibration_active(const PedalService *service);

/**
 * @brief Reports whether automatic auxiliary calibration is active.
 *
 * Requires an active pedal calibration path and no asserted relevant V3 connection flags.
 *
 * @param[in] service Pedal service state to inspect.
 * @return True when automatic auxiliary endpoint settling is active.
 */
bool pedal_service_auxiliary_automatic_calibration(const PedalService *service);

/**
 * @brief Replaces the requested pedal protocol status.
 *
 * Stores the value, selectors, and scale used by pedal transports.
 *
 * @param[in,out] service Pedal service state to update.
 * @param[in] status Protocol status to copy.
 */
void pedal_service_set_protocol_status(PedalService *service, const PedalProtocolStatus *status);

/**
 * @brief Clears the requested pedal protocol status.
 *
 * Resets the retained value, selectors, and scale to zero.
 *
 * @param[in,out] service Pedal service state to update.
 */
void pedal_service_reset_protocol_status(PedalService *service);

/**
 * @brief Sets the brake-indicator protocol selector.
 *
 * Replaces only the first protocol selector in the retained status.
 *
 * @param[in,out] service Pedal service state to update.
 * @param[in] selector Brake-indicator selector to retain.
 */
void pedal_service_set_brake_indicator_selector(PedalService *service, uint8_t selector);

/**
 * @brief Reports whether legacy pedal transport is active.
 *
 * Includes both legacy request and response phases.
 *
 * @param[in] service Pedal service state to inspect.
 * @return True during a legacy request or response phase.
 */
bool pedal_service_legacy_transport_active(const PedalService *service);

/**
 * @brief Reports whether V3 startup handshake is active.
 *
 * Includes baud switching, handshake transmission, and initial V3 stream reports.
 *
 * @param[in] service Pedal service state to inspect.
 * @return True while V3 startup is active.
 */
bool pedal_service_handshake_active(const PedalService *service);

/**
 * @brief Applies a decoded pedal protocol command.
 *
 * Updates protocol selectors or applies a legacy scale when legacy transport is active.
 *
 * @param[in,out] service Pedal service state to update.
 * @param[in] command Decoded protocol command to apply.
 */
void pedal_service_apply_protocol_command(PedalService *service,
                                          const PedalProtocolCommand *command);

/**
 * @brief Queues V3 pedal calibration controls.
 *
 * Merges control bits with controls already awaiting transmission.
 *
 * @param[in,out] service Pedal service state to update.
 * @param[in] control V3 calibration control bits to queue.
 */
void pedal_service_request_control(PedalService *service, PedalV3Control control);

/**
 * @brief Reports whether V3 calibration controls are pending.
 *
 * Checks whether at least one control bit remains queued.
 *
 * @param[in] service Pedal service state to inspect.
 * @return True when a control bit awaits transmission.
 */
bool pedal_service_control_pending(const PedalService *service);

/**
 * @brief Queues V3 pedal calibration input values.
 *
 * Replaces any pending input-command values with the supplied bytes.
 *
 * @param[in,out] service Pedal service state to update.
 * @param[in] values Three input-command values to queue.
 */
void pedal_service_request_input_command(PedalService *service,
                                         const uint8_t values[PEDAL_INPUT_AXIS_COUNT]);

/**
 * @brief Queues V3 brake-force configuration.
 *
 * Replaces the pending force and retains a reset request until transmission.
 *
 * @param[in,out] service Pedal service state to update.
 * @param[in] brake_force Brake-force percentage to configure.
 * @param[in] reset True to request a controller reset marker.
 */
void pedal_service_request_configuration(PedalService *service, uint8_t brake_force, bool reset);

/**
 * @brief Reports whether V4 adjustment requests can be accepted.
 *
 * Checks whether the V4 transfer session is active.
 *
 * @param[in] service Pedal service state to inspect.
 * @return True while the V4 transfer session is active.
 */
bool pedal_service_adjustment_available(const PedalService *service);

/**
 * @brief Queues a host-requested V4 adjustment.
 *
 * Marks host adjustment work for selection by the V4 service.
 *
 * @param[in,out] service Pedal service state to update.
 */
void pedal_service_request_host_adjustment(PedalService *service);

/**
 * @brief Queues a wheel-button V4 adjustment.
 *
 * Marks button adjustment work for selection by the V4 service.
 *
 * @param[in,out] service Pedal service state to update.
 */
void pedal_service_request_button_adjustment(PedalService *service);

/**
 * @brief Queues one logical V4 host transfer.
 *
 * Handles reserved requests and appends other valid payloads to the host-transfer queue.
 *
 * @param[in,out] service Pedal service state to update.
 * @param[in] data Logical host request payload.
 * @param[in] length Number of payload bytes.
 */
void pedal_service_queue_host_transfer(PedalService *service, const uint8_t *data, uint8_t length);

/**
 * @brief Returns the pending host response.
 *
 * Keeps the response stable until pedal_service_release_transfer_response is called.
 *
 * @param[in] service Pedal service state to inspect.
 * @return Read-only pending response, or null when none is available.
 */
const PedalTransferResponse *pedal_service_transfer_response(const PedalService *service);

/**
 * @brief Releases the pending host response.
 *
 * Completes a queued host request when its response has been consumed and clears the response slot.
 *
 * @param[in,out] service Pedal service state to update.
 */
void pedal_service_release_transfer_response(PedalService *service);

/**
 * @brief Takes the pending V4 adjustment display result.
 *
 * Clears the pending indication after returning the retained result.
 *
 * @param[in,out] service Pedal service state to update.
 * @return Pending display result, or PEDAL_ADJUSTMENT_DISPLAY_IDLE when none is pending.
 */
PedalAdjustmentDisplay pedal_service_take_adjustment_display(PedalService *service);

/**
 * @brief Takes the pending V3 alternate brake-force report.
 *
 * Clears the pending indication after returning its retained value.
 *
 * @param[in,out] service Pedal service state to update.
 * @return Reported brake-force percentage, or PEDAL_ALTERNATE_BRAKE_FORCE_NO_UPDATE when none is
 * pending.
 */
uint8_t pedal_service_take_alternate_brake_force(PedalService *service);

/**
 * @brief Advances pedal discovery, transport, and input processing.
 *
 * Services at most one phase at each elapsed millisecond and updates published input and pending
 * operations.
 *
 * @param[in,out] service Pedal service state to advance.
 * @param[in] now_ms Current monotonic time in milliseconds.
 */
void pedal_service_run(PedalService *service, uint32_t now_ms);

/**
 * @brief Returns the currently published pedal input.
 *
 * The returned state remains owned by service and is read-only to the caller.
 *
 * @param[in] service Pedal service state to inspect.
 * @return Read-only published pedal input.
 */
const PedalInput *pedal_service_input(const PedalService *service);

/**
 * @brief Returns the retained V3 pedal report state.
 *
 * The returned state remains owned by service and is read-only to the caller.
 *
 * @param[in] service Pedal service state to inspect.
 * @return Read-only V3 state.
 */
const PedalV3State *pedal_service_v3_state(const PedalService *service);

#endif
