#include "wheel/service.h"

#include <stdbool.h>
#include <stdint.h>

#include "platform/time.h"
#include "serial/message.h"
#include "serial/service.h"
#include "wheel/adapter_commands.h"
#include "wheel/auxiliary_output.h"
#include "wheel/display_output.h"
#include "wheel/display_overlay.h"
#include "wheel/output_reports.h"
#include "wheel/protocol.h"
#include "wheel/protocol_bridge_service.h"

/** @brief Attached-wheel protocol, packet-layout, and timing constants. */
enum {
    WHEEL_PROTOCOL_TRANSPORT_COMMAND = 2,   /**< Serial message type for command-two traffic. */
    WHEEL_BUTTON_COMMAND = 3,               /**< Serial message type for command-three scans. */
    WHEEL_BUTTON_REQUEST_READY = 1,         /**< Ready flag in a command-three request. */
    WHEEL_BUTTON_RESPONSE_READY = 2,        /**< Ready flag in a command-three response. */
    WHEEL_BUTTON_SCAN_STATUS_UNAVAILABLE = 0x40, /**< Status-report service is unavailable. */
    WHEEL_BUTTON_SCAN_STATUS_MEMORY_UPDATE = 0x80, /**< One-shot status-memory update marker. */
    WHEEL_BUTTON_PRIMARY_RESPONSE = 0xe0,   /**< Primary command-three response marker. */
    WHEEL_BUTTON_SECONDARY_RESPONSE = 0xc0, /**< Secondary command-three response marker. */
    WHEEL_BUTTON_RESPONSE_MASK = 0xe0,      /**< Mask for a command-three response marker. */
    WHEEL_BUTTON_VALUE_MASK = 0x1f,         /**< Mask for a command-three sample value. */
    WHEEL_PROTOCOL_WAITING_INTERVAL_MS = 1, /**< Initial and reset transport interval. */
    WHEEL_PROTOCOL_SYNCHRONIZING_INTERVAL_MS = 10,   /**< Handshake acknowledgement interval. */
    WHEEL_PROTOCOL_ACKNOWLEDGING_INTERVAL_MS = 2000, /**< Ready-mode interval. */
    WHEEL_PROTOCOL_SELECTING_INTERVAL_MS = 1,        /**< Mode-selection transport interval. */
    WHEEL_PROTOCOL_AUTHENTICATING_INTERVAL_MS = 4,   /**< Authentication transport interval. */
    WHEEL_PROTOCOL_ACTIVE_INTERVAL_MS = 1,           /**< Active protocol transport interval. */
    WHEEL_PROTOCOL_ACTIVITY_TIMEOUT_MS = 2000,       /**< Normal protocol activity timeout. */
    WHEEL_PROTOCOL_PROOF_TIMEOUT_MS = 2000,
    WHEEL_PROTOCOL_SELECTION_TIMEOUT_MS = 10000,
    WHEEL_MULTI_POSITION_PRIMARY_OFFSET = 6,   /**< Primary rotary position offset. */
    WHEEL_MULTI_POSITION_SECONDARY_OFFSET = 7, /**< Secondary rotary position offset. */
    WHEEL_MULTI_POSITION_PACKED_OFFSET = 13,   /**< Packed rotary position offset. */
    WHEEL_ACCESSORY_FLAGS_OFFSET = 15,         /**< Accessory flags offset. */
    WHEEL_INPUT_DIRECTIONAL_OFFSET = 0,        /**< Directional button offset. */
    WHEEL_INPUT_SECONDARY_OFFSET = 1,          /**< Secondary-button offset. */
    WHEEL_INPUT_CLUTCH_OFFSET = 3,             /**< Clutch-paddle offset. */
    WHEEL_INPUT_TUNING_OFFSET = 5,             /**< Tuning and motion offset. */
    WHEEL_INPUT_AUXILIARY_OFFSET = 22,         /**< Auxiliary-report offset. */
    WHEEL_PACKED_REPORT_TWO_OPCODE = 0x0a,     /**< Operating-mode opcode for compact report two. */
    WHEEL_PACKED_REPORT_ONE_OPCODE = 0x0b,     /**< Operating-mode opcode for compact report one. */
    WHEEL_ALTERNATIVE_SHIFTER_BUTTONS = 0x0009,   /**< Secondary-button activation mask. */
    WHEEL_ALTERNATIVE_SHIFTER_LATCH_FLAGS = 0x03, /**< Control-latch activation mask. */
    WHEEL_ALTERNATIVE_SHIFTER_DEBOUNCE_MS = 800,  /**< Alternative-shifter debounce interval. */
};

/**
 * @brief Assigns one source bit to a destination bit.
 *
 * Replaces the selected destination bit while preserving every other bit.
 *
 * @param[in,out] value Destination byte to update.
 * @param[in] target Destination bit index.
 * @param[in] source Source byte.
 * @param[in] source_bit Source bit index.
 */
static void assign(uint8_t *value, uint8_t target, uint8_t source, uint8_t source_bit) {
    uint8_t mask = (uint8_t)(1u << target);
    *value = (*value & (uint8_t)~mask) | (((source >> source_bit) & 1u) << target);
}

/**
 * @brief Maps a native-mode phase-8 sample into the base button banks.
 *
 * Applies the auxiliary scan-phase bit mapping to one sample set.
 *
 * @param[in,out] banks Three sampled button banks to update.
 * @param[in] sample Five input bits returned by the attached device.
 */
static void apply_auxiliary(uint8_t banks[WHEEL_BUTTON_BANK_COUNT], uint8_t sample,
                            uint8_t interface_mode) {
    assign(&banks[0], 3, sample, 3);
    assign(&banks[0], 1, sample, 4);
    assign(&banks[0], 2, sample, 1);
    assign(&banks[0], 0, sample, 2);
    assign(&banks[2], interface_mode == 6 ? 3 : 2, sample, 0);
}

/**
 * @brief Maps a native-mode phase-1 sample into the base button banks.
 *
 * Applies the first scan-phase bit mapping to one sample set.
 *
 * @param[in,out] banks Three sampled button banks to update.
 * @param[in] sample Five input bits returned by the attached device.
 * @param[in] secondary Adds the secondary-channel mapping for sample bit 1 when true.
 * @param[in] interface_mode Active host-interface presentation mode selecting the sample-bit map.
 */
static void apply_first(uint8_t banks[WHEEL_BUTTON_BANK_COUNT], uint8_t sample, bool secondary,
                        uint8_t interface_mode) {
    assign(&banks[2], 5, sample, 0);
    bool primary_mapping = interface_mode <= 1 || (interface_mode >= 6 && interface_mode <= 8);
    assign(&banks[2], primary_mapping ? 1 : 3, sample, 3);
    assign(&banks[1], 2, sample, 4);
    assign(&banks[1], 1, sample, 2);
    if (secondary) {
        assign(&banks[2], 3, sample, 1);
    }
}

/**
 * @brief Maps a native-mode phase-2 sample into the base button banks.
 *
 * Applies the second scan-phase bit mapping to one sample set.
 *
 * @param[in,out] banks Three sampled button banks to update.
 * @param[in] sample Five input bits returned by the attached device.
 */
static void apply_second(uint8_t banks[WHEEL_BUTTON_BANK_COUNT], uint8_t sample) {
    assign(&banks[1], 3, sample, 0);
    assign(&banks[1], 5, sample, 3);
    assign(&banks[1], 4, sample, 4);
    assign(&banks[1], 7, sample, 1);
    assign(&banks[1], 6, sample, 2);
}

/**
 * @brief Maps a native-mode phase-4 sample into the base button banks.
 *
 * Applies the third scan-phase bit mapping to one sample set.
 *
 * @param[in,out] banks Three sampled button banks to update.
 * @param[in] sample Five input bits returned by the attached device.
 */
static void apply_third(uint8_t banks[WHEEL_BUTTON_BANK_COUNT], uint8_t sample) {
    assign(&banks[0], 4, sample, 2);
    assign(&banks[0], 6, sample, 1);
    assign(&banks[0], 5, sample, 4);
    assign(&banks[0], 7, sample, 3);
    assign(&banks[1], 0, sample, 0);
}

/**
 * @brief Publishes the filtered command-3 button samples.
 *
 * Intersects the three interleaved sample sets and advances the sample insertion position.
 *
 * @param[in,out] service Wheel service that owns the sample history and output banks.
 */
static void publish_scan_samples(WheelService *service,
                                 uint8_t samples[WHEEL_SCAN_SAMPLE_DEPTH][WHEEL_BUTTON_BANK_COUNT],
                                 uint8_t *sample_index) {
    for (uint8_t bank = 0; bank < WHEEL_BUTTON_BANK_COUNT; bank++) {
        service->button_banks[bank] = samples[0][bank] & samples[1][bank] & samples[2][bank];
    }
    (*sample_index)++;
    if (*sample_index == WHEEL_SCAN_SAMPLE_DEPTH) {
        *sample_index = 0;
    }
}

/**
 * @brief Selects the response prefix for the active scan channel.
 *
 * Uses the negotiated primary or secondary scan phase to select its response marker.
 *
 * @param[in] service Wheel service with the negotiated scan channel.
 * @return 0xE0 for the primary channel or 0xC0 for the secondary channel.
 */
static uint8_t expected_scan_response(const WheelService *service) {
    return service->protocol.phase == WHEEL_PROTOCOL_SCANNING_SECONDARY
               ? WHEEL_BUTTON_SECONDARY_RESPONSE
               : WHEEL_BUTTON_PRIMARY_RESPONSE;
}

static void clear_scan_filter(WheelService *service);
static void reset_connection(WheelService *service);

/**
 * @brief Selects the official logical protocol transport interval.
 *
 * Maps each handshake phase to the delay used before its next command-two request. The deadline
 * is armed when a request is queued and remains unchanged when a response advances the phase.
 *
 * @param[in] service Wheel service with the current protocol phase.
 * @return Logical transport interval in milliseconds.
 */
static uint16_t protocol_transport_interval(const WheelService *service) {
    switch (service->protocol.phase) {
    case WHEEL_PROTOCOL_SYNCHRONIZING:
        return WHEEL_PROTOCOL_SYNCHRONIZING_INTERVAL_MS;
    case WHEEL_PROTOCOL_ACKNOWLEDGING:
        return WHEEL_PROTOCOL_ACKNOWLEDGING_INTERVAL_MS;
    case WHEEL_PROTOCOL_SELECTING:
        return WHEEL_PROTOCOL_SELECTING_INTERVAL_MS;
    case WHEEL_PROTOCOL_AUTHENTICATING:
        return WHEEL_PROTOCOL_AUTHENTICATING_INTERVAL_MS;
    case WHEEL_PROTOCOL_ACTIVE:
        return WHEEL_PROTOCOL_ACTIVE_INTERVAL_MS;
    case WHEEL_PROTOCOL_WAITING:
    case WHEEL_PROTOCOL_UNSUPPORTED:
    case WHEEL_PROTOCOL_SCANNING_PRIMARY:
    case WHEEL_PROTOCOL_SCANNING_SECONDARY:
    default:
        return WHEEL_PROTOCOL_WAITING_INTERVAL_MS;
    }
}

/**
 * @brief Tests whether the next logical protocol request may start.
 *
 * Uses the strict greater-than comparison of the reference transport poller and keeps the first
 * request immediately available after initialization or connection reset.
 *
 * @param[in] service Wheel service with its transport schedule.
 * @param[in] now_ms Current monotonic time in milliseconds.
 * @return True when a command-two request may be queued.
 */
static bool protocol_transport_due(const WheelService *service, uint32_t now_ms) {
    return !service->protocol_transport_deadline_active ||
           (int32_t)(now_ms - service->protocol_transport_deadline_ms) > 0;
}

/**
 * @brief Accepts a command-3 button response.
 *
 * Validates the response marker and ready bit, maps its five input bits for the active scan phase,
 * and publishes the updated three-sample filter.
 *
 * @param[in,out] service Wheel service that owns the scan phase and output banks.
 * @param[in] response Received transport message, or null when no message is available.
 */
static void apply_scan_response(WheelService *service, const SerialMessageAssembly *response) {
    if (response == 0 || response->length != SERIAL_PACKET_MAX_PAYLOAD_SIZE) {
        reset_connection(service);
        return;
    }
    if ((response->data[SERIAL_PACKET_MAX_PAYLOAD_SIZE - 1] & WHEEL_BUTTON_RESPONSE_READY) == 0) {
        clear_scan_filter(service);
        service->scan_phase = 0;
        return;
    }
    uint8_t encoded = response->data[1];
    wheel_capability_update_scan(&service->protocol.capabilities, encoded);
    uint8_t response_type = encoded & WHEEL_BUTTON_RESPONSE_MASK;
    if (response_type != expected_scan_response(service)) {
        return;
    }
    uint8_t sample = encoded & WHEEL_BUTTON_VALUE_MASK;
    uint8_t *sample_index = response_type == WHEEL_BUTTON_SECONDARY_RESPONSE
                                ? &service->secondary_scan_sample_index
                                : &service->primary_scan_sample_index;
    uint8_t (*samples)[WHEEL_BUTTON_BANK_COUNT] = response_type == WHEEL_BUTTON_SECONDARY_RESPONSE
                                                      ? service->secondary_scan_samples
                                                      : service->primary_scan_samples;
    uint8_t *banks = samples[*sample_index];
    switch (service->scan_phase) {
    case WHEEL_SCAN_PHASE_FIRST:
        apply_first(banks, sample, response_type == WHEEL_BUTTON_SECONDARY_RESPONSE,
                    service->protocol.interface_mode);
        break;
    case WHEEL_SCAN_PHASE_SECOND:
        apply_second(banks, sample);
        break;
    case WHEEL_SCAN_PHASE_THIRD:
        apply_third(banks, sample);
        break;
    case WHEEL_SCAN_PHASE_AUXILIARY:
        apply_auxiliary(banks, sample, service->protocol.interface_mode);
        break;
    }
    publish_scan_samples(service, samples, sample_index);
}

