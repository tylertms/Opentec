#ifndef OPENTEC_BASE_WHEEL_SERVICE_H
#define OPENTEC_BASE_WHEEL_SERVICE_H

#include <stdbool.h>
#include <stdint.h>

#include "serial/service.h"
#include "wheel/adapter_commands.h"
#include "wheel/auxiliary_output.h"
#include "wheel/display_output.h"
#include "wheel/display_overlay.h"
#include "wheel/protocol.h"
#include "wheel/rotary_input.h"
#include "wheel/vibration.h"

enum {
    WHEEL_BUTTON_BANK_COUNT = 3,
    WHEEL_SCAN_SAMPLE_DEPTH = 3,
    WHEEL_MULTI_POSITION_CHANNEL_COUNT = 3,
};

typedef enum {
    WHEEL_SERVICE_REQUEST_NONE,
    WHEEL_SERVICE_REQUEST_PROTOCOL,
    WHEEL_SERVICE_REQUEST_BUTTONS,
} WheelServiceRequest;

typedef struct {
    uint8_t position;
    WheelRotaryEvent event;
    bool active;
} WheelMultiPositionChannel;

typedef struct {
    WheelMultiPositionChannel channels[WHEEL_MULTI_POSITION_CHANNEL_COUNT];
    bool remap_selectors;
} WheelMultiPositionInput;

/** @brief Normalized attached-wheel fields shared by host input formats. */
typedef struct {
    uint16_t secondary_buttons;
    uint8_t directional_buttons;
    uint8_t clutch_paddles[2];
    int8_t tuning_input;
    uint8_t auxiliary_report[3];
    bool axis_report_enabled;
} WheelInputSnapshot;

typedef struct {
    SerialService *transport;
    WheelProtocol protocol;
    WheelAdapterCommandService adapter_commands;
    WheelRotaryInput rotary_input;
    WheelDisplayOutput display_output;
    WheelDisplayOutput default_display_output;
    WheelDisplayOutput display_override_output;
    WheelDisplayOverlay display_overlay;
    WheelAuxiliaryOutput auxiliary_output;
    uint8_t adapter_display_state;
    uint8_t request[SERIAL_PACKET_MAX_PAYLOAD_SIZE];
    uint8_t button_banks[WHEEL_BUTTON_BANK_COUNT];
    uint8_t scan_samples[WHEEL_SCAN_SAMPLE_DEPTH][WHEEL_BUTTON_BANK_COUNT];
    uint32_t protocol_deadline_ms;
    uint8_t scan_phase;
    uint8_t scan_sample_index;
    WheelServiceRequest request_kind;
    bool protocol_deadline_active;
    bool protocol_exchange_completed;
    bool display_override_active;
} WheelService;

void wheel_service_init(WheelService *service, SerialService *transport);
void wheel_service_reset_adapter_commands(WheelService *service);
void wheel_service_run(WheelService *service, uint32_t now_ms, bool start_allowed);
bool wheel_service_start_protocol_exchange(WheelService *service, uint32_t now_ms);
bool wheel_service_take_protocol_exchange_completed(WheelService *service);
void wheel_service_run_adapter_commands(WheelService *service, CommandTransport *transport);
bool wheel_service_take_adapter_host_controls(WheelService *service,
                                              uint8_t output[WHEEL_ADAPTER_HOST_CONTROLS_SIZE]);
void wheel_service_queue_adapter_remote_tuning_active(WheelService *service, bool active);
void wheel_service_queue_adapter_refresh_state(WheelService *service, bool active);
void wheel_service_queue_adapter_setup_selection(WheelService *service, uint8_t selection);
void wheel_service_queue_adapter_display_state(WheelService *service, uint8_t state);
bool wheel_service_queue_tuning_display_command(WheelService *service, uint8_t command);
bool wheel_service_queue_adapter_text_line(WheelService *service, uint8_t line, uint8_t metadata,
                                           const uint8_t *text, uint8_t length);
bool wheel_service_queue_adapter_text_close(WheelService *service);
void wheel_service_set_display_output(WheelService *service, const WheelDisplayOutput *output);
WheelDisplayOutput *wheel_service_default_display_output(WheelService *service);
void wheel_service_set_display_override(WheelService *service, const WheelDisplayOutput *output);
void wheel_service_clear_display_override(WheelService *service);
void wheel_service_begin_display_overlay(WheelService *service, uint8_t command, uint32_t now_ms);
bool wheel_service_update_display_overlay(WheelService *service, uint32_t now_ms);
bool wheel_service_display_overlay_active(const WheelService *service);
void wheel_service_set_vibration_output(WheelService *service, const WheelVibrationOutput *output);
void wheel_service_set_auxiliary_report(WheelService *service, uint16_t report);
void wheel_service_set_auxiliary_output_disabled(WheelService *service, bool disabled);
void wheel_service_set_legacy_axes(WheelService *service, const uint8_t axes[2]);
void wheel_service_reset_host_protocol_outputs(WheelService *service);
void wheel_service_set_adapter(WheelService *service, const WheelAdapterInput *adapter);
void wheel_service_set_host_capability(WheelService *service, bool enabled);
void wheel_service_configure_axis_processing(WheelService *service, uint8_t interface_mode,
                                             uint8_t paddle_mode, uint8_t bite_point_percent,
                                             uint32_t now_ms);
