#include "wheel/service.h"

#include <stdbool.h>
#include <stdint.h>

#include "platform/time.h"
#include "serial/message.h"
#include "serial/service.h"
#include "wheel/display_output.h"
#include "wheel/output_reports.h"
#include "wheel/protocol.h"

enum {
    WHEEL_PROTOCOL_TRANSPORT_COMMAND = 2,
    WHEEL_BUTTON_COMMAND = 3,
    WHEEL_BUTTON_REQUEST_READY = 1,
    WHEEL_BUTTON_RESPONSE_READY = 2,
    WHEEL_BUTTON_PRIMARY_RESPONSE = 0xe0,
    WHEEL_BUTTON_SECONDARY_RESPONSE = 0xc0,
    WHEEL_BUTTON_RESPONSE_MASK = 0xe0,
    WHEEL_BUTTON_VALUE_MASK = 0x1f,
    WHEEL_PROTOCOL_ACTIVITY_TIMEOUT_MS = 2000,
    WHEEL_MULTI_POSITION_PRIMARY_OFFSET = 6,
    WHEEL_MULTI_POSITION_SECONDARY_OFFSET = 7,
    WHEEL_MULTI_POSITION_PACKED_OFFSET = 14,
    WHEEL_ACCESSORY_FLAGS_OFFSET = 15,
    WHEEL_INPUT_DIRECTIONAL_OFFSET = 0,
    WHEEL_INPUT_SECONDARY_OFFSET = 1,
    WHEEL_INPUT_CLUTCH_OFFSET = 3,
    WHEEL_INPUT_AUXILIARY_OFFSET = 22,
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
static void apply_auxiliary(uint8_t banks[WHEEL_BUTTON_BANK_COUNT], uint8_t sample) {
    assign(&banks[0], 3, sample, 3);
    assign(&banks[0], 1, sample, 4);
    assign(&banks[0], 2, sample, 1);
    assign(&banks[0], 0, sample, 2);
    assign(&banks[2], 2, sample, 0);
}

/**
 * @brief Maps a native-mode phase-1 sample into the base button banks.
 *
 * Applies the first scan-phase bit mapping to one sample set.
 *
 * @param[in,out] banks Three sampled button banks to update.
 * @param[in] sample Five input bits returned by the attached device.
 * @param[in] secondary Adds the secondary-channel mapping for sample bit 1 when true.
 */
static void apply_first(uint8_t banks[WHEEL_BUTTON_BANK_COUNT], uint8_t sample, bool secondary) {
    assign(&banks[2], 5, sample, 0);
    assign(&banks[2], 1, sample, 3);
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
static void publish_scan_samples(WheelService *service) {
    for (uint8_t bank = 0; bank < WHEEL_BUTTON_BANK_COUNT; bank++) {
        service->button_banks[bank] = service->scan_samples[0][bank] &
                                      service->scan_samples[1][bank] &
                                      service->scan_samples[2][bank];
    }
    service->scan_sample_index++;
    if (service->scan_sample_index == WHEEL_SCAN_SAMPLE_DEPTH) {
        service->scan_sample_index = 0;
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
    if (response == 0 || response->length != SERIAL_PACKET_MAX_PAYLOAD_SIZE ||
        (response->data[SERIAL_PACKET_MAX_PAYLOAD_SIZE - 1] & WHEEL_BUTTON_RESPONSE_READY) == 0) {
        return;
    }
    uint8_t encoded = response->data[1];
    uint8_t response_type = encoded & WHEEL_BUTTON_RESPONSE_MASK;
    if (response_type != expected_scan_response(service)) {
        return;
    }
    uint8_t sample = encoded & WHEEL_BUTTON_VALUE_MASK;
    uint8_t *banks = service->scan_samples[service->scan_sample_index];
    switch (service->scan_phase) {
    case WHEEL_SCAN_PHASE_FIRST:
        apply_first(banks, sample, response_type == WHEEL_BUTTON_SECONDARY_RESPONSE);
        break;
    case WHEEL_SCAN_PHASE_SECOND:
        apply_second(banks, sample);
        break;
    case WHEEL_SCAN_PHASE_THIRD:
        apply_third(banks, sample);
        break;
    case WHEEL_SCAN_PHASE_AUXILIARY:
        apply_auxiliary(banks, sample);
        break;
    }
    publish_scan_samples(service);
}

/**
 * @brief Clears scan-mode button filtering.
 *
 * Zeros all three published button banks and their three-sample histories, then resets the sample
 * insertion position.
 *
 * @param[out] service Attached-wheel service whose scan filter is cleared.
 */
static void clear_scan_filter(WheelService *service) {
    for (uint8_t bank = 0; bank < WHEEL_BUTTON_BANK_COUNT; bank++) {
        service->button_banks[bank] = 0;
        for (uint8_t sample = 0; sample < WHEEL_SCAN_SAMPLE_DEPTH; sample++) {
            service->scan_samples[sample][bank] = 0;
        }
    }
    service->scan_sample_index = 0;
}

/**
 * @brief Restarts attached-wheel connection discovery.
 *
 * Reinitializes handshake and input state while preserving configured outputs, input filters,
 * adapter state, report capabilities, pending system status and remote-tuning work, and
 * axis-processing configuration, including an in-progress bite-point adjustment and its pending
 * notifications. The transient input-capability latch, scan input, and activity timing restart
 * from their initial states.
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
    WheelPacketPackedFilter packed_filter = service->protocol.packed_filter;
    WheelPacketCrcFilter crc_filter = service->protocol.crc_filter;
    WheelPacketCrcOutput crc_output = service->protocol.crc_output;
    WheelPacketCrcAdapter crc_adapter = service->protocol.crc_adapter;
    WheelPacketRemoteTuningOutput system_control_output = service->protocol.system_control_output;
    WheelPacketRemoteTuningOutput remote_tuning_output = service->protocol.remote_tuning_output;
    WheelOutputReports output_reports = service->protocol.output_reports;
    WheelCapabilityState capabilities = service->protocol.capabilities;
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
    bool host_capability_enabled = service->protocol.host_capability_enabled;
    bool profile_transition_pending = service->protocol.profile_transition_pending;
    bool system_status_pending = service->protocol.system_status_pending;
    wheel_protocol_init(&service->protocol);
    service->protocol.mode_one_button_filter = mode_one_button_filter;
    service->protocol.mode_one_control_axis_filter = mode_one_control_axis_filter;
    service->protocol.mode_one_output = mode_one_output;
    service->protocol.mode_four_filter = mode_four_filter;
    service->protocol.mode_four_output = mode_four_output;
    service->protocol.packed_filter = packed_filter;
    service->protocol.crc_filter = crc_filter;
    service->protocol.crc_output = crc_output;
    service->protocol.crc_adapter = crc_adapter;
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
    wheel_protocol_set_host_capability(&service->protocol, host_capability_enabled);
    if (system_status_pending) {
        wheel_protocol_queue_system_status(&service->protocol, system_status_code);
    }
    clear_scan_filter(service);
    service->protocol_deadline_ms = 0;
    service->protocol_deadline_active = false;
    service->scan_phase = 0;
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
    return service->protocol.phase == WHEEL_PROTOCOL_AUTHENTICATING ||
           service->protocol.phase == WHEEL_PROTOCOL_ACTIVE;
}

/**
 * @brief Extends the active command-2 exchange deadline.
 *
 * Starts a new two-second activity window after an attached wheel marks a packet ready.
 *
 * @param[in,out] service Wheel service that owns the command-2 exchange.
 * @param[in] now_ms Current monotonic time in milliseconds.
 */
static void refresh_protocol_deadline(WheelService *service, uint32_t now_ms) {
    service->protocol_deadline_ms = now_ms + WHEEL_PROTOCOL_ACTIVITY_TIMEOUT_MS;
    service->protocol_deadline_active = true;
}

/**
 * @brief Starts the next command-three wheel scan.
 *
 * Rotates through scan phases 8, 4, 2, and 1, encodes current display output, marks the request
 * ready, and submits a full 57-byte type-three message.
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
    service->request[1] =
        (uint8_t)~wheel_display_output_encode(&service->display_output, service->scan_phase);
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
 */
static void start_protocol(WheelService *service, uint32_t now_ms) {
    service->request_kind = WHEEL_SERVICE_REQUEST_PROTOCOL;
    if (!serial_service_start(service->transport, WHEEL_PROTOCOL_TRANSPORT_COMMAND,
                              wheel_protocol_response(&service->protocol),
                              WHEEL_PROTOCOL_PACKET_SIZE, now_ms)) {
        reset_connection(service);
    }
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
    wheel_rotary_input_init(&service->rotary_input);
    clear_scan_filter(service);
    for (uint8_t index = 0; index < WHEEL_DISPLAY_GLYPH_COUNT; index++) {
        service->display_output.glyphs[index] = 0;
    }
    service->display_output.auxiliary = 0;
    service->display_output.third_glyph_marker = false;
    service->protocol_deadline_ms = 0;
    service->scan_phase = 0;
    service->request_kind = WHEEL_SERVICE_REQUEST_NONE;
    service->protocol_deadline_active = false;
}

/**
 * @brief Updates the output state sent to the attached wheel.
 *
 * Applies the same display and auxiliary output to each negotiated packet-family encoder.
 *
 * @param[in,out] service Attached-wheel service to update.
 * @param[in] output Display glyphs, auxiliary byte, and marker state to send.
 */
void wheel_service_set_display_output(WheelService *service, const WheelDisplayOutput *output) {
    service->display_output = *output;
    WheelPacketModeOneOutput mode_one_output = service->protocol.mode_one_output;
    mode_one_output.display = *output;
    service->protocol.mode_one_output = mode_one_output;
    WheelPacketModeFourOutput mode_four_output = service->protocol.mode_four_output;
    mode_four_output.display = *output;
    service->protocol.mode_four_output = mode_four_output;
    WheelPacketCrcOutput crc_output = service->protocol.crc_output;
    crc_output.display = *output;
    service->protocol.crc_output = crc_output;
}

/**
 * @brief Updates the vibration output sent to the attached wheel.
 *
 * Applies the same two vibration channels to every negotiated packet-family encoder.
 *
 * @param[in,out] service Attached-wheel service to update.
 * @param[in] output Two attached-wheel vibration channels.
 */
void wheel_service_set_vibration_output(WheelService *service, const WheelVibrationOutput *output) {
    for (uint8_t channel = 0; channel < WHEEL_VIBRATION_CHANNEL_COUNT; channel++) {
        service->protocol.mode_one_output.vibration[channel] = output->channels[channel];
        service->protocol.mode_four_output.vibration[channel] = output->channels[channel];
        service->protocol.crc_output.vibration[channel] = output->channels[channel];
    }
}

/**
 * @brief Updates the legacy axes sent to the attached wheel.
 *
 * Applies the same high-byte and low-byte axis values to each packet family that publishes the
 * shared legacy-axis fields.
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
 * @brief Configures the attached-wheel CRC packet adapter.
 *
 * Retains adapter buttons, axes, rotary positions, mode, connection state, and pending motion used
 * by attached-wheel input processing.
 *
 * @param[in,out] service Attached-wheel service to configure.
 * @param[in] adapter CRC packet adapter configuration.
 */
void wheel_service_set_crc_adapter(WheelService *service, const WheelPacketCrcAdapter *adapter) {
    wheel_protocol_set_crc_adapter(&service->protocol, adapter);
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
 * negotiated remote-tuning wheel mode.
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
 * remote-tuning work.
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
 * @brief Resolves the attached wheel's effective multi-position reporting mode.
 *
 * Combines the active profile setting with the retained host override, negotiated wheel mode, and
 * current attached-wheel input activity.
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
                                                service->protocol.request_ready);
}

/**
 * @brief Reports whether the attached wheel supplies multi-position input.
 *
 * Applies the negotiated wheel-mode rules and current input-transport activity used by the
 * multi-position report path.
 *
 * @param[in] service Attached-wheel service and capability state.
 * @return True when multi-position input reporting is supported.
 */
bool wheel_service_multi_position_supported(const WheelService *service) {
    return service != NULL && wheel_capability_multi_position_supported(
                                  service->protocol.mode, service->protocol.request_ready);
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
    if (!service->protocol.crc_adapter.connected) {
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
           (adapter_source && service->protocol.crc_adapter.mode == 1);
}

/**
 * @brief Builds the current multi-position rotary input.
 *
 * Selects direct protocol positions or adapter selectors, advances the three rotary transition
 * channels, and marks the alternate selector layout used by extended remote-tuning wheels.
 *
 * @param[in,out] service Attached-wheel service and rotary transition state.
 * @param[in] now_ms Current monotonic time in milliseconds.
 * @param[out] input Three logical rotary channels and selector-layout state.
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
            input->channels[channel].position =
                service->protocol.crc_adapter.rotary_positions[channel];
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
    return true;
}

/**
 * @brief Applies a host-provided attached-wheel output report.
 *
 * Uses the negotiated wheel mode, current adapter mode, and display blink state to retain or
 * suppress the selected report for the next protocol response.
 *
 * @param[in,out] service Attached-wheel service that owns the report queue.
 * @param[in] arguments Action byte followed by the report payload.
 * @param[in] display_blink_active True while the local legacy display blink phase is active.
 */
void wheel_service_apply_output_report(WheelService *service, const uint8_t *arguments,
                                       bool display_blink_active) {
    wheel_output_reports_apply(&service->protocol.output_reports, arguments, service->protocol.mode,
                               service->protocol.crc_adapter.mode, display_blink_active);
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
    return service != 0 &&
           wheel_output_reports_queue_remote_telemetry(&service->protocol.output_reports, payload);
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
    return service != 0 &&
           wheel_output_reports_remote_telemetry_pending(&service->protocol.output_reports);
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
    if (service->transport->status != SERIAL_SERVICE_IDLE &&
        service->transport->request_type != WHEEL_PROTOCOL_TRANSPORT_COMMAND &&
        service->transport->request_type != WHEEL_BUTTON_COMMAND) {
        return;
    }
    if (service->transport->status == SERIAL_SERVICE_SUCCEEDED) {
        const SerialMessageAssembly *response = serial_service_response(service->transport);
        if (service->request_kind == WHEEL_SERVICE_REQUEST_PROTOCOL && response != 0 &&
            response->length == WHEEL_PROTOCOL_PACKET_SIZE) {
            wheel_protocol_accept(&service->protocol, response->data);
            if ((response->data[WHEEL_PROTOCOL_FLAGS_OFFSET] & WHEEL_PROTOCOL_REQUEST_READY) != 0 &&
                protocol_exchange_active(service)) {
                refresh_protocol_deadline(service, now_ms);
            }
        } else if (service->request_kind == WHEEL_SERVICE_REQUEST_BUTTONS) {
            apply_scan_response(service, response);
        }
        serial_service_release(service->transport);
    } else if (service->transport->status == SERIAL_SERVICE_FAILED) {
        serial_service_release(service->transport);
        reset_connection(service);
    }

    if (service->protocol_deadline_active && protocol_exchange_active(service) &&
        platform_time_reached(now_ms, service->protocol_deadline_ms)) {
        reset_connection(service);
    }
    if (!start_allowed) {
        return;
    }

    if (scan_active(service)) {
        start_scan(service, now_ms);
    } else {
        start_protocol(service, now_ms);
    }
}

/**
 * @brief Returns the current attached-wheel button banks.
 *
 * Selects decoded mode-one, mode-four, packed, or CRC-family packet buttons after the wheel
 * protocol becomes active. Scan-mode wheels use the three filtered button banks assembled from
 * command-3 responses.
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
 * Reads the directional byte, sixteen secondary buttons, two clutch paddles, and three auxiliary
 * bytes from the current thirty-byte request view. The separately retained axis-report capability
 * accompanies the values. An unavailable request produces a cleared destination.
 *
 * @param[in] service Attached-wheel service state.
 * @param[out] snapshot Normalized host input fields.
 * @return True when a supported attached-wheel request is available.
 */
bool wheel_service_input_snapshot(const WheelService *service, WheelInputSnapshot *snapshot) {
    if (snapshot == 0) {
        return false;
    }
    *snapshot = (WheelInputSnapshot){0};
    const uint8_t *request = wheel_protocol_request(&service->protocol);
    if (request == 0) {
        return false;
    }

    snapshot->directional_buttons = request[WHEEL_INPUT_DIRECTIONAL_OFFSET];
    snapshot->secondary_buttons = (uint16_t)request[WHEEL_INPUT_SECONDARY_OFFSET] |
                                  (uint16_t)request[WHEEL_INPUT_SECONDARY_OFFSET + 1] << 8;
    snapshot->clutch_paddles[0] = request[WHEEL_INPUT_CLUTCH_OFFSET];
    snapshot->clutch_paddles[1] = request[WHEEL_INPUT_CLUTCH_OFFSET + 1];
    snapshot->auxiliary_report[0] = request[WHEEL_INPUT_AUXILIARY_OFFSET];
    snapshot->auxiliary_report[1] = request[WHEEL_INPUT_AUXILIARY_OFFSET + 1];
    snapshot->auxiliary_report[2] = request[WHEEL_INPUT_AUXILIARY_OFFSET + 2];
    snapshot->axis_report_enabled = wheel_protocol_axis_report_enabled(&service->protocol);
    return true;
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
    return wheel_protocol_mode_buttons(&service->protocol);
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
const WheelPacketCrcAdapter *wheel_service_adapter(const WheelService *service) {
    return &service->protocol.crc_adapter;
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
    return !service->protocol.crc_adapter.connected || (mode != 4 && mode != 6);
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
 * @brief Reports attached-wheel input eligible to acknowledge a display overlay.
 *
 * Uses mode-one, mode-four, packed, or CRC-family directional, button, and auxiliary input state.
 * Scan-mode wheels report active when any filtered button bank is nonzero.
 *
 * @param[in] service Attached-wheel service state.
 * @return True while an eligible input is active.
 */
bool wheel_service_acknowledgement_input_active(const WheelService *service) {
    if (wheel_protocol_mode_one_input(&service->protocol) != 0 ||
        wheel_protocol_mode_four_input(&service->protocol) != 0 ||
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
 * Uses button bank three bit zero for wheel modes 0x0E, 0x0F, and 0x17. Other wheel modes use
 * button bank two bit seven.
 *
 * @param[in] service Attached-wheel service state.
 * @return True while the mode-specific calibration input is active.
 */
bool wheel_service_calibration_advance_input_active(const WheelService *service) {
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
 * Returns the connection state retained from the current CRC-family adapter report.
 *
 * @param[in] service Attached-wheel service state.
 * @return True while the adapter is connected.
 */
bool wheel_service_adapter_connected(const WheelService *service) {
    return service->protocol.crc_adapter.connected;
}

/**
 * @brief Reports whether the attached adapter requests the Xbox host capability.
 *
 * Requires an attached CRC adapter and the high status bit carried by its second button byte.
 *
 * @param[in] service Attached-wheel service and adapter state.
 * @return True when the adapter-specific host-capability condition is active.
 */
bool wheel_service_adapter_requests_host_capability(const WheelService *service) {
    return service->protocol.crc_adapter.connected &&
           (service->protocol.crc_adapter.buttons[1] & 0x80u) != 0;
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
 * @brief Reports whether the attached wheel exposes the tuning menu.
 *
 * Applies the negotiated wheel mode's inherent and report-driven availability rules to the retained
 * capability state.
 *
 * @param[in] service Attached-wheel service and capability state.
 * @return True when tuning-menu operation is available.
 */
bool wheel_service_tuning_menu_available(const WheelService *service) {
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