/**
 * @brief Clears scan-mode button filtering.
 *
 * Zeros all three published button banks and both three-sample histories, then resets both sample
 * insertion positions.
 *
 * @param[in,out] service Attached-wheel service whose scan filter is cleared.
 */
static void clear_scan_filter(WheelService *service) {
    for (uint8_t bank = 0; bank < WHEEL_BUTTON_BANK_COUNT; bank++) {
        service->button_banks[bank] = 0;
        for (uint8_t sample = 0; sample < WHEEL_SCAN_SAMPLE_DEPTH; sample++) {
            service->primary_scan_samples[sample][bank] = 0;
            service->secondary_scan_samples[sample][bank] = 0;
        }
    }
    service->primary_scan_sample_index = 0;
    service->secondary_scan_sample_index = 0;
}

/**
 * @brief Restarts attached-wheel connection discovery.
 *
 * Reinitializes handshake and input state while preserving configured outputs, input filters,
 * adapter state, report capability configuration, pending system status, remote-tuning work, and
 * axis-processing configuration. An in-progress bite-point adjustment and its pending notifications
 * also remain intact. Negotiated calibration, tuning-menu, and input availability latches restart
 * with scan input and activity timing, matching the official reset phase at 0x044CDA-0x044CE0.
 *
 * @param[in,out] service Attached-wheel service to restart.
 */
static void reset_connection(WheelService *service) {
    WheelPacketModeOneButtonFilter mode_one_button_filter =
        service->protocol.mode_one_button_filter;
    WheelPacketModeOneControlAxisFilter mode_one_control_axis_filter =
        service->protocol.mode_one_control_axis_filter;
    WheelPacketModeOneOutput mode_one_output = service->protocol.mode_one_output;
    WheelPacketModeFourFilter mode_four_filter = service->protocol.mode_four_filter;
    WheelPacketModeFourOutput mode_four_output = service->protocol.mode_four_output;
    WheelPacketDisplayFilter display_filter = service->protocol.display_filter;
    WheelPacketRemappedFilter remapped_filter = service->protocol.remapped_filter;
    WheelPacketAlternateFilter alternate_filter = service->protocol.alternate_filter;
    WheelPacketAlternateOutput alternate_output = service->protocol.alternate_output;
    WheelPacketPackedFilter packed_filter = service->protocol.packed_filter;
    WheelPacketCrcFilter crc_filter = service->protocol.crc_filter;
    WheelPacketCrcOutput crc_output = service->protocol.crc_output;
    WheelAdapterInput adapter = service->protocol.adapter;
    WheelPacketAdapterOutput adapter_output = service->protocol.adapter_output;
    WheelPacketRemoteTuningOutput system_control_output = service->protocol.system_control_output;
    WheelPacketRemoteTuningOutput remote_tuning_output = service->protocol.remote_tuning_output;
    WheelOutputReports output_reports = service->protocol.output_reports;
    WheelCapabilityState capabilities = service->protocol.capabilities;
    capabilities.calibration_available = false;
    capabilities.tuning_menu_available = false;
    capabilities.input_available = false;
    uint32_t now_ms = service->protocol.now_ms;
    uint8_t interface_mode = service->protocol.interface_mode;
    uint8_t axis_override_mode = service->protocol.configured_axis_override_mode;
    uint8_t paddle_bite_point_percent = service->protocol.paddle_bite_point_percent;
    int16_t display_rotation_angle = service->protocol.display_rotation_angle;
    uint8_t system_status_code = service->protocol.system_status_code;
    uint32_t paddle_adjustment_deadline_ms =
        service->protocol.axis_override_processor.paddle_adjustment_deadline_ms;
    uint8_t axis_multiplex_phase = service->protocol.axis_override_processor.multiplex_phase;
    uint8_t paddle_clutch_phase = service->protocol.axis_override_processor.paddle_clutch_phase;
    bool axis_x_available = service->protocol.axis_override_processor.x_available;
    bool axis_y_available = service->protocol.axis_override_processor.y_available;
    bool packet_axis_report_enabled =
        service->protocol.axis_override_processor.packet_axis_report_enabled;
    bool paddle_bite_point_report_pending =
        service->protocol.axis_override_processor.paddle_bite_point_report_pending;
    bool paddle_bite_point_commit_pending =
        service->protocol.axis_override_processor.paddle_bite_point_commit_pending;
    bool button_latch_enabled = service->protocol.button_latch_enabled;
    bool display_rotation_enabled = service->protocol.display_rotation_enabled;
    bool display_character_mode = service->protocol.display_character_mode;
    bool host_capability_enabled = service->protocol.host_capability_enabled;
    bool profile_transition_pending = service->protocol.profile_transition_pending;
    bool system_status_pending = service->protocol.system_status_pending;
    wheel_protocol_init(&service->protocol);
    service->protocol.mode_one_button_filter = mode_one_button_filter;
    service->protocol.mode_one_control_axis_filter = mode_one_control_axis_filter;
    service->protocol.mode_one_output = mode_one_output;
    service->protocol.mode_four_filter = mode_four_filter;
    service->protocol.mode_four_output = mode_four_output;
    service->protocol.display_filter = display_filter;
    service->protocol.remapped_filter = remapped_filter;
    service->protocol.alternate_filter = alternate_filter;
    service->protocol.alternate_output = alternate_output;
    service->protocol.packed_filter = packed_filter;
    service->protocol.crc_filter = crc_filter;
    service->protocol.crc_output = crc_output;
    service->protocol.adapter = adapter;
    service->protocol.adapter_output = adapter_output;
    service->protocol.system_control_output = system_control_output;
    service->protocol.remote_tuning_output = remote_tuning_output;
    service->protocol.output_reports = output_reports;
    service->protocol.capabilities = capabilities;
    wheel_protocol_set_axis_processing(&service->protocol, interface_mode, axis_override_mode,
                                       paddle_bite_point_percent, now_ms);
    service->protocol.axis_override_processor.paddle_adjustment_deadline_ms =
        paddle_adjustment_deadline_ms;
    service->protocol.axis_override_processor.multiplex_phase = axis_multiplex_phase;
    service->protocol.axis_override_processor.paddle_clutch_phase = paddle_clutch_phase;
    service->protocol.axis_override_processor.x_available = axis_x_available;
    service->protocol.axis_override_processor.y_available = axis_y_available;
    service->protocol.axis_override_processor.packet_axis_report_enabled =
        packet_axis_report_enabled;
    service->protocol.axis_override_processor.paddle_bite_point_report_pending =
        paddle_bite_point_report_pending;
    service->protocol.axis_override_processor.paddle_bite_point_commit_pending =
        paddle_bite_point_commit_pending;
    wheel_protocol_set_button_latch(&service->protocol, button_latch_enabled,
                                    profile_transition_pending);
    wheel_protocol_set_display_rotation(&service->protocol, display_rotation_enabled,
                                        display_rotation_angle);
    wheel_protocol_set_display_character_mode(&service->protocol, display_character_mode);
    wheel_protocol_set_host_capability(&service->protocol, host_capability_enabled);
    if (system_status_pending) {
        wheel_protocol_queue_system_status(&service->protocol, system_status_code);
    }
    clear_scan_filter(service);
    wheel_service_set_auxiliary_exclusive_mode(service, false);
    service->protocol_deadline_ms = 0;
    service->protocol_transport_deadline_ms = 0;
    service->protocol_transport_interval_ms = WHEEL_PROTOCOL_WAITING_INTERVAL_MS;
    service->protocol_deadline_active = false;
    service->protocol_transport_deadline_active = false;
    service->protocol_exchange_completed = false;
    service->bridge_recovery_pending = false;
    service->scan_phase = 0;
    service->request_kind = WHEEL_SERVICE_REQUEST_NONE;
    for (uint16_t index = 0; index < sizeof(service->request); index++) {
        service->request[index] = 0;
    }
    service->status_memory_startup_pending = false;
    service->tuning_menu_override_enabled = false;
    service->tuning_menu_override_value = false;
}

/**
 * @brief Configures attached-wheel analog-paddle processing.
 *
 * Applies the active host interface, tuning-profile paddle mode, and per-profile bite-point
 * percentage to subsequent attached-wheel input packets.
 *
 * @param[in,out] service Attached-wheel service to configure.
 * @param[in] interface_mode Active host interface mode.
 * @param[in] paddle_mode Active tuning-profile analog-paddle mode.
 * @param[in] bite_point_percent Active profile bite-point percentage.
 * @param[in] now_ms Current monotonic time in milliseconds.
 */
void wheel_service_configure_axis_processing(WheelService *service, uint8_t interface_mode,
                                             uint8_t paddle_mode, uint8_t bite_point_percent,
                                             uint32_t now_ms) {
    wheel_protocol_set_axis_processing(&service->protocol, interface_mode, paddle_mode,
                                       bite_point_percent, now_ms);
}

/**
 * @brief Takes a completed attached-wheel bite-point adjustment.
 *
 * Returns the adjusted active-profile percentage once after the paddle adjustment gesture ends.
 *
 * @param[in,out] service Attached-wheel service and protocol state.
 * @param[out] updated_percent Completed percentage to persist.
 * @return True when a completed adjustment was available.
 */
bool wheel_service_take_bite_point(WheelService *service, uint8_t *updated_percent) {
    return wheel_protocol_take_bite_point(&service->protocol, updated_percent);
}

/**
 * @brief Takes an attached-wheel bite-point report update.
 *
 * Returns each accepted percentage change once for presentation in the primary input report.
 *
 * @param[in,out] service Attached-wheel service and protocol state.
 * @param[out] updated_percent Percentage to publish in the next input report.
 * @return True when a new percentage was available.
 */
bool wheel_service_take_bite_point_report(WheelService *service, uint8_t *updated_percent) {
    return wheel_protocol_take_bite_point_report(&service->protocol, updated_percent);
}

/**
 * @brief Reports the active attached-wheel bite-point adjustment.
 *
 * Returns the live percentage while the analog paddle remains in adjustment mode.
 *
 * @param[in] service Attached-wheel service and protocol state.
 * @param[out] percent Current bite-point percentage.
 * @return True while bite-point adjustment is active.
 */
bool wheel_service_bite_point_adjustment(const WheelService *service, uint8_t *percent) {
    if (service->protocol.axis_override_processor.paddle_clutch_phase !=
        WHEEL_PADDLE_CLUTCH_ADJUSTING) {
        return false;
    }
    *percent = service->protocol.paddle_bite_point_percent;
    return true;
}

/**
 * @brief Reports whether command-2 protocol traffic is active.
 *
 * Treats authentication and negotiated packet exchange as active protocol phases.
 *
 * @param[in] service Attached-wheel service state.
 * @return True during authentication or active packet exchange.
 */
static bool protocol_exchange_active(const WheelService *service) {
    return service->protocol.phase == WHEEL_PROTOCOL_SELECTING ||
           service->protocol.phase == WHEEL_PROTOCOL_AUTHENTICATING ||
           service->protocol.phase == WHEEL_PROTOCOL_ACTIVE;
}

/**
 * @brief Extends the active command-2 exchange deadline.
 *
 * Starts a two-second activity window after a packet-ready event, or while the authentication
 * proof is pending.
 *
 * @param[in,out] service Wheel service that owns the command-2 exchange.
 * @param[in] now_ms Current monotonic time in milliseconds.
 */
static void refresh_protocol_deadline(WheelService *service, uint32_t now_ms) {
    uint32_t timeout =
        service->protocol.phase == WHEEL_PROTOCOL_SELECTING ? WHEEL_PROTOCOL_SELECTION_TIMEOUT_MS
        : service->protocol.phase == WHEEL_PROTOCOL_AUTHENTICATING &&
                service->protocol.authentication.stage == WHEEL_AUTHENTICATION_AWAITING_PROOF
            ? WHEEL_PROTOCOL_PROOF_TIMEOUT_MS
            : WHEEL_PROTOCOL_ACTIVITY_TIMEOUT_MS;
    service->protocol_deadline_ms = now_ms + timeout;
    service->protocol_deadline_active = true;
}

/**
 * @brief Starts the next command-three wheel scan.
 *
 * Rotates through scan phases 8, 4, 2, and 1, publishes status-service availability and a pending
 * status-memory update in the phase byte, encodes current display output, marks the request ready,
 * and submits a full 57-byte type-three message.
 *
 * @param[in,out] service Wheel service starting the scan.
 * @param[in] now_ms Current monotonic time in milliseconds.
 */