bool wheel_service_take_bite_point(WheelService *service, uint8_t *updated_percent);
bool wheel_service_take_bite_point_report(WheelService *service, uint8_t *updated_percent);
bool wheel_service_bite_point_adjustment(const WheelService *service, uint8_t *percent);
bool wheel_service_queue_remote_tuning_response(WheelService *service,
                                                const RemoteTuningResponse *response);
bool wheel_service_queue_system_control_response(WheelService *service,
                                                 const RemoteTuningResponse *response);
bool wheel_service_remote_tuning_response_pending(const WheelService *service);
bool wheel_service_apply_auxiliary_output_command(WheelService *service,
                                                  const UsbOperatingModeCommand *command);
bool wheel_service_apply_multi_position_command(WheelService *service,
                                                const UsbOperatingModeCommand *command);
bool wheel_service_apply_packed_report_command(WheelService *service,
                                               const UsbOperatingModeCommand *command);
bool wheel_service_apply_report_six_command(WheelService *service,
                                            const UsbOperatingModeCommand *command);
bool wheel_service_apply_interface_mode_command(WheelService *service,
                                                const UsbOperatingModeCommand *command);
void wheel_service_update_interface_mode_gate(WheelService *service, uint32_t now_ms);
uint8_t wheel_service_multi_position_mode(const WheelService *service,
                                          TuningMultiPositionMode configured_mode);
bool wheel_service_multi_position_supported(const WheelService *service);
bool wheel_service_multi_position_input(WheelService *service, uint32_t now_ms,
                                        WheelMultiPositionInput *input);
void wheel_service_apply_output_report(WheelService *service, const uint8_t *arguments);
void wheel_service_queue_report_seventeen(
    WheelService *service, const uint8_t payload[WHEEL_OUTPUT_REPORT_SEVENTEEN_SIZE]);
bool wheel_service_queue_remote_telemetry(
    WheelService *service, const uint8_t payload[WHEEL_OUTPUT_REMOTE_TELEMETRY_SIZE]);
bool wheel_service_remote_telemetry_pending(const WheelService *service);
void wheel_service_set_button_illumination(WheelService *service, bool enabled);
void wheel_service_set_display_rotation(WheelService *service, bool enabled, int16_t angle);
void wheel_service_queue_system_status(WheelService *service, uint16_t code);
const uint8_t *wheel_service_buttons(const WheelService *service);
bool wheel_service_input_snapshot(const WheelService *service, WheelInputSnapshot *snapshot);
uint8_t wheel_service_axis_limit(const WheelService *service);
uint8_t wheel_service_mode_buttons(const WheelService *service);
const uint8_t *wheel_service_clutch_paddles(const WheelService *service);
bool wheel_service_axis_report_enabled(const WheelService *service);
const WheelAdapterInput *wheel_service_adapter(const WheelService *service);
bool wheel_service_axis_values(const WheelService *service, uint16_t values[2]);
const WheelAxisOverrides *wheel_service_axis_overrides(const WheelService *service);
bool wheel_service_controls(const WheelService *service, uint8_t controls[8]);
bool wheel_service_extended_report_fields(const WheelService *service);
uint8_t wheel_service_accessory_flags(const WheelService *service);
int8_t wheel_service_encoder_direction(const WheelService *service);
int8_t wheel_service_take_encoder_step(WheelService *service);
bool wheel_service_acknowledgement_input_active(const WheelService *service);
bool wheel_service_calibration_advance_input_active(const WheelService *service);
bool wheel_service_adapter_connected(const WheelService *service);
bool wheel_service_adapter_requests_host_capability(const WheelService *service);
uint16_t wheel_service_capability_flags(const WheelService *service);
bool wheel_service_host_capability_enabled(const WheelService *service);
bool wheel_service_calibration_available(const WheelService *service);
bool wheel_service_tuning_menu_available(const WheelService *service);
bool wheel_service_tuning_display_supported(const WheelService *service);
bool wheel_service_input_capability_available(const WheelService *service);
uint8_t wheel_service_mode(const WheelService *service);
WheelProtocolPhase wheel_service_protocol_phase(const WheelService *service);

#endif
