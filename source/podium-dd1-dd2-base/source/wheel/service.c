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
};

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

static void clear_scan_filter(WheelService *service) {
    for (uint8_t bank = 0; bank < WHEEL_BUTTON_BANK_COUNT; bank++) {
        service->button_banks[bank] = 0;
        for (uint8_t sample = 0; sample < WHEEL_SCAN_SAMPLE_DEPTH; sample++) {
            service->scan_samples[sample][bank] = 0;
        }
    }
    service->scan_sample_index = 0;
}

static void reset_connection(WheelService *service) {
    WheelPacketModeOneButtonFilter mode_one_button_filter =
        service->protocol.mode_one_button_filter;
    WheelPacketModeOneControlAxisFilter mode_one_control_axis_filter =
        service->protocol.mode_one_control_axis_filter;
    WheelPacketModeOneOutput mode_one_output = service->protocol.mode_one_output;
    WheelPacketModeFourFilter mode_four_filter = service->protocol.mode_four_filter;
    WheelPacketModeFourOutput mode_four_output = service->protocol.mode_four_output;
    WheelPacketCrcFilter crc_filter = service->protocol.crc_filter;
    WheelPacketCrcOutput crc_output = service->protocol.crc_output;
    WheelPacketCrcAdapter crc_adapter = service->protocol.crc_adapter;
    WheelOutputReports output_reports = service->protocol.output_reports;
    WheelCapabilityState capabilities = service->protocol.capabilities;
    uint8_t interface_mode = service->protocol.interface_mode;
    uint8_t axis_override_mode = service->protocol.configured_axis_override_mode;
    uint8_t axis_calibration_value = service->protocol.axis_calibration_value;
    uint8_t axis_multiplex_phase = service->protocol.axis_override_processor.multiplex_phase;
    bool axis_x_available = service->protocol.axis_override_processor.x_available;
    bool axis_y_available = service->protocol.axis_override_processor.y_available;
    bool packet_axis_report_enabled =
        service->protocol.axis_override_processor.packet_axis_report_enabled;
    bool button_latch_enabled = service->protocol.button_latch_enabled;
    bool profile_transition_pending = service->protocol.profile_transition_pending;
    wheel_protocol_init(&service->protocol);
    service->protocol.mode_one_button_filter = mode_one_button_filter;
    service->protocol.mode_one_control_axis_filter = mode_one_control_axis_filter;
    service->protocol.mode_one_output = mode_one_output;
    service->protocol.mode_four_filter = mode_four_filter;
    service->protocol.mode_four_output = mode_four_output;
    service->protocol.crc_filter = crc_filter;
    service->protocol.crc_output = crc_output;
    service->protocol.crc_adapter = crc_adapter;
    service->protocol.output_reports = output_reports;
    service->protocol.capabilities = capabilities;
    wheel_protocol_set_axis_processing(&service->protocol, interface_mode, axis_override_mode,
                                       axis_calibration_value);
    service->protocol.axis_override_processor.multiplex_phase = axis_multiplex_phase;
    service->protocol.axis_override_processor.x_available = axis_x_available;
    service->protocol.axis_override_processor.y_available = axis_y_available;
    service->protocol.axis_override_processor.packet_axis_report_enabled =
        packet_axis_report_enabled;
    wheel_protocol_set_button_latch(&service->protocol, button_latch_enabled,
                                    profile_transition_pending);
    clear_scan_filter(service);
    service->protocol_deadline_ms = 0;
    service->protocol_deadline_active = false;
    service->scan_phase = 0;
}

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

static bool scan_active(const WheelService *service) {
    return service->protocol.phase == WHEEL_PROTOCOL_SCANNING_PRIMARY ||
           service->protocol.phase == WHEEL_PROTOCOL_SCANNING_SECONDARY;
}

/**
 * @brief Initializes attached-wheel protocol service state.
 *
 * Attaches the shared serial service and resets protocol, display, scan filter, request, and
 * activity-deadline state.
 *
 * @param[out] service Wheel service to initialize.
 * @param[in,out] transport Shared serial service used for type-two and type-three traffic.
 */
void wheel_service_init(WheelService *service, SerialService *transport) {
    service->transport = transport;
    wheel_protocol_init(&service->protocol);
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
 * @brief Configures the attached-wheel CRC packet adapter.
 *
 * Retains the adapter identifier, mode, and variant used to encode and decode CRC-family packets.
 *
 * @param[in,out] service Attached-wheel service to configure.
 * @param[in] adapter CRC packet adapter configuration.
 */
void wheel_service_set_crc_adapter(WheelService *service, const WheelPacketCrcAdapter *adapter) {
    wheel_protocol_set_crc_adapter(&service->protocol, adapter);
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
 * Selects decoded mode-one, mode-four, or CRC-family packet buttons after the wheel protocol
 * becomes active. Scan-mode wheels use the three filtered button banks assembled from command-3
 * responses.
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
    const WheelPacketCrcInput *crc_input = wheel_protocol_crc_input(&service->protocol);
    return crc_input != 0 ? crc_input->buttons : service->button_banks;
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
 * @brief Takes one queued attached-wheel encoder step.
 *
 * Consumes one signed step from the motion accumulated by valid wheel protocol reports.
 *
 * @param[in,out] service Attached-wheel service state.
 * @return Negative one, zero, or positive one.
 */
int8_t wheel_service_take_encoder_delta(WheelService *service) {
    return wheel_protocol_take_motion(&service->protocol);
}

/**
 * @brief Reports attached-wheel input eligible to acknowledge a display overlay.
 *
 * Uses mode-one, mode-four, or CRC-family directional, button, and auxiliary input state. Scan-mode
 * wheels report active when any filtered button bank is nonzero.
 *
 * @param[in] service Attached-wheel service state.
 * @return True while an eligible input is active.
 */
bool wheel_service_acknowledgement_input_active(const WheelService *service) {
    if (wheel_protocol_mode_one_input(&service->protocol) != 0 ||
        wheel_protocol_mode_four_input(&service->protocol) != 0 ||
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