static void start_scan(WheelService *service, uint32_t now_ms) {
    service->scan_phase >>= 1;
    if (service->scan_phase == 0) {
        service->scan_phase = WHEEL_SCAN_PHASE_AUXILIARY;
    }
    for (uint8_t index = 0; index < SERIAL_PACKET_MAX_PAYLOAD_SIZE; index++) {
        service->request[index] = 0;
    }
    service->request[0] = service->scan_phase;
    if (service->protocol.crc_output.report_state == UINT8_MAX) {
        service->request[0] |= WHEEL_BUTTON_SCAN_STATUS_UNAVAILABLE;
    }
    if (service->protocol.crc_output.status_update_pending) {
        service->request[0] |= WHEEL_BUTTON_SCAN_STATUS_MEMORY_UPDATE;
        service->protocol.crc_output.status_update_pending = false;
    }
    WheelDisplayOutput display_output = service->display_output;
    display_output.third_glyph_marker = service->scan_phase == WHEEL_SCAN_PHASE_THIRD &&
                                        wheel_protocol_report_mode_marker(&service->protocol);
    uint8_t encoded = service->scan_phase == WHEEL_SCAN_PHASE_AUXILIARY
                          ? wheel_auxiliary_output_encode(&service->auxiliary_output)
                          : wheel_display_output_encode(&display_output, service->scan_phase);
    service->request[1] = (uint8_t)~encoded;
    service->request[SERIAL_PACKET_MAX_PAYLOAD_SIZE - 1] = WHEEL_BUTTON_REQUEST_READY;
    service->request_kind = WHEEL_SERVICE_REQUEST_BUTTONS;
    if (!serial_service_start(service->transport, WHEEL_BUTTON_COMMAND, service->request,
                              sizeof(service->request), now_ms)) {
        reset_connection(service);
    }
}

/**
 * @brief Starts the next command-two wheel protocol exchange.
 *
 * Submits the current 57-byte protocol response through serial message type two.
 *
 * @param[in,out] service Wheel service starting the protocol exchange.
 * @param[in] now_ms Current monotonic time in milliseconds.
 * @return True when the command-two request entered the serial scheduler; otherwise false.
 */
static bool start_protocol(WheelService *service, uint32_t now_ms, bool bypass_interval) {
    if (service->status_memory_startup_pending ||
        (!bypass_interval && !protocol_transport_due(service, now_ms))) {
        return false;
    }
    service->request_kind = WHEEL_SERVICE_REQUEST_PROTOCOL;
    if (serial_service_start(service->transport, WHEEL_PROTOCOL_TRANSPORT_COMMAND,
                             wheel_protocol_response(&service->protocol),
                             WHEEL_PROTOCOL_PACKET_SIZE, now_ms)) {
        service->protocol_transport_interval_ms = protocol_transport_interval(service);
        service->protocol_transport_deadline_ms = now_ms + service->protocol_transport_interval_ms;
        service->protocol_transport_deadline_active = true;
        return true;
    }
    reset_connection(service);
    return false;
}

/**
 * @brief Reports whether scan-mode button polling is active.
 *
 * Accepts either primary or secondary scan mode.
 *
 * @param[in] service Attached-wheel service state.
 * @return True during primary or secondary scan polling.
 */
static bool scan_active(const WheelService *service) {
    return service->protocol.phase == WHEEL_PROTOCOL_SCANNING_PRIMARY ||
           service->protocol.phase == WHEEL_PROTOCOL_SCANNING_SECONDARY;
}

/**
 * @brief Initializes attached-wheel protocol service state.
 *
 * Attaches the shared serial service and resets protocol, rotary transitions, display, scan
 * filter, request, and activity-deadline state.
 *
 * @param[out] service Wheel service to initialize.
 * @param[in,out] transport Shared serial service used for type-two and type-three traffic.
 */
void wheel_service_init(WheelService *service, SerialService *transport) {
    service->transport = transport;
    wheel_protocol_init(&service->protocol);
    service->adapter_display_state = 0;
    wheel_service_reset_adapter_commands(service);
    wheel_rotary_input_init(&service->rotary_input);
    clear_scan_filter(service);
    service->display_output = (WheelDisplayOutput){0};
    service->default_display_output = (WheelDisplayOutput){0};
    service->display_override_output = (WheelDisplayOutput){0};
    wheel_display_overlay_init(&service->display_overlay);
    service->auxiliary_output = (WheelAuxiliaryOutput){0};
    for (uint16_t index = 0; index < sizeof(service->request); index++) {
        service->request[index] = 0;
    }
    service->protocol_deadline_ms = 0;
    service->protocol_transport_deadline_ms = 0;
    service->protocol_transport_interval_ms = WHEEL_PROTOCOL_WAITING_INTERVAL_MS;
    service->alternative_shifter_deadline_ms = 0;
    service->scan_phase = 0;
    service->request_kind = WHEEL_SERVICE_REQUEST_NONE;
    service->protocol_deadline_active = false;
    service->protocol_transport_deadline_active = false;
    service->protocol_exchange_completed = false;
    service->bridge_recovery_pending = false;
    service->display_override_active = false;
    service->alternative_shifter_enabled = false;
    service->alternative_shifter_debounced = false;
    service->status_memory_startup_pending = false;
    service->tuning_menu_override_enabled = false;
    service->tuning_menu_override_value = false;
}

/**
 * @brief Restarts attached-adapter command discovery.
 *
 * Clears incomplete adapter command work and restores the logical adapter defaults so a reset
 * type-four transport cannot complete a stale request.
 *
 * @param[in,out] service Attached-wheel service whose adapter commands restart.
 */
void wheel_service_reset_adapter_commands(WheelService *service) {
    if (service == 0) {
        return;
    }
    wheel_adapter_command_service_init(&service->adapter_commands, &service->protocol.adapter);
    wheel_adapter_command_service_queue_display_state(&service->adapter_commands,
                                                      service->adapter_display_state);
}

/**
 * @brief Advances asynchronous commands for the attached adapter.
 *
 * Transfers a newly latched display report into the adapter command service, then advances
 * endpoint discovery, status polling, component reads, or display transmission over the shared
 * type-four transport.
 *
 * @param[in,out] service Attached-wheel service containing adapter state and output latches.
 * @param[in,out] transport Shared command transport used by adapter requests.
 */
void wheel_service_run_adapter_commands(WheelService *service, CommandTransport *transport) {
    if (service == 0 || transport == 0) {
        return;
    }
    if (service->protocol.adapter_output.display_update_pending) {
        wheel_adapter_command_service_queue_display(
            &service->adapter_commands, service->protocol.adapter_output.previous_display_report);
        service->protocol.adapter_output.display_update_pending = false;
    }
    wheel_adapter_command_service_set_glyphs(&service->adapter_commands,
                                             service->protocol.adapter_output.published_glyphs);
    wheel_adapter_command_service_run(&service->adapter_commands, &service->protocol.adapter,
                                      transport);
}

/**
 * @brief Takes the latest adapter-originated host control batch.
 *
 * Copies and consumes a completed offset-0xA0 adapter read from the command service.
 *
 * @param[in,out] service Attached-wheel service retaining the completed batch.
 * @param[out] output Destination for the complete 30-byte control area.
 * @return True when a completed batch was copied.
 */
bool wheel_service_take_adapter_host_controls(WheelService *service,
                                              uint8_t output[WHEEL_ADAPTER_HOST_CONTROLS_SIZE]) {
    return service != 0 &&
           wheel_adapter_command_service_take_host_controls(&service->adapter_commands, output);
}

/**
 * @brief Tests whether the current wheel presents its own tuning display.
 *
 * Adapter-oriented modes 4, 6, 12, and 21 use the extended adapter endpoint as their local
 * display. Modes 9, 10, 11, 14, 15, 16, 23, 27, 28, and 29 always provide a local display.
 *
 * @param[in] service Attached-wheel service and adapter state.
 * @return True when remote tuning remains on the wheel-side display.
 */
bool wheel_service_tuning_display_supported(const WheelService *service) {
    switch (service->protocol.mode) {
    case 4:
    case 6:
    case 12:
    case WHEEL_MODE_CRC_AUTHENTICATED:
        return service->protocol.adapter.connected && service->protocol.adapter.mode == 1;
    case 9:
    case 10:
    case 11:
    case WHEEL_MODE_REMOTE_TUNING_LEGACY:
    case WHEEL_MODE_LEGACY_ALTERNATE:
    case 16:
    case WHEEL_MODE_LEGACY_COMPATIBILITY:
    case 27:
    case WHEEL_MODE_REMOTE_TUNING_EXTENDED:
    case 29:
        return true;
    default:
        return false;
    }
}

/**
 * @brief Activates the Xbox GIP display-report state.
 *
 * Applies the attached-wheel display capability gate, preserves inhibited reports, clears report
 * bits two through four, and sets report bits six and thirteen.
 *
 * @param[in,out] service Wheel service containing the display-report state.
 */
void wheel_service_activate_xbox_gip_display(WheelService *service) {
    if (service == NULL || !wheel_service_tuning_display_supported(service)) {
        return;
    }

    uint16_t report = service->protocol.adapter_output.display_report;
    if ((report & 0x1001u) != 0) {
        return;
    }

    service->protocol.adapter_output.display_report = (report & 0xffe3u) | 0x2040u;
}

/**
 * @brief Queues the adapter's remote-tuning active state.
 *
 * Sends an active state only while an adapter is connected and the current wheel does not provide
 * its own tuning display. All other conditions send an inactive state.
 *
 * @param[in,out] service Attached-wheel service receiving the state.
 * @param[in] active Current host remote-tuning session state.
 */
void wheel_service_queue_adapter_remote_tuning_active(WheelService *service, bool active) {
    if (service == 0) {
        return;
    }
    bool adapter_active = active && service->protocol.adapter.connected &&
                          !wheel_service_tuning_display_supported(service);
    wheel_adapter_command_service_queue_remote_tuning_active(&service->adapter_commands,
                                                             adapter_active);
}

/**
 * @brief Queues the adapter's refresh state.
 *
 * Retains the newest Boolean state in the adapter command service for transmission at the next
 * available shared-transport boundary.
 *
 * @param[in,out] service Attached-wheel service receiving the state.
 * @param[in] active Adapter refresh state.
 */
void wheel_service_queue_adapter_refresh_state(WheelService *service, bool active) {
    if (service == 0) {
        return;
    }
    wheel_adapter_command_service_queue_refresh_state(&service->adapter_commands, active);
}

/**
 * @brief Queues a remote setup selection for the attached adapter.
 *
 * Retains the newest one-based selection in the adapter command service for transmission at the
 * next available shared-transport boundary.
 *
 * @param[in,out] service Attached-wheel service receiving the selection.
 * @param[in] selection One-based setup selection.
 */
void wheel_service_queue_adapter_setup_selection(WheelService *service, uint8_t selection) {
    if (service == 0) {
        return;
    }
    wheel_adapter_command_service_queue_setup_selection(&service->adapter_commands, selection);
}

/**
 * @brief Queues a system display state for the attached adapter.
 *
 * Retains the newest nonzero state across adapter command resets and queues it for the standard
 * endpoint's offset-0x18 display-state command.
 *
 * @param[in,out] service Attached-wheel service receiving the display state.
 * @param[in] state Nonzero system display state.
 */
void wheel_service_queue_adapter_display_state(WheelService *service, uint8_t state) {
    if (service == 0 || state == 0) {
        return;
    }
    service->adapter_display_state = state;
    wheel_adapter_command_service_queue_display_state(&service->adapter_commands, state);
}

/**
 * @brief Queues a native tuning-display command.
 *
 * Retains a type-0x82 command only for a directly attached wheel with a supported tuning display.
 * Extended adapter displays use their offset-0x1A text-line transport instead.
 *
 * @param[in,out] service Attached-wheel service receiving the command.
 * @param[in] command Native tuning-display command.
 * @return True when the command was queued for a directly attached display.
 */
bool wheel_service_queue_tuning_display_command(WheelService *service, uint8_t command) {
    if (service == 0 || !wheel_service_tuning_display_supported(service) ||
        (service->protocol.adapter.connected && service->protocol.adapter.mode == 1)) {
        return false;
    }
    wheel_output_reports_queue_display_command(&service->protocol.output_reports, command);
    return true;
}

enum {
    WHEEL_SERVICE_CONFIGURATION_AUTOMATIC_SLOT = 0,
    WHEEL_SERVICE_CONFIGURATION_PROFILE_MODE_SLOT = 1,
    WHEEL_SERVICE_CONFIGURATION_PROFILE_COMMAND_FIRST = 25,
};

static uint8_t selected_tuning_configuration_command(TuningEntry entry,
                                                     const TuningProfileBank *bank,
                                                     bool profile_mode_enabled) {
    uint8_t command = (uint8_t)entry;
    if (entry != TUNING_ENTRY_SETUP) {
        return command;
    }
    if (profile_mode_enabled &&
        bank->active_slot == WHEEL_SERVICE_CONFIGURATION_PROFILE_MODE_SLOT) {
        return TUNING_ENTRY_COUNT;
    }
    if (bank->active_slot != WHEEL_SERVICE_CONFIGURATION_AUTOMATIC_SLOT) {
        return (uint8_t)(bank->selected_slot + WHEEL_SERVICE_CONFIGURATION_PROFILE_COMMAND_FIRST);
    }
    return command;
}

bool wheel_service_queue_selected_tuning_configuration(WheelService *service, TuningEntry entry,
                                                       const TuningProfileBank *bank,
                                                       bool profile_mode_enabled) {
    if (service == 0 || bank == 0 || entry >= TUNING_ENTRY_COUNT ||
        bank->selected_slot >= TUNING_PROFILE_SLOT_COUNT ||
        bank->active_slot >= TUNING_PROFILE_SLOT_COUNT) {
        return false;
    }
    return wheel_service_queue_tuning_display_command(
        service, selected_tuning_configuration_command(entry, bank, profile_mode_enabled));
}

/**
 * @brief Queues a native tuning-display notification.
 *
 * Passes recognized prompt and confirmation commands to the attached-wheel output scheduler. The
 * scheduler emits them only in remote-tuning wheel modes.
 *
 * @param[in,out] service Attached-wheel service receiving the notification.
 * @param[in] command Native prompt or confirmation command.
 * @return True when the service was available to accept the command.
 */
bool wheel_service_queue_tuning_display_notification(WheelService *service, uint8_t command) {
    if (service == NULL) {
        return false;
    }
    wheel_output_reports_queue_display_notification(&service->protocol.output_reports, command);
    return true;
}

/**
 * @brief Queues one line for an extended adapter display.
 *
 * Forwards a valid line only while the mode-one adapter endpoint is connected.
 *
 * @param[in,out] service Attached-wheel service receiving the line.
 * @param[in] line One-based display line identifier from one through four.
 * @param[in] metadata Display line presentation metadata.
 * @param[in] text Text bytes to retain.
 * @param[in] length Number of text bytes.
 * @return True when the line was queued for the extended adapter.
 */
bool wheel_service_queue_adapter_text_line(WheelService *service, uint8_t line, uint8_t metadata,
                                           const uint8_t *text, uint8_t length) {
    if (service == 0 || !service->protocol.adapter.connected ||
        service->protocol.adapter.mode != 1) {
        return false;
    }
    return wheel_adapter_command_service_queue_text_line(&service->adapter_commands, line, metadata,
                                                         text, length);
}

/**
 * @brief Queues the extended adapter's text-page close record.
 *
 * Retains line identifier zero only while the mode-one adapter endpoint is connected.
 *
 * @param[in,out] service Attached-wheel service receiving the close record.
 * @return True when the close record was queued for the extended adapter.
 */
bool wheel_service_queue_adapter_text_close(WheelService *service) {
    if (service == 0 || !service->protocol.adapter.connected ||
        service->protocol.adapter.mode != 1) {
        return false;
    }
    wheel_adapter_command_service_queue_text_close(&service->adapter_commands);
    return true;
}

/**
 * @brief Applies the shared two-byte auxiliary report.
 *
 * Updates the legacy response vibration fields, alternate-packet auxiliary fields, and scan
 * encoder. The display report is retained independently.
 *
 * @param[in,out] service Attached-wheel service to update.
 * @param[in] report Shared auxiliary report.
 */
void wheel_service_set_auxiliary_report(WheelService *service, uint16_t report) {
    if (service == 0) {
        return;
    }
    service->auxiliary_output.report = report;
    for (uint8_t channel = 0; channel < WHEEL_VIBRATION_CHANNEL_COUNT; channel++) {
        uint8_t value = channel == 0 ? (uint8_t)report : (uint8_t)(report >> 8);
        service->protocol.mode_one_output.vibration[channel] = value;
    }
    service->protocol.alternate_output.display.auxiliary = (uint8_t)report;
    service->protocol.alternate_output.auxiliary_status = ((report >> 8) & 1u) != 0;
}

/**
 * @brief Applies the shared two-byte display report.
 *
 * Stores the report used by attached-wheel response builders and raises the one-shot status-memory
 * marker used by report 0x0F.
 *
 * @param[in,out] service Attached-wheel service to update.
 * @param[in] report Shared display report.
 */
void wheel_service_set_display_report(WheelService *service, uint16_t report) {
    if (service == NULL) {
        return;
    }
    service->protocol.adapter_output.display_report = report;
    service->protocol.crc_output.status_update_pending = true;
}

/**
 * @brief Publishes the visible attached-wheel display output.
 *
 * Applies the same glyph and marker output to each negotiated packet-family encoder while
 * preserving the shared auxiliary report.
 *
 * @param[in,out] service Attached-wheel service to update.
 * @param[in] output Visible display glyphs and marker state to send.
 */
static void publish_display_output(WheelService *service, WheelDisplayOutput output) {
    service->display_output = output;
    WheelPacketModeOneOutput mode_one_output = service->protocol.mode_one_output;
    mode_one_output.display = output;
    service->protocol.mode_one_output = mode_one_output;
    WheelPacketModeFourOutput mode_four_output = service->protocol.mode_four_output;
    mode_four_output.display = output;
    service->protocol.mode_four_output = mode_four_output;
    WheelPacketCrcOutput crc_output = service->protocol.crc_output;
    crc_output.display = output;
    service->protocol.crc_output = crc_output;
    uint8_t alternate_auxiliary = service->protocol.alternate_output.display.auxiliary;
    service->protocol.alternate_output.display = output;
    service->protocol.alternate_output.display.auxiliary = alternate_auxiliary;
    service->protocol.adapter_output.display = output;
}

/**
 * @brief Publishes the current temporary attached-wheel display page.
 *
 * Overlays its three glyphs on the retained default output so unrelated auxiliary state remains
 * available while the temporary page owns the display.
 *
 * @param[in,out] service Attached-wheel service publishing the temporary page.
 */
static void publish_display_overlay(WheelService *service) {
    WheelDisplayOutput output = service->default_display_output;
    for (uint8_t index = 0; index < WHEEL_DISPLAY_GLYPH_COUNT; index++) {
        output.glyphs[index] = service->display_overlay.output.glyphs[index];
    }
    output.third_glyph_marker = service->display_overlay.output.third_glyph_marker;
    publish_display_output(service, output);
}

/**
 * @brief Publishes the highest-priority attached-wheel display page.
 *
 * Gives an explicit interaction override priority over a timed command presentation and the
 * retained default page.
 *
 * @param[in,out] service Attached-wheel service publishing its visible page.
 */
static void publish_visible_display(WheelService *service) {
    if (service->display_override_active) {
        publish_display_output(service, service->display_override_output);
    } else if (service->display_overlay.active) {
        publish_display_overlay(service);
    } else {
        publish_display_output(service, service->default_display_output);
    }
}

/**
 * @brief Updates the default output state sent to the attached wheel.
 *
 * Retains page-zero glyph and marker changes while a temporary page is active. Otherwise publishes
 * them immediately to every negotiated packet-family encoder.
 *
 * @param[in,out] service Attached-wheel service to update.
 * @param[in] output Default display glyphs and marker state to retain.
 */
void wheel_service_set_display_output(WheelService *service, const WheelDisplayOutput *output) {
    service->default_display_output = *output;
    if (!service->display_overlay.active && !service->display_override_active) {
        publish_display_output(service, *output);
    }
}

/**
 * @brief Returns the mutable default attached-wheel display output.
 *
 * Provides page-zero state to normal display producers even while a temporary page is visible.
 *
 * @param[in,out] service Attached-wheel service retaining both display pages.
 * @return Mutable default display output.
 */
WheelDisplayOutput *wheel_service_default_display_output(WheelService *service) {
    return &service->default_display_output;
}

/**
 * @brief Publishes an interaction-owned attached-wheel display page.
 *
 * Retains the newest complete override and presents it ahead of timed command and default pages.
 * Changes to lower-priority pages continue to accumulate for restoration.
 *
 * @param[in,out] service Attached-wheel service receiving the override.
 * @param[in] output Complete interaction display output.
 */
void wheel_service_set_display_override(WheelService *service, const WheelDisplayOutput *output) {
    if (service == NULL || output == NULL) {
        return;
    }
    service->display_override_output = *output;
    service->display_override_active = true;
    publish_display_output(service, *output);
}

/**
 * @brief Releases the interaction-owned attached-wheel display page.
 *
 * Restores an active timed command presentation or the newest retained default page.
 *
 * @param[in,out] service Attached-wheel service releasing the override.
 */
void wheel_service_clear_display_override(WheelService *service) {
    if (service == NULL || !service->display_override_active) {
        return;
    }
    service->display_override_active = false;
    publish_visible_display(service);
}

/**
 * @brief Starts a temporary attached-wheel command presentation.
 *
 * Replaces any active page-one presentation, restarts its timing, and immediately publishes its
 * initial glyphs without changing the retained default page.
 *
 * @param[in,out] service Attached-wheel service receiving the command.
 * @param[in] command Command byte selecting the temporary presentation.
 * @param[in] now_ms Current monotonic time in milliseconds.
 */
void wheel_service_begin_display_overlay(WheelService *service, uint8_t command, uint32_t now_ms) {
    wheel_display_overlay_begin(&service->display_overlay, command, now_ms);
    if (!service->display_override_active) {
        publish_display_overlay(service);
    }
}

/**
 * @brief Advances the temporary attached-wheel command presentation.
 *
 * Advances countdown glyphs while page one is active and publishes them unless an interaction
 * override owns the output. When the deadline passes, restores the highest-priority lower page.
 *
 * @param[in,out] service Attached-wheel service advancing the temporary presentation.
 * @param[in] now_ms Current monotonic time in milliseconds.
 * @return True when the visible display output changed.
 */
bool wheel_service_update_display_overlay(WheelService *service, uint32_t now_ms) {
    if (!wheel_display_overlay_update(&service->display_overlay, now_ms)) {
        return false;
    }
    if (service->display_override_active) {
        return false;
    }
    publish_visible_display(service);
    return true;
}

/**
 * @brief Reports whether the temporary attached-wheel display page owns the output.
 *
 * Returns the current overlay ownership state without advancing its timing.
 *
 * @param[in] service Attached-wheel service retaining the temporary page.
 * @return True while a temporary command presentation is active.
 */
bool wheel_service_display_overlay_active(const WheelService *service) {
    return service->display_overlay.active;
}

/**
 * @brief Sets attached-wheel vibration output.
 *
 * Converts both vibration channel amplitudes into the shared auxiliary report and propagates it to
 * each packet family, the alternate packet, the scan encoder, and the adapter display report.
 *
 * @param[in,out] service Attached-wheel service to update.
 * @param[in] output Two attached-wheel vibration channels.
 */
void wheel_service_set_vibration_output(WheelService *service, const WheelVibrationOutput *output) {
    wheel_service_set_auxiliary_report(service, (uint16_t)output->channels[0] |
                                                    (uint16_t)output->channels[1] << 8);
}

/**
 * @brief Applies the attached-wheel auxiliary output option.
 *
 * Normalizes the host value to Boolean state and suppresses scan and alternate-packet output when
 * the normalized option is enabled.
 *
 * @param[in,out] service Attached-wheel service to update.
 * @param[in] option Attached-wheel auxiliary option; zero enables output and any nonzero value
 * disables it.
 */
void wheel_service_set_auxiliary_output_option(WheelService *service, uint8_t option) {
    if (service == NULL) {
        return;
    }
    uint8_t normalized = option != 0;
    service->auxiliary_output.option = normalized;
    service->protocol.alternate_output.suppress_auxiliary_display = normalized != 0;
}

/**
 * @brief Sets exclusive auxiliary output ownership.
 *
 * Retains exclusive selection while local tuning owns the attached-wheel presentation and clears
 * transient auxiliary-band latches whenever ownership ends.
 *
 * @param[in,out] service Wheel service receiving the ownership state.
 * @param[in] enabled True while local tuning owns auxiliary output.
 */
void wheel_service_set_auxiliary_exclusive_mode(WheelService *service, bool enabled) {
    if (service == NULL) {
        return;
    }
    service->auxiliary_output.exclusive_mode = enabled;
    if (!enabled) {
        service->auxiliary_output.latched_bands = 0;
    }
}

/**
 * @brief Updates the legacy axes sent to the attached wheel.
 *
 * Copies both axis bytes to each packet family that publishes the shared legacy-axis fields.
 *
 * @param[in,out] service Attached-wheel service to update.
 * @param[in] axes Two legacy-axis bytes in attached-wheel response order.
 */
void wheel_service_set_legacy_axes(WheelService *service, const uint8_t axes[2]) {
    if (service == NULL || axes == NULL) {
        return;
    }
    for (uint8_t axis = 0; axis < 2; axis++) {
        service->protocol.mode_one_output.legacy_axes[axis] = axes[axis];
        service->protocol.mode_four_output.legacy_axes[axis] = axes[axis];
        service->protocol.crc_output.legacy_axes[axis] = axes[axis];
    }
}

/**
 * @brief Updates the legacy pedal status returned to the attached wheel.
 *
 * Forwards both status bytes to the protocol response state without changing pedal axes.
 *
 * @param[in,out] service Attached-wheel service to update.
 * @param[in] first First pedal status byte.
 * @param[in] second Second pedal status byte.
 */
void wheel_service_set_legacy_pedal_status(WheelService *service, uint8_t first, uint8_t second) {
    wheel_protocol_set_legacy_pedal_status(&service->protocol, first, second);
}

/**
 * @brief Resets host-controlled attached-wheel protocol outputs.
 *
 * Clears both legacy axes and the shared auxiliary report, then attempts to queue zero-valued
 * compact reports two and one in protocol order; report two remains suppressed while its legacy
 * interface gate is closed. Accepted reports are mirrored to the active adapter transport. Wheels
 * without a tuning display receive a cleared default three-glyph page when no higher- priority
 * display page is active.
 *
 * @param[in,out] service Attached-wheel service and retained protocol outputs.
 */
void wheel_service_reset_host_protocol_outputs(WheelService *service) {
    if (service == NULL) {
        return;
    }
    /** @brief Zero-filled compact report payload used to reset host outputs. */
    static const uint8_t cleared[4] = {0};
    wheel_service_set_auxiliary_report(service, 0);
    wheel_service_set_legacy_axes(service, cleared);
    if (!wheel_service_tuning_display_supported(service) && !service->display_overlay.active &&
        !service->display_override_active) {
        service->default_display_output = (WheelDisplayOutput){0};
        publish_display_output(service, service->default_display_output);
    }

    if (wheel_output_reports_queue_packed(&service->protocol.output_reports, 2, cleared,
                                          service->protocol.mode)) {
        wheel_adapter_command_service_queue_report_two(&service->adapter_commands,
                                                       service->protocol.output_reports.report_two);
    }
    if (wheel_output_reports_queue_packed(&service->protocol.output_reports, 1, cleared,
                                          service->protocol.mode)) {
        wheel_adapter_command_service_queue_report_one(&service->adapter_commands,
                                                       service->protocol.output_reports.report_one);
    }
    wheel_service_set_display_report(service, 0);
}

/**
 * @brief Configures attached-wheel adapter input.
 *
 * Retains adapter buttons, axes, rotary positions, profile flags, mode, connection state, and
 * pending motion used by attached-wheel packet families.
 *
 * @param[in,out] service Attached-wheel service to configure.
 * @param[in] adapter Attached-wheel adapter input.
 */
void wheel_service_set_adapter(WheelService *service, const WheelAdapterInput *adapter) {
    wheel_protocol_set_adapter(&service->protocol, adapter);
}

/**
 * @brief Selects the host-controlled attached-wheel capability.
 *
 * Applies the host capability to subsequent attached-wheel response packets and preserves it
 * across attached-wheel connection discovery.
 *
 * @param[in,out] service Attached-wheel service to configure.
 * @param[in] enabled True to advertise the host-controlled capability.
 */
void wheel_service_set_host_capability(WheelService *service, bool enabled) {
    wheel_protocol_set_host_capability(&service->protocol, enabled);
}

/**
 * @brief Queues a remote-tuning response for the attached wheel.
 *
 * Retains the response only when its selected legacy or extended link matches the currently
 * negotiated remote-tuning wheel mode and the response is representable by the protocol.
 *
 * @param[in,out] service Attached-wheel service that owns protocol output.
 * @param[in] response Remote-tuning link, response code, and value.
 * @return True when the response was queued.
 */
bool wheel_service_queue_remote_tuning_response(WheelService *service,
                                                const RemoteTuningResponse *response) {
    return wheel_protocol_queue_remote_tuning_response(&service->protocol, response);
}

/**
 * @brief Queues a system-owned remote-tuning response.
 *
 * Retains the response across connection discovery in a priority slot separate from host-owned
 * remote-tuning work when its link, code, and value are supported by the protocol.
 *
 * @param[in,out] service Attached-wheel service that owns protocol output.
 * @param[in] response Semantic system-control response.
 * @return True when the response was queued.
 */
bool wheel_service_queue_system_control_response(WheelService *service,
                                                 const RemoteTuningResponse *response) {
    return wheel_protocol_queue_system_control_response(&service->protocol, response);
}

/**
 * @brief Reports whether a remote-tuning response awaits attached-wheel transfer.
 *
 * Tests protocol output without consuming the queued response.
 *
 * @param[in] service Attached-wheel service state.
 * @return True when a supported response is pending.
 */
bool wheel_service_remote_tuning_response_pending(const WheelService *service) {
    return service != 0 && wheel_protocol_remote_tuning_response_pending(&service->protocol);
}

/**
 * @brief Applies an auxiliary-output operating-mode command.
 *
 * Opcode 0x06 normalizes the auxiliary option, opcode 0x07 normalizes code mode, and opcode 0x08
 * updates the independent display report from its high-byte and low-byte parameters.
 *
 * @param[in,out] service Attached-wheel service and output state.
 * @param[in] command Decoded F8 09 operating-mode command.
 * @return True when the command selects an auxiliary-output operation.
 */
bool wheel_service_apply_auxiliary_output_command(WheelService *service,
                                                  const UsbOperatingModeCommand *command) {
    if (service == NULL || command == NULL) {
        return false;
    }
    if (command->opcode == WHEEL_AUXILIARY_OPTION_OPCODE) {
        wheel_service_set_auxiliary_output_option(service, command->parameters[0]);
        return true;
    }
    if (command->opcode == WHEEL_AUXILIARY_CODE_MODE_OPCODE) {
        service->auxiliary_output.code_mode = command->parameters[0] != 0;
        return true;
    }
    if (command->opcode != WHEEL_AUXILIARY_REPORT_OPCODE) {
        return false;
    }

    wheel_service_set_display_report(service, (uint16_t)command->parameters[1] |
                                                  (uint16_t)command->parameters[0] << 8);
    return true;
}

/**
 * @brief Applies a host multi-position reporting override.
 *
 * Routes selector 0x16 to the capability state retained across attached-wheel reconnections.
 *
 * @param[in,out] service Attached-wheel service and capability state.
 * @param[in] command Decoded F8 09 operating-mode command.
 * @return True when the command selects the multi-position override.
 */
bool wheel_service_apply_multi_position_command(WheelService *service,
                                                const UsbOperatingModeCommand *command) {
    return service != NULL &&
           wheel_capability_apply_multi_position_command(&service->protocol.capabilities, command);
}

/**
 * @brief Applies a compact host output-report update.
 *
 * Routes operating-mode opcode 0x0A to report two and opcode 0x0B to report one. The four command
 * parameters expand into the retained attached-wheel payload, and accepted reports are mirrored
 * to the matching standard-adapter command. A gated legacy report-two command is consumed without
 * changing either destination.
 *
 * @param[in,out] service Attached-wheel service and adapter command state.
 * @param[in] command Decoded F8 09 operating-mode command.
 * @return True when the command selects a compact output report.
 */
bool wheel_service_apply_packed_report_command(WheelService *service,
                                               const UsbOperatingModeCommand *command) {
    if (service == NULL || command == NULL ||
        (command->opcode != WHEEL_PACKED_REPORT_TWO_OPCODE &&
         command->opcode != WHEEL_PACKED_REPORT_ONE_OPCODE)) {
        return false;
    }
    uint8_t report = command->opcode == WHEEL_PACKED_REPORT_TWO_OPCODE ? 2u : 1u;
    if (!wheel_output_reports_queue_packed(&service->protocol.output_reports, report,
                                           command->parameters, service->protocol.mode)) {
        return true;
    }
    if (report == 2) {
        wheel_adapter_command_service_queue_report_two(&service->adapter_commands,
                                                       service->protocol.output_reports.report_two);
        service->protocol.adapter_output.display_report = 0;
    } else {
        wheel_adapter_command_service_queue_report_one(&service->adapter_commands,
                                                       service->protocol.output_reports.report_one);
    }
    return true;
}

/**
 * @brief Applies a host report-six update.
 *
 * Routes operating-mode opcode 0x0D parameters zero and three into the shared report-four payload,
 * then queues attached-wheel report six and the corresponding extended-adapter write.
 *
 * @param[in,out] service Attached-wheel service and adapter command state.
 * @param[in] command Decoded F8 09 operating-mode command.
 * @return True when the command carries a report-six update.
 */
bool wheel_service_apply_report_six_command(WheelService *service,
                                            const UsbOperatingModeCommand *command) {
    if (service == NULL || command == NULL || command->opcode != 0x0d) {
        return false;
    }
    uint8_t first = command->parameters[0];
    uint8_t second = command->parameters[3];
    wheel_output_reports_queue_six(&service->protocol.output_reports, first, second);
    wheel_adapter_command_service_queue_report_six(&service->adapter_commands, first, second);
    return true;
}

/**
 * @brief Applies a host interface-mode gate update.
 *
 * Routes operating-mode opcode 0x0E parameter zero into the normalized legacy wheel interface
 * state.
 *
 * @param[in,out] service Attached-wheel service and retained output state.
 * @param[in] command Decoded F8 09 operating-mode command.
 * @return True when the command carries an interface-mode gate update.
 */
bool wheel_service_apply_interface_mode_command(WheelService *service,
                                                const UsbOperatingModeCommand *command) {
    if (service == NULL || command == NULL || command->opcode != 0x0e) {
        return false;
    }
    wheel_output_reports_set_interface_mode_gate(&service->protocol.output_reports,
                                                 command->parameters[0] != 0);
    return true;
}

/**
 * @brief Updates the local legacy wheel interface-mode shortcut.
 *
 * When a request is ready in wheel modes 0x0F and 0x17, samples the current secondary buttons and
 * advances the retained interface gate chord latch and deadline.
 *
 * @param[in,out] service Attached-wheel service and retained output state.
 * @param[in] now_ms Current monotonic time in milliseconds.
 */
void wheel_service_update_interface_mode_gate(WheelService *service, uint32_t now_ms) {
    if (service == NULL || !service->protocol.request_ready ||
        (service->protocol.mode != WHEEL_MODE_LEGACY_ALTERNATE &&
         service->protocol.mode != WHEEL_MODE_LEGACY_COMPATIBILITY)) {
        return;
    }
    uint16_t secondary_buttons =
        (uint16_t)service->protocol.request[WHEEL_INPUT_SECONDARY_OFFSET] |
        ((uint16_t)service->protocol.request[WHEEL_INPUT_SECONDARY_OFFSET + 1] << 8);
    wheel_output_reports_update_interface_mode_gate(&service->protocol.output_reports,
                                                    secondary_buttons, now_ms);
}

/**
 * @brief Resolves the attached wheel's effective multi-position reporting mode.
 *
 * Combines the active profile setting with the retained host override, negotiated wheel mode, and
 * attached-adapter state.
 *
 * @param[in] service Attached-wheel service and capability state.
 * @param[in] configured_mode Multi-position mode from the active tuning profile.
 * @return Effective reporting-mode byte.
 */
uint8_t wheel_service_multi_position_mode(const WheelService *service,
                                          TuningMultiPositionMode configured_mode) {
    if (service == NULL) {
        return TUNING_MULTI_POSITION_ENCODER;
    }
    return wheel_capability_multi_position_mode(&service->protocol.capabilities, configured_mode,
                                                service->protocol.mode,
                                                service->protocol.adapter.connected);
}

/**
 * @brief Reports whether the attached wheel supplies multi-position input.
 *
 * Applies the negotiated wheel-mode and attached-adapter rules used by the multi-position report
 * path.
 *
 * @param[in] service Attached-wheel service and capability state.
 * @return True when multi-position input reporting is supported.
 */
bool wheel_service_multi_position_supported(const WheelService *service) {
    return service != NULL && wheel_capability_multi_position_supported(
                                  service->protocol.mode, service->protocol.adapter.connected);
}

/**
 * @brief Tests whether the attached adapter supplies rotary positions.
 *
 * Selects the wheel modes that replace the direct rotary bytes with the three adapter selectors
 * while the adapter is connected.
 *
 * @param[in] service Attached-wheel service and adapter state.
 * @return True when adapter rotary positions are the active source.
 */
static bool adapter_supplies_multi_position_input(const WheelService *service) {
    if (!service->protocol.adapter.connected) {
        return false;
    }
    switch (service->protocol.mode) {
    case 4:
    case 6:
    case 12:
    case WHEEL_MODE_CRC_AUTHENTICATED:
        return true;
    default:
        return false;
    }
}

/**
 * @brief Tests whether the third multi-position channel is exposed.
 *
 * Enables the third direct selector for its three wheel modes and the third adapter selector for
 * adapter mode one.
 *
 * @param[in] service Attached-wheel service and adapter state.
 * @param[in] adapter_source True when rotary positions come from the attached adapter.
 * @return True when the third channel contributes to the input report.
 */
static bool third_multi_position_channel_active(const WheelService *service, bool adapter_source) {
    return service->protocol.mode == WHEEL_MODE_LEGACY_ALTERNATE ||
           service->protocol.mode == WHEEL_MODE_LEGACY_COMPATIBILITY ||
           service->protocol.mode == WHEEL_MODE_REMOTE_TUNING_EXTENDED ||
           (adapter_source && service->protocol.adapter.mode == 1);
}

/**
 * @brief Builds the current multi-position rotary input.
 *
 * Selects direct protocol positions or adapter selectors, advances the three selector transition
 * channels, advances the fourth direct rotary channel from the packed high nibble in legacy
 * remote-tuning mode, and marks the alternate selector layout used by extended remote-tuning
 * wheels.
 *
 * @param[in,out] service Attached-wheel service and rotary transition state.
 * @param[in] now_ms Current monotonic time in milliseconds.
 * @param[out] input Three logical selector channels and selector-layout state.
 * @return True when a supported attached-wheel request is available.
 */
bool wheel_service_multi_position_input(WheelService *service, uint32_t now_ms,
                                        WheelMultiPositionInput *input) {
    if (service == NULL || input == NULL) {
        return false;
    }
    *input = (WheelMultiPositionInput){0};
    const uint8_t *request = wheel_protocol_request(&service->protocol);
    if (request == NULL) {
        return false;
    }

    bool adapter_source = adapter_supplies_multi_position_input(service);
    if (adapter_source) {
        for (uint8_t channel = 0; channel < WHEEL_MULTI_POSITION_CHANNEL_COUNT; channel++) {
            input->channels[channel].position = service->protocol.adapter.rotary_positions[channel];
        }
    } else {
        input->channels[0].position = request[WHEEL_MULTI_POSITION_PRIMARY_OFFSET];
        input->channels[1].position = request[WHEEL_MULTI_POSITION_SECONDARY_OFFSET];
        input->channels[2].position = request[WHEEL_MULTI_POSITION_PACKED_OFFSET] & 0x0fu;
    }

    input->channels[0].active = true;
    input->channels[1].active = true;
    input->channels[2].active = third_multi_position_channel_active(service, adapter_source);
    input->remap_selectors = service->protocol.mode == WHEEL_MODE_REMOTE_TUNING_EXTENDED;
    for (uint8_t channel = 0; channel < WHEEL_MULTI_POSITION_CHANNEL_COUNT; channel++) {
        if (input->channels[channel].active) {
            input->channels[channel].event = wheel_rotary_input_update(
                &service->rotary_input, channel, input->channels[channel].position, now_ms);
        }
    }
    if (!adapter_source && service->protocol.mode == WHEEL_MODE_REMOTE_TUNING_LEGACY) {
        uint8_t quaternary_position = request[WHEEL_MULTI_POSITION_PACKED_OFFSET] >> 4;
        (void)wheel_rotary_input_update(&service->rotary_input,
                                        WHEEL_ROTARY_INPUT_CHANNEL_COUNT - 1, quaternary_position,
                                        now_ms);
    }
    return true;
}

/**
 * @brief Applies a host-provided attached-wheel output report.
 *
 * Uses the negotiated wheel mode, current adapter mode, and interface gate to retain or
 * suppress the selected report for the next protocol response. Accepted reports are also
 * forwarded to the matching standard- or extended-adapter command service.
 *
 * @param[in,out] service Attached-wheel service that owns the report queue.
 * @param[in] arguments Action byte followed by the report payload.
 */
void wheel_service_apply_output_report(WheelService *service, const uint8_t *arguments) {
    if (service == NULL || arguments == NULL ||
        !wheel_output_reports_apply(&service->protocol.output_reports, arguments,
                                    service->protocol.mode, service->protocol.adapter.mode)) {
        return;
    }
    if (arguments[0] == WHEEL_OUTPUT_REPORT_ACTION_TWO) {
        wheel_adapter_command_service_queue_report_two(&service->adapter_commands, arguments + 1);
    } else if (arguments[0] == WHEEL_OUTPUT_REPORT_ACTION_ONE) {
        wheel_adapter_command_service_queue_report_one(&service->adapter_commands, arguments + 1);
    } else if (arguments[0] == WHEEL_OUTPUT_REPORT_ACTION_FOUR) {
        wheel_adapter_command_service_queue_report_four(&service->adapter_commands, arguments + 1);
    } else if (arguments[0] == WHEEL_OUTPUT_REPORT_ACTION_FIVE) {
        wheel_adapter_command_service_queue_report_five(&service->adapter_commands, arguments + 1);
    }
}

/**
 * @brief Queues a tuning-menu report for the attached wheel.
 *
 * Retains the complete report 17 payload in the wheel protocol and restarts its segmented transfer.
 *
 * @param[in,out] service Attached-wheel service that owns the report queue.
 * @param[in] payload Complete 61-byte report payload.
 */
void wheel_service_queue_report_seventeen(
    WheelService *service, const uint8_t payload[WHEEL_OUTPUT_REPORT_SEVENTEEN_SIZE]) {
    wheel_output_reports_queue_seventeen(&service->protocol.output_reports, payload);
}

/**
 * @brief Queues an H-pattern calibration state for the attached wheel.
 *
 * Retains the official type-0x16 shifter-state payload in the protocol output scheduler.
 *
 * @param[in,out] service Wheel service owning protocol output.
 * @param[in] state Three-byte shifter-state payload.
 */
void wheel_service_queue_shifter_calibration_state(WheelService *service, const uint8_t state[3]) {
    if (service != NULL) {
        wheel_output_reports_queue_shifter_state(&service->protocol.output_reports, state);
    }
}

/**
 * @brief Queues one remote telemetry report for the attached wheel.
 *
 * Retains the complete report only when no earlier telemetry report remains pending in the wheel
 * protocol output scheduler.
 *
 * @param[in,out] service Attached-wheel service that owns the report queue.
 * @param[in] payload Complete 30-byte telemetry report.
 * @return True when the report was queued.
 */
bool wheel_service_queue_remote_telemetry(
    WheelService *service, const uint8_t payload[WHEEL_OUTPUT_REMOTE_TELEMETRY_SIZE]) {
    return service != 0 && wheel_protocol_queue_remote_telemetry(&service->protocol, payload);
}

/**
 * @brief Reports whether remote telemetry awaits attached-wheel transfer.
 *
 * Tests the wheel protocol output scheduler without consuming a transmission.
 *
 * @param[in] service Attached-wheel service state.
 * @return True while a telemetry report remains queued.
 */
bool wheel_service_remote_telemetry_pending(const WheelService *service) {
    return service != 0 && wheel_protocol_remote_telemetry_pending(&service->protocol);
}

/**
 * @brief Applies active-profile button illumination to the attached wheel.
 *
 * Retains the normalized setting in the wheel protocol so compatible remote-tuning wheel modes
 * receive it after higher-priority output transfers.
 *
 * @param[in,out] service Attached-wheel service that owns the output scheduler.
 * @param[in] enabled True to enable attached-wheel button illumination.
 */
void wheel_service_set_button_illumination(WheelService *service, bool enabled) {
    wheel_output_reports_set_button_illumination(&service->protocol.output_reports, enabled);
}

/**
 * @brief Activates a legacy host-interface presentation on the attached wheel.
 *
 * Requires a wheel-side tuning display, replaces the direct wheel presentation cycle, and mirrors
 * modes one through three to an active extended adapter with zero-length commands 0x20 through
 * 0x22.
 *
 * @param[in,out] service Attached-wheel service receiving the presentation request.
 * @param[in] mode Requested legacy host-interface presentation mode.
 * @return True when the active wheel supports tuning-display presentation.
 */
bool wheel_service_activate_interface_presentation(WheelService *service, uint8_t mode) {
    if (service == NULL || !wheel_service_tuning_display_supported(service)) {
        return false;
    }
    wheel_output_reports_activate_interface_presentation(&service->protocol.output_reports, mode);
    if (mode >= 1 && mode <= 3 && service->protocol.adapter.connected &&
        service->protocol.adapter.mode == 1) {
        wheel_adapter_command_service_queue_interface_presentation(&service->adapter_commands,
                                                                   mode);
    }
    return true;
}

/**
 * @brief Selects character or raw-segment output for mode-nine wheel displays.
 *
 * Retains the translation mode used when the protocol encodes subsequent mode-nine display data.
 *
 * @param[in,out] service Attached-wheel service to configure.
 * @param[in] enabled True to translate mode-nine glyphs to protocol characters.
 */
void wheel_service_set_display_character_mode(WheelService *service, bool enabled) {
    if (service != NULL) {
        wheel_protocol_set_display_character_mode(&service->protocol, enabled);
    }
}

/**
 * @brief Applies active-profile display rotation to the attached wheel.
 *
 * Retains the profile flag and current signed angle for legacy remote-tuning wheel responses.
 *
 * @param[in,out] service Attached-wheel service that owns the protocol state.
 * @param[in] enabled True to include display rotation output.
 * @param[in] angle Signed angle in hundredths of a degree.
 */
void wheel_service_set_display_rotation(WheelService *service, bool enabled, int16_t angle) {
    wheel_protocol_set_display_rotation(&service->protocol, enabled, angle);
}

/**
 * @brief Queues a system status code for the attached wheel.
 *
 * Retains the code across connection discovery and publishes it through the next active command-2
 * exchange.
 *
 * @param[in,out] service Attached-wheel service that owns protocol output.
 * @param[in] code System status code to publish.
 */
void wheel_service_queue_system_status(WheelService *service, uint16_t code) {
    wheel_protocol_queue_system_status(&service->protocol, code);
}

/**
 * @brief Advances attached-wheel protocol traffic.
 *
 * Applies a completed type-two or type-three response, maintains protocol activity state, and
 * starts the next wheel exchange when the shared serial scheduler grants the slot.
 *
 * @param[in,out] service Attached-wheel service to advance.
 * @param[in] now_ms Current monotonic time in milliseconds.
 * @param[in] start_allowed Allows a new wheel request to claim the shared serial service.
 */
void wheel_service_run(WheelService *service, uint32_t now_ms, bool start_allowed) {
    if (service->transport == 0 || service->transport->status == SERIAL_SERVICE_PENDING) {
        return;
    }
    if (service->bridge_recovery_pending) {
        return;
    }
    if (service->transport->status != SERIAL_SERVICE_IDLE &&
        service->transport->request_type != WHEEL_PROTOCOL_TRANSPORT_COMMAND &&
        service->transport->request_type != WHEEL_BUTTON_COMMAND) {
        return;
    }
    if (service->transport->status == SERIAL_SERVICE_SUCCEEDED) {
        const SerialMessageAssembly *response = serial_service_response(service->transport);
        if (service->request_kind == WHEEL_SERVICE_REQUEST_PROTOCOL) {
            if (response == 0 || response->length != WHEEL_PROTOCOL_PACKET_SIZE) {
                serial_service_release(service->transport);
                reset_connection(service);
            } else {
                bool selecting = service->protocol.phase == WHEEL_PROTOCOL_SELECTING;
                wheel_protocol_accept(&service->protocol, response->data);
                service->protocol_transport_interval_ms = protocol_transport_interval(service);
                if (service->protocol.phase == WHEEL_PROTOCOL_WAITING) {
                    service->protocol_deadline_active = false;
                }
                if (selecting && response->data[0] == WHEEL_PROTOCOL_COMMAND_SELECT_MODE) {
                    uint8_t mode = wheel_service_mode(service);
                    service->status_memory_startup_pending = mode == 0x0au || mode == 0x1cu;
                    service->tuning_menu_override_enabled = false;
                    service->tuning_menu_override_value = false;
                }
                service->protocol_exchange_completed = true;
                if ((response->data[WHEEL_PROTOCOL_FLAGS_OFFSET] & WHEEL_PROTOCOL_REQUEST_READY) !=
                        0 &&
                    protocol_exchange_active(service) &&
                    !(selecting && service->protocol.phase == WHEEL_PROTOCOL_SELECTING)) {
                    refresh_protocol_deadline(service, now_ms);
                }
                serial_service_release(service->transport);
            }
        } else if (service->request_kind == WHEEL_SERVICE_REQUEST_BUTTONS) {
            apply_scan_response(service, response);
            serial_service_release(service->transport);
        } else {
            serial_service_release(service->transport);
        }
    } else if (service->transport->status == SERIAL_SERVICE_FAILED) {
        serial_service_release(service->transport);
        reset_connection(service);
    }

    if (service->protocol_deadline_active && protocol_exchange_active(service) &&
        !service->status_memory_startup_pending &&
        platform_time_reached(now_ms, service->protocol_deadline_ms)) {
        if (service->protocol.phase == WHEEL_PROTOCOL_SELECTING &&
            service->protocol.selection_recovery_pending) {
            service->protocol.selection_recovery_pending = false;
            service->protocol_deadline_active = false;
            service->bridge_recovery_pending = true;
            return;
        }
        reset_connection(service);
    }
    if (!start_allowed) {
        return;
    }

    if (scan_active(service)) {
        start_scan(service, now_ms);
    } else {
        (void)start_protocol(service, now_ms, false);
    }
}

/**
 * @brief Starts a command-two exchange independently of the negotiated input mode.
 *
 * Claims an idle wheel serial service with the current protocol response so bridge preparation can
 * continue even when normal input uses command-three scanning.
 *
 * @param[in,out] service Attached-wheel service starting the exchange.
 * @param[in] now_ms Current monotonic time in milliseconds.
 * @return True when the exchange started; otherwise false.
 */
bool wheel_service_start_protocol_exchange(WheelService *service, uint32_t now_ms) {
    return service != NULL && service->transport != NULL &&
           service->transport->status == SERIAL_SERVICE_IDLE &&
           start_protocol(service, now_ms, true);
}

/**
 * @brief Takes the completed command-two exchange event.
 *
 * Reports one successfully received 57-byte protocol response and clears the retained event.
 *
 * @param[in,out] service Attached-wheel service holding the completion event.
 * @return True once for each completed exchange; otherwise false.
 */
bool wheel_service_take_protocol_exchange_completed(WheelService *service) {
    if (service == NULL || !service->protocol_exchange_completed) {
        return false;
    }
    service->protocol_exchange_completed = false;
    return true;
}

/**
 * @brief Takes the unknown-selection bridge recovery event.
 *
 * Reports and clears the recovery event after the command deadline expires without a recognized
 * selection command.
 *
 * @param[in,out] service Wheel service holding the recovery event.
 * @return True once for each timed-out unknown selection; otherwise false.
 */
bool wheel_service_take_bridge_recovery(WheelService *service) {
    if (service == NULL || !service->bridge_recovery_pending) {
        return false;
    }
    service->bridge_recovery_pending = false;
    return true;
}

/**
 * @brief Reports whether unknown-selection bridge recovery is pending.
 *
 * Checks the one-shot recovery notification without consuming it.
 *
 * @param[in] service Wheel service to inspect.
 * @return True while bridge recovery awaits the runtime owner; otherwise false.
 */
bool wheel_service_bridge_recovery_pending(const WheelService *service) {
    return service != NULL && service->bridge_recovery_pending;
}

/**
 * @brief Reports whether selected-wheel startup is waiting for status memory.
 *
 * @param[in] service Wheel service to inspect.
 * @return True after selecting mode 0x0A or 0x1C until startup status completes.
 */
bool wheel_service_status_memory_startup_pending(const WheelService *service) {
    return service != NULL && service->status_memory_startup_pending;
}

/**
 * @brief Completes selected-wheel status-memory startup.
 *
 * Clears the protocol timeout gate and retains the recovered tuning-menu availability as an
 * explicit capability override. Calls without pending startup work are ignored.
 *
 * @param[in,out] service Wheel service awaiting startup status.
 * @param[in] available Recovered tuning-menu availability.
 */
void wheel_service_finish_status_memory_startup(WheelService *service, bool available) {
    if (service == NULL || !service->status_memory_startup_pending) {
        return;
    }
    service->status_memory_startup_pending = false;
    service->tuning_menu_override_enabled = true;
    service->tuning_menu_override_value = available;
}

/**
 * @brief Returns the current attached-wheel button banks.
 *
 * Selects decoded mode-one, mode-four, display, remapped, packed, or CRC-family packet buttons
 * after the wheel protocol becomes active. Scan-mode wheels use the three filtered button banks
 * assembled from command-3 responses.
 *
 * @param[in] service Attached-wheel service state.
 * @return Three current button bytes.
 */
const uint8_t *wheel_service_buttons(const WheelService *service) {
    const WheelPacketModeOneInput *input = wheel_protocol_mode_one_input(&service->protocol);
    if (input != 0) {
        return input->buttons;
    }
    const WheelPacketModeFourInput *mode_four_input =
        wheel_protocol_mode_four_input(&service->protocol);
    if (mode_four_input != 0) {
        return mode_four_input->buttons;
    }
    const WheelPacketDisplayInput *display_input = wheel_protocol_display_input(&service->protocol);
    if (display_input != 0) {
        return display_input->buttons;
    }
    const WheelPacketRemappedInput *remapped_input =
        wheel_protocol_remapped_input(&service->protocol);
    if (remapped_input != 0) {
        return remapped_input->buttons;
    }
    const WheelPacketPackedInput *packed_input = wheel_protocol_packed_input(&service->protocol);
    if (packed_input != 0) {
        return packed_input->buttons;
    }
    const WheelPacketCrcInput *crc_input = wheel_protocol_crc_input(&service->protocol);
    return crc_input != 0 ? crc_input->buttons : service->button_banks;
}

/**
 * @brief Copies normalized attached-wheel host input fields.
 *
 * Reads the directional byte, sixteen secondary buttons, two clutch paddles, signed tuning input,
 * motion byte, packed rotary positions, and auxiliary bytes from the current thirty-byte request
 * view. The motion field is copied from the same tuning-input byte. Current X and Y axis
 * availability and the separately retained axis-report capability accompany the raw auxiliary
 * bytes. An unavailable request produces a cleared destination.
 *
 * @param[in] service Attached-wheel service state.
 * @param[out] snapshot Normalized host input fields.
 * @return True when a supported attached-wheel request is available.
 */
bool wheel_service_input_snapshot(const WheelService *service, WheelInputSnapshot *snapshot) {
    if (service == 0 || snapshot == 0) {
        return false;
    }
    *snapshot = (WheelInputSnapshot){0};
    if (scan_active(service)) {
        snapshot->directional_buttons = service->button_banks[0];
        snapshot->secondary_buttons =
            (uint16_t)service->button_banks[1] | (uint16_t)service->button_banks[2] << 8;
        return true;
    }
    const uint8_t *request = wheel_protocol_request(&service->protocol);
    if (request == 0) {
        return false;
    }

    snapshot->directional_buttons = request[WHEEL_INPUT_DIRECTIONAL_OFFSET];
    snapshot->secondary_buttons = (uint16_t)request[WHEEL_INPUT_SECONDARY_OFFSET] |
                                  (uint16_t)request[WHEEL_INPUT_SECONDARY_OFFSET + 1] << 8;
    snapshot->clutch_paddles[0] = request[WHEEL_INPUT_CLUTCH_OFFSET];
    snapshot->clutch_paddles[1] = request[WHEEL_INPUT_CLUTCH_OFFSET + 1];
    snapshot->tuning_input = (int8_t)request[WHEEL_INPUT_TUNING_OFFSET];
    snapshot->motion = request[WHEEL_INPUT_TUNING_OFFSET];
    snapshot->packed_rotary_positions = request[WHEEL_MULTI_POSITION_PACKED_OFFSET];
    snapshot->auxiliary_report[0] = request[WHEEL_INPUT_AUXILIARY_OFFSET];
    snapshot->auxiliary_report[1] = request[WHEEL_INPUT_AUXILIARY_OFFSET + 1];
    snapshot->auxiliary_report[2] = request[WHEEL_INPUT_AUXILIARY_OFFSET + 2];
    snapshot->axis_report_enabled = wheel_protocol_axis_report_enabled(&service->protocol);
    const WheelAxisOverrideProcessor *axis = wheel_protocol_axis_overrides(&service->protocol);
    snapshot->axis_availability = (axis->x_available ? 1u : 0u) | (axis->y_available ? 2u : 0u);
    return true;
}

/**
 * @brief Updates alternative-shifter mode from the attached-wheel activation chord.
 *
 * Requires profile context and axis reporting, adds the mode-specific control latch outside legacy
 * alternate mode, debounces the chord, toggles alternative-shifter state, and mirrors the resulting
 * latch into the protocol response.
 *
 * @param[in,out] service Attached-wheel service and alternative-shifter latch state.
 * @param[in] profile_context_pending True while the profile activation context is available.
 * @param[in] now_ms Current monotonic time in milliseconds.
 * @return Enabled, disabled, or unchanged transition state.
 */
WheelAlternativeShifterEvent wheel_service_update_alternative_shifter(WheelService *service,
                                                                      bool profile_context_pending,
                                                                      uint32_t now_ms) {
    uint8_t controls[8];
    const uint8_t *request = wheel_protocol_request(&service->protocol);
    if (request == 0 || !wheel_protocol_axis_report_enabled(&service->protocol)) {
        service->alternative_shifter_enabled = false;
        wheel_protocol_set_button_latch(&service->protocol, false, profile_context_pending);
        return WHEEL_ALTERNATIVE_SHIFTER_UNCHANGED;
    }

    uint16_t secondary_buttons = (uint16_t)request[WHEEL_INPUT_SECONDARY_OFFSET] |
                                 (uint16_t)request[WHEEL_INPUT_SECONDARY_OFFSET + 1] << 8;
    bool activation_input =
        profile_context_pending && (secondary_buttons & WHEEL_ALTERNATIVE_SHIFTER_BUTTONS) ==
                                       WHEEL_ALTERNATIVE_SHIFTER_BUTTONS;
    if (service->protocol.mode != WHEEL_MODE_LEGACY_ALTERNATE) {
        activation_input = activation_input && wheel_service_controls(service, controls) &&
                           (controls[3] & WHEEL_ALTERNATIVE_SHIFTER_LATCH_FLAGS) ==
                               WHEEL_ALTERNATIVE_SHIFTER_LATCH_FLAGS;
    }

    WheelAlternativeShifterEvent event = WHEEL_ALTERNATIVE_SHIFTER_UNCHANGED;
    if (!activation_input) {
        service->alternative_shifter_debounced = false;
    } else if (!service->alternative_shifter_debounced &&
               (int32_t)(now_ms - service->alternative_shifter_deadline_ms) > 0) {
        service->alternative_shifter_enabled = !service->alternative_shifter_enabled;
        service->alternative_shifter_deadline_ms = now_ms + WHEEL_ALTERNATIVE_SHIFTER_DEBOUNCE_MS;
        service->alternative_shifter_debounced = true;
        event = service->alternative_shifter_enabled ? WHEEL_ALTERNATIVE_SHIFTER_ENABLED
                                                     : WHEEL_ALTERNATIVE_SHIFTER_DISABLED;
    }

    wheel_protocol_set_button_latch(&service->protocol, service->alternative_shifter_enabled,
                                    profile_context_pending);
    return event;
}

/**
 * @brief Reports whether alternative-shifter mode is enabled.
 *
 * Returns the debounced mode state without modifying the activation latch or deadline.
 *
 * @param[in] service Attached-wheel service state.
 * @return True while alternative-shifter mode is enabled.
 */
bool wheel_service_alternative_shifter_enabled(const WheelService *service) {
    return service->alternative_shifter_enabled;
}

/**
 * @brief Returns the attached wheel's axis-limit value.
 *
 * Reads the axis-limit byte retained from the current supported packet-family input report.
 *
 * @param[in] service Attached-wheel service state.
 * @return Current axis-limit value, or zero when unavailable.
 */
uint8_t wheel_service_axis_limit(const WheelService *service) {
    return wheel_protocol_axis_limit(&service->protocol);
}

/**
 * @brief Returns the attached wheel's secondary button byte.
 *
 * Reads the mode-button field retained by the current supported packet-family input report.
 *
 * @param[in] service Attached-wheel service state.
 * @return Current secondary button byte, or zero when unavailable.
 */
uint8_t wheel_service_mode_buttons(const WheelService *service) {
    if (service == 0) {
        return 0;
    }
    if (scan_active(service)) {
        return (uint8_t)(expected_scan_response(service) >> 5);
    }
    return wheel_protocol_mode_buttons(&service->protocol);
}

/**
 * @brief Returns one debounced direct-wheel rotary event.
 *
 * Reads the latest event without consuming it. The fourth direct channel is updated from the
 * packed rotary byte's high nibble by wheel_service_multi_position_input().
 *
 * @param[in] service Wheel service and rotary state to inspect.
 * @param[in] channel Zero-based rotary channel index.
 * @return Current event, or WHEEL_ROTARY_EVENT_NONE for an invalid service or channel.
 */
WheelRotaryEvent wheel_service_rotary_event(const WheelService *service, uint8_t channel) {
    if (service == 0 || channel >= WHEEL_ROTARY_INPUT_CHANNEL_COUNT) {
        return WHEEL_ROTARY_EVENT_NONE;
    }
    return service->rotary_input.channels[channel].event;
}

/**
 * @brief Returns the attached wheel's two clutch-paddle bytes.
 *
 * Selects the current axis-output bytes that feed the two clutch-paddle report fields.
 *
 * @param[in] service Attached-wheel service state.
 * @return Two clutch-paddle bytes, or null when unavailable.
 */
const uint8_t *wheel_service_clutch_paddles(const WheelService *service) {
    return wheel_protocol_axis_outputs(&service->protocol);
}

/**
 * @brief Reports whether native clutch-paddle fields are available.
 *
 * Applies the official mode, dual-analog-paddle, and adapter-connected availability gate.
 *
 * @param[in] service Attached-wheel service state.
 * @return True when native clutch-paddle values should be copied; otherwise false.
 */
bool wheel_service_clutch_paddles_available(const WheelService *service) {
    if (service == NULL) {
        return false;
    }

    switch (service->protocol.mode) {
    case 0x01:
    case 0x02:
    case 0x03:
    case 0x0a:
    case 0x13:
    case 0x14:
    case 0x16:
        return true;
    default:
        return service->protocol.configured_axis_override_mode ==
                   WHEEL_AXIS_OVERRIDE_MODE_MULTIPLEXED ||
               service->protocol.adapter.connected;
    }
}

/**
 * @brief Reports whether the attached wheel enabled its axis report.
 *
 * Returns the capability flag from the current supported wheel packet family.
 *
 * @param[in] service Attached-wheel service state.
 * @return True while the current input packet enables its axis report.
 */
bool wheel_service_axis_report_enabled(const WheelService *service) {
    return wheel_protocol_axis_report_enabled(&service->protocol);
}

/**
 * @brief Returns the current attached adapter input.
 *
 * Exposes the logical adapter buttons, axes, mode, motion, and connection state retained by the
 * CRC-family wheel protocol.
 *
 * @param[in] service Attached-wheel service state.
 * @return Current attached adapter input.
 */
const WheelAdapterInput *wheel_service_adapter(const WheelService *service) {
    return &service->protocol.adapter;
}

/**
 * @brief Returns the retained protocol callback report identifier.
 *
 * Maps the startup-selected adapter endpoint to the report identifier used by the protocol
 * callback transfer.
 *
 * @param[in] service Wheel service containing startup endpoint state.
 * @return Retained protocol callback report identifier, or zero when unavailable.
 */
uint8_t wheel_service_protocol_bridge_report_id(const WheelService *service) {
    if (service == NULL || service->adapter_commands.endpoint_index > 1) {
        return 0;
    }
    return service->adapter_commands.endpoint_index == 0 ? WHEEL_PROTOCOL_BRIDGE_REPORT_ID_STANDARD
                                                         : WHEEL_PROTOCOL_BRIDGE_REPORT_ID_EXTENDED;
}

/**
 * @brief Copies the attached wheel's two 16-bit axis values.
 *
 * Returns the values retained from the current supported packet-family input report.
 *
 * @param[in] service Attached-wheel service state.
 * @param[out] values Two 16-bit axis values, cleared when unavailable.
 * @return True when axis values were available.
 */
bool wheel_service_axis_values(const WheelService *service, uint16_t values[2]) {
    return wheel_protocol_axis_values(&service->protocol, values);
}

/**
 * @brief Returns the attached wheel's pedal-axis overrides.
 *
 * Exposes the four logical override channels produced by the current wheel axis mode.
 *
 * @param[in] service Attached-wheel service state.
 * @return Current pedal and auxiliary override channels.
 */
const WheelAxisOverrides *wheel_service_axis_overrides(const WheelService *service) {
    return &service->protocol.axis_override_processor.overrides;
}

/**
 * @brief Copies the attached wheel's eight control bytes.
 *
 * Returns the normalized controls from the current supported packet-family input report.
 *
 * @param[in] service Attached-wheel service state.
 * @param[out] controls Eight control bytes, cleared when unavailable.
 * @return True when controls were available.
 */
bool wheel_service_controls(const WheelService *service, uint8_t controls[8]) {
    return wheel_protocol_controls(&service->protocol, controls);
}

/**
 * @brief Reports whether attached-wheel extended fields feed the input report.
 *
 * Suppresses the extended fields for authenticated CRC wheels and while modes four or six use the
 * attached adapter. Other supported packet modes retain their extended fields.
 *
 * @param[in] service Attached-wheel service and adapter state.
 * @return True when extended attached-wheel fields contribute to the input report.
 */
bool wheel_service_extended_report_fields(const WheelService *service) {
    if (wheel_protocol_request(&service->protocol) == 0) {
        return false;
    }
    uint8_t mode = service->protocol.mode;
    if (mode == WHEEL_MODE_CRC_AUTHENTICATED) {
        return false;
    }
    return !service->protocol.adapter.connected || (mode != 4 && mode != 6);
}

/**
 * @brief Returns the attached-wheel accessory flags.
 *
 * Reads the low-nibble accessory field retained in the normalized request view.
 *
 * @param[in] service Attached-wheel service state.
 * @return Current accessory flags, or zero when input is unavailable.
 */
uint8_t wheel_service_accessory_flags(const WheelService *service) {
    const uint8_t *request = wheel_protocol_request(&service->protocol);
    return request != 0 ? request[WHEEL_ACCESSORY_FLAGS_OFFSET] & 0x0fu : 0;
}

/**
 * @brief Returns the queued attached-wheel encoder direction.
 *
 * Inspects the motion accumulated by valid wheel protocol reports without consuming it.
 *
 * @param[in] service Attached-wheel service state.
 * @return Negative one, zero, or positive one.
 */
int8_t wheel_service_encoder_direction(const WheelService *service) {
    return wheel_protocol_motion_direction(&service->protocol);
}

/**
 * @brief Takes one queued attached-wheel encoder step.
 *
 * Consumes one signed step from the motion accumulated by valid wheel protocol reports.
 *
 * @param[in,out] service Attached-wheel service state.
 * @return Negative one, zero, or positive one.
 */
int8_t wheel_service_take_encoder_step(WheelService *service) {
    return wheel_protocol_take_motion(&service->protocol);
}

/**
 * @brief Returns one queued attached-wheel axis motion direction.
 *
 * Inspects the selected protocol motion counter without consuming it.
 *
 * @param[in] service Attached-wheel service state.
 * @param[in] axis Zero-based auxiliary motion axis.
 * @return Negative one, zero, or positive one; zero for an unsupported axis.
 */
int8_t wheel_service_axis_motion_direction(const WheelService *service, uint8_t axis) {
    return wheel_protocol_axis_motion_direction(&service->protocol, axis);
}

/**
 * @brief Takes one queued attached-wheel axis motion step.
 *
 * Consumes one signed step from the selected protocol motion counter.
 *
 * @param[in,out] service Attached-wheel service state.
 * @param[in] axis Zero-based auxiliary motion axis.
 * @return Negative one, zero, or positive one; zero for an unsupported axis.
 */
int8_t wheel_service_take_axis_motion(WheelService *service, uint8_t axis) {
    return wheel_protocol_take_axis_motion(&service->protocol, axis);
}

/**
 * @brief Takes the latest attached-wheel remote-tuning controls.
 *
 * Copies the retained 30-byte control payload and clears its one-shot pending latch.
 *
 * @param[in,out] service Attached-wheel service state.
 * @param[out] output Destination for the complete control payload.
 * @return True when a pending payload was copied.
 */
bool wheel_service_take_remote_tuning_controls(WheelService *service, uint8_t output[30]) {
    return service != NULL &&
           wheel_protocol_take_remote_tuning_controls(&service->protocol, output);
}

/**
 * @brief Discards motion accumulated while tuning owns attached-wheel controls.
 *
 * Clears protocol motion and rotary transition state so tuning navigation cannot emerge later as
 * a delayed host input event.
 *
 * @param[in,out] service Attached-wheel protocol and rotary input state.
 */
void wheel_service_discard_host_motion(WheelService *service) {
    if (service == NULL) {
        return;
    }
    wheel_motion_init(&service->protocol.motion);
    wheel_rotary_input_init(&service->rotary_input);
}

/**
 * @brief Reports attached-wheel input eligible to acknowledge a display overlay.
 *
 * Uses mode-one, mode-four, display, remapped, packed, or CRC-family directional, button, and
 * auxiliary input state. Scan-mode wheels report active when any filtered button bank is nonzero.
 *
 * @param[in] service Attached-wheel service state.
 * @return True while an eligible input is active.
 */
bool wheel_service_acknowledgement_input_active(const WheelService *service) {
    if (wheel_protocol_mode_one_input(&service->protocol) != 0 ||
        wheel_protocol_mode_four_input(&service->protocol) != 0 ||
        wheel_protocol_display_input(&service->protocol) != 0 ||
        wheel_protocol_remapped_input(&service->protocol) != 0 ||
        wheel_protocol_packed_input(&service->protocol) != 0 ||
        wheel_protocol_crc_input(&service->protocol) != 0) {
        return wheel_protocol_acknowledgement_input_active(&service->protocol);
    }
    for (uint8_t bank = 0; bank < WHEEL_BUTTON_BANK_COUNT; bank++) {
        if (service->button_banks[bank] != 0) {
            return true;
        }
    }
    return false;
}

/**
 * @brief Reports the attached-wheel H-pattern calibration input.
 *
 * Uses adapter button bank two bit zero for a connected mode-one adapter. Otherwise, uses wheel
 * button bank three bit zero for modes 0x0E, 0x0F, and 0x17, and bank two bit seven for other
 * modes.
 *
 * @param[in] service Attached-wheel service state.
 * @return True while the mode-specific calibration input is active.
 */
bool wheel_service_calibration_advance_input_active(const WheelService *service) {
    if (service->protocol.adapter.connected && service->protocol.adapter.mode == 1) {
        return (service->protocol.adapter.buttons[1] & 0x01u) != 0;
    }
    const uint8_t *buttons = wheel_service_buttons(service);
    uint8_t mode = service->protocol.mode;
    if (mode == WHEEL_MODE_REMOTE_TUNING_LEGACY || mode == WHEEL_MODE_LEGACY_ALTERNATE ||
        mode == WHEEL_MODE_LEGACY_COMPATIBILITY) {
        return (buttons[2] & 0x01u) != 0;
    }
    return (buttons[1] & 0x80u) != 0;
}

/**
 * @brief Reports the protocol-only H-pattern calibration completion input.
 *
 * Local shifter modes use button-bank three bit zero and all other modes use button-bank two bit
 * seven. The adapter-specific advance path is not part of completion semantics.
 *
 * @param[in] service Attached-wheel service state.
 * @return True while the protocol completion button is active.
 */
bool wheel_service_calibration_completion_input_active(const WheelService *service) {
    if (service == NULL) {
        return false;
    }
    const uint8_t *buttons = wheel_service_buttons(service);
    uint8_t mode = service->protocol.mode;
    if (mode == WHEEL_MODE_REMOTE_TUNING_LEGACY || mode == WHEEL_MODE_LEGACY_ALTERNATE ||
        mode == WHEEL_MODE_LEGACY_COMPATIBILITY) {
        return (buttons[2] & 0x01u) != 0;
    }
    return (buttons[1] & 0x80u) != 0;
}

/**
 * @brief Reports whether an attached adapter is connected.
 *
 * Returns the connection state retained from the attached adapter report.
 *
 * @param[in] service Attached-wheel service state.
 * @return True while the adapter is connected.
 */
bool wheel_service_adapter_connected(const WheelService *service) {
    return service->protocol.adapter.connected;
}

/**
 * @brief Reports whether the connected adapter requests the host capability.
 *
 * Reads bit seven of the adapter's first button byte only while the adapter is connected.
 *
 * @param[in] service Attached-wheel service state.
 * @return True while a connected adapter asserts the host-capability request bit.
 */
bool wheel_service_adapter_requests_host_capability(const WheelService *service) {
    return service->protocol.adapter.connected &&
           (service->protocol.adapter.buttons[0] & 0x80u) != 0;
}

/**
 * @brief Returns the attached-wheel capability flags.
 *
 * Reads the retained capability word assembled from the current attached-wheel report.
 *
 * @param[in] service Attached-wheel service and capability state.
 * @return Current attached-wheel capability flags.
 */
uint16_t wheel_service_capability_flags(const WheelService *service) {
    return wheel_protocol_capabilities(&service->protocol)->capability_flags;
}

/**
 * @brief Reports whether the Xbox host capability is enabled.
 *
 * Returns the persistent host-capability state applied to attached-wheel response packets.
 *
 * @param[in] service Attached-wheel service state.
 * @return True after the host enables the capability and until it disables or resets it.
 */
bool wheel_service_host_capability_enabled(const WheelService *service) {
    return service->protocol.host_capability_enabled;
}

/**
 * @brief Reports whether the attached wheel exposes calibration controls.
 *
 * Returns the effective capability after applying the negotiated wheel mode's forced availability
 * rules to the report capability bit.
 *
 * @param[in] service Attached-wheel service and capability state.
 * @return True when attached-wheel calibration is available.
 */
bool wheel_service_calibration_available(const WheelService *service) {
    return wheel_protocol_capabilities(&service->protocol)->calibration_available;
}

/**
 * @brief Reports whether the Torque Key acknowledgement can grant full strength.
 *
 * Prevents acknowledgement while scan-mode button polling is active or the attached wheel exposes
 * calibration controls. The acknowledgement can be requested again after both conditions clear.
 *
 * @param[in] service Attached-wheel service and capability state.
 * @return True when the Torque Key prompt can grant full strength.
 */
bool wheel_service_torque_key_acknowledgement_available(const WheelService *service) {
    return !scan_active(service) && !wheel_service_calibration_available(service);
}

/**
 * @brief Reports whether the attached wheel exposes the tuning menu.
 *
 * Applies the negotiated wheel mode's inherent and report-driven availability rules to the retained
 * capability state.
 *
 * @param[in] service Attached-wheel service and capability state.
 * @return True when tuning-menu operation is available.
 */
bool wheel_service_tuning_menu_available(const WheelService *service) {
    if (service->tuning_menu_override_enabled) {
        return service->tuning_menu_override_value;
    }
    return wheel_capability_tuning_menu_available(wheel_protocol_capabilities(&service->protocol),
                                                  wheel_service_mode(service));
}

/**
 * @brief Reports the effective attached-wheel input capability.
 *
 * Applies the negotiated wheel mode's report eligibility to the retained input-capability latch.
 *
 * @param[in] service Attached-wheel service and capability state.
 * @return True when the current wheel mode exposes a latched input capability.
 */
bool wheel_service_input_capability_available(const WheelService *service) {
    return wheel_capability_input_available(wheel_protocol_capabilities(&service->protocol),
                                            service->protocol.mode);
}

/**
 * @brief Reports whether attached-wheel force-output mode selection is complete.
 *
 * Accepts authenticated packet traffic and either command-three scan mode. Connection handshake
 * and mode selection remain interlocked.
 *
 * @param[in] service Attached-wheel service and protocol phase.
 * @return True when the selected wheel protocol can proceed to force-output confirmation.
 */
bool wheel_service_force_output_ready(const WheelService *service) {
    WheelProtocolPhase phase = service->protocol.phase;
    return phase == WHEEL_PROTOCOL_AUTHENTICATING || phase == WHEEL_PROTOCOL_ACTIVE ||
           phase == WHEEL_PROTOCOL_SCANNING_PRIMARY || phase == WHEEL_PROTOCOL_SCANNING_SECONDARY;
}

/**
 * @brief Reports whether the selected wheel requires the motor transition interlock.
 *
 * Raises the interlock throughout discovery, synchronization, acknowledgement, and mode selection,
 * or after an invalid active command.
 *
 * @param[in] service Attached-wheel service and protocol phase.
 * @return True while negotiation, rejection, or an invalid command requires the interlock.
 */
bool wheel_service_force_output_transition_active(const WheelService *service) {
    return service->protocol.phase <= WHEEL_PROTOCOL_SELECTING ||
           service->protocol.phase == WHEEL_PROTOCOL_UNSUPPORTED ||
           service->protocol.command_invalid;
}

/**
 * @brief Returns the negotiated attached-wheel mode.
 *
 * Reads the mode selected by the attached-wheel protocol handshake.
 *
 * @param[in] service Attached-wheel service state.
 * @return Current attached-wheel mode identifier.
 */
uint8_t wheel_service_mode(const WheelService *service) { return service->protocol.mode; }

/**
 * @brief Returns the attached-wheel protocol phase.
 *
 * Reads the current handshake or active-traffic phase maintained by the wheel protocol.
 *
 * @param[in] service Attached-wheel service state.
 * @return Current attached-wheel protocol phase.
 */
WheelProtocolPhase wheel_service_protocol_phase(const WheelService *service) {
    return service->protocol.phase;
}

/**
 * @brief Reports whether startup discovery reached the active protocol phase.
 *
 * Startup discovery remains in its protocol wait while command-three scan traffic is active. The
 * scan tail is serviced only after the discovery deadline or an active protocol transition.
 *
 * @param[in] service Wheel service to inspect.
 * @return True when startup discovery can leave its protocol wait; otherwise false.
 */
bool wheel_service_startup_discovery_complete(const WheelService *service) {
    if (service == NULL) {
        return false;
    }
    return service->protocol.phase == WHEEL_PROTOCOL_ACTIVE;
}
