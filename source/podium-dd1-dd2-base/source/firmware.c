#include <stdbool.h>
#include <string.h>
#include <xc.h>

#include "analog/auxiliary_axis.h"
#include "board/identity.h"
#include "board/led_pattern.h"
#include "board/power.h"
#include "board/torque_key.h"
#include "cooling/controller.h"
#include "cooling/effect_limit.h"
#include "cooling/tachometer.h"
#include "cooling/temperature.h"
#include "display/auxiliary_calibration_page.h"
#include "display/force_feedback_analysis_page.h"
#include "display/identity_page.h"
#include "display/motor_data_analysis_page.h"
#include "display/notice.h"
#include "display/prompt.h"
#include "display/system_information_page.h"
#include "display/temperature_analysis_page.h"
#include "display/tuning_page.h"
#include "force_feedback/command.h"
#include "force_feedback/output_enable.h"
#include "force_feedback/output_scale.h"
#include "force_feedback/script_report.h"
#include "force_feedback/script_runtime.h"
#include "force_feedback/script_tick.h"
#include "force_feedback/state.h"
#include "motor/calibration.h"
#include "motor/command_channel.h"
#include "motor/command_mailbox.h"
#include "motor/command_serial.h"
#include "motor/command_startup_service.h"
#include "motor/live_frame.h"
#include "motor/output_status.h"
#include "motor/output_transport.h"
#include "motor/probe.h"
#include "motor/rotation_guard.h"
#include "motor/startup_centering.h"
#include "motor/status_service.h"
#include "motor/telemetry_service.h"
#include "motor/tuning_service.h"
#include "pedal/brake_indicator.h"
#include "pedal/calibration_command.h"
#include "pedal/input.h"
#include "pedal/protocol_command.h"
#include "pedal/service.h"
#include "platform/adc.h"
#include "platform/aux_bus.h"
#include "platform/board_identity.h"
#include "platform/clock.h"
#include "platform/cooling.h"
#include "platform/display.h"
#include "platform/force_feedback_timer.h"
#include "platform/led_pattern.h"
#include "platform/motor_link.h"
#include "platform/pedal_link.h"
#include "platform/pin_mux.h"
#include "platform/power.h"
#include "platform/serial_link.h"
#include "platform/shifter.h"
#include "platform/system.h"
#include "platform/time.h"
#include "platform/torque_key.h"
#include "platform/usb.h"
#include "profile/bank.h"
#include "profile/tuning.h"
#include "profile/tuning_display.h"
#include "profile/tuning_entry.h"
#include "profile/tuning_interaction.h"
#include "profile/tuning_menu.h"
#include "secure_element/authentication.h"
#include "secure_element/session.h"
#include "security/code.h"
#include "serial/service.h"
#include "settings/persistence.h"
#include "settings/state.h"
#include "shifter/calibration.h"
#include "shifter/display.h"
#include "shifter/h_pattern.h"
#include "shifter/input.h"
#include "system/control_state.h"
#include "system/event_dispatcher.h"
#include "system/event_queue.h"
#include "system/notice.h"
#include "system/runtime_bridge.h"
#include "system/torque_key_prompt.h"
#include "system/torque_transition.h"
#include "usb/connection.h"
#include "usb/console_descriptor.h"
#include "usb/device.h"
#include "usb/diagnostic_report.h"
#include "usb/fallback_command.h"
#include "usb/fallback_tuning.h"
#include "usb/fanatec_encoder.h"
#include "usb/fanatec_input.h"
#include "usb/feature_report.h"
#include "usb/host_capability_recovery.h"
#include "usb/input_report.h"
#include "usb/motor_vendor_service.h"
#include "usb/operating_mode_command.h"
#include "usb/output_command.h"
#include "usb/playstation_wheel_value.h"
#include "usb/remote_tuning_service.h"
#include "usb/transfer_request.h"
#include "usb/transfer_response.h"
#include "usb/tuning_menu_service.h"
#include "usb/tuning_profile_report.h"
#include "usb/tuning_profile_service.h"
#include "usb/tuning_status_report.h"
#include "usb/updater_service.h"
#include "usb/vendor_command.h"
#include "usb/xbox_gip_command.h"
#include "usb/xbox_gip_input.h"
#include "usb/xbox_gip_vendor_tunnel.h"
#include "wheel/accessory_service.h"
#include "wheel/center_capture.h"
#include "wheel/command_forwarder.h"
#include "wheel/compatibility_alert.h"
#include "wheel/position.h"
#include "wheel/protocol_bridge_service.h"
#include "wheel/service.h"
#include "wheel/startup_display.h"
#include "wheel/startup_version_page.h"
#include "wheel/status_service.h"
#include "wheel/steering_limit.h"
#include "wheel/transfer_service.h"
#include "wheel/usb_bridge_gate.h"
#include "wheel/usb_disconnect_display.h"
#include "wheel/vibration.h"

#pragma config GWRP = OFF
#pragma config GSS = OFF
#pragma config GSSK = OFF
#pragma config FNOSC = FRC
#pragma config IESO = OFF
#pragma config POSCMD = HS
#pragma config OSCIOFNC = OFF
#pragma config IOL1WAY = OFF
#pragma config FCKSM = CSECMD
#pragma config WDTPOST = PS32768
#pragma config WDTPRE = PR128
#pragma config PLLKEN = ON
#pragma config WINDIS = OFF
#pragma config FWDTEN = OFF
#pragma config FPWRT = PWR16
#pragma config BOREN = ON
#pragma config ALTI2C1 = OFF
#pragma config ALTI2C2 = OFF
#pragma config ICS = PGD2
#pragma config RSTPRI = PF
#pragma config JTAGEN = OFF

typedef enum {
    FORCE_FEEDBACK_SCRIPT_REPORT_NONE,
    FORCE_FEEDBACK_SCRIPT_REPORT_AXES,
    FORCE_FEEDBACK_SCRIPT_REPORT_SAMPLES,
    FORCE_FEEDBACK_SCRIPT_REPORT_SLOT,
    FORCE_FEEDBACK_SCRIPT_REPORT_STATUS,
    FORCE_FEEDBACK_SCRIPT_REPORT_VALUES,
} ForceFeedbackScriptReportKind;

/** @brief Phase-specific sources reused while composing one PlayStation input report. */
typedef union {
    WheelInputSnapshot wheel;
    UsbPlaystationButtonInput buttons;
    UsbPlaystationClutchInput clutch;
} UsbPlaystationInputSources;

/** @brief Working state for one PlayStation input report. */
typedef struct {
    UsbPlaystationInputState state;
    UsbPlaystationInputSources sources;
} UsbPlaystationInputWorkspace;

/** @brief Mutually exclusive input state for the active console protocol. */
typedef union {
    UsbXboxGipInputState xbox;
    UsbPlaystationInputWorkspace playstation;
} UsbConsoleInputWorkspace;

static BoardIdentity board_identity;
static MotorProbe motor_probe;
static MotorStartupCentering motor_startup_centering;
static bool motor_startup_direct_force;
static bool motor_startup_wheel_discovery_started;
static uint32_t motor_startup_direct_force_deadline_ms;
static CommandTransport command_transport;
static MotorCommandMailboxExchange motor_command_mailbox;
static MotorCommandChannel motor_command_channel;
static MotorCommandStartupService motor_command_startup_service;
static UsbMotorVendorService usb_motor_vendor_service;
static WheelTransferService wheel_transfer_service;
static WheelAccessoryService wheel_accessory_service;
static MotorStatusService motor_status_service;
static MotorTelemetryService motor_telemetry_service;
static MotorTuningService motor_tuning_service;
static MotorCalibrationService motor_calibration_service;
static MotorRotationGuard motor_rotation_guard;
static BaseSettings base_settings;
static BaseSettingsPersistence settings_persistence;
static TuningProfile runtime_tuning_profile;
static const TuningProfile *tuning_profile;
static MotorTuningContext motor_tuning_context;
static bool motor_tuning_ready;
static bool motor_command_request_pending;
static MotorPositionReport motor_position_report;
static bool motor_position_ready;
static WheelPositionCalibration wheel_position_calibration;
static WheelVelocityEstimator wheel_velocity_estimator;
static WheelCenterCaptureCommand wheel_center_capture_command;
static MotorCalibrationOperation motor_calibration_operation;
static PedalService pedal_service;
static PedalBrakeIndicator pedal_brake_indicator;
static WheelVibrationOutput wheel_vibration_output;
static SerialService serial_service;
static WheelService wheel_service;
static WheelStatusService wheel_status_service;
static WheelCompatibilityAlert wheel_compatibility_alert;
static WheelDisplayOutput wheel_compatibility_display_output;
static AnalogSamples analog_samples;
static AuxiliaryAxis auxiliary_axis;
static HPatternShifter h_pattern_shifter;
static HPatternCalibrationService h_pattern_calibration_service;
static HPatternCalibrationCommand h_pattern_calibration_command;
static ShifterInputState shifter_input;
static ShifterDisplay shifter_display;
static WheelStartupDisplay wheel_startup_display;
static WheelStartupVersionPage wheel_startup_version_page;
static UsbDisconnectDisplay usb_disconnect_display;
static UsbInputReportState usb_input_state;
static FanatecEncoder fanatec_encoder;
static UsbXboxGipInputBuilder xbox_input_builder;
static UsbConsoleInputWorkspace usb_console_input_workspace;
static UsbXboxGipInputSnapshot xbox_input_snapshot;
static UsbXboxGipExtendedStatus xbox_extended_status;
static WheelPositionCalibration xbox_position_calibration;
static uint16_t xbox_steering_range_degrees;
static uint16_t xbox_profile_steering_range_degrees;
static uint8_t xbox_force_feedback_percent;
static uint8_t xbox_profile_force_feedback_percent;
static WheelMultiPositionInput wheel_multi_position_input;
static fanatec_multi_position_input fanatec_multi_position_input_state;
static uint8_t usb_input_report[USB_INPUT_REPORT_MAX_SIZE];
static uint8_t usb_feature_reports[4][USB_DEVICE_REPORT_SIZE];
static uint8_t usb_motor_acknowledgement[USB_DEVICE_REPORT_SIZE];
static uint8_t usb_motor_acknowledgement_length;
static uint8_t usb_vendor_response[USB_DEVICE_REPORT_SIZE];
static uint8_t usb_vendor_response_length;
static UsbDiagnosticReportService usb_diagnostic_report_service;
static UsbTuningStatusReportService usb_tuning_status_report_service;
static UsbRemoteTuningService usb_remote_tuning_service;
static WheelCommandForwarder wheel_command_forwarder;
static WheelProtocolBridgeService wheel_protocol_bridge_service;
static WheelUsbBridgeGate wheel_usb_bridge_gate;
static uint8_t wheel_command_batch[USB_REMOTE_TUNING_FORWARD_BATCH_SIZE];
static uint8_t wheel_command_batch_length;
static RemoteTuningResponse usb_remote_tuning_response;
static RemoteTuningResponse system_wheel_response;
static uint8_t wheel_remote_telemetry_report[REMOTE_TELEMETRY_REPORT_SIZE];
static uint8_t wheel_adapter_host_controls[WHEEL_ADAPTER_HOST_CONTROLS_SIZE];
static bool wheel_adapter_remote_tuning_active;
static bool wheel_adapter_refresh_state;
static uint8_t wheel_adapter_setup_selection;
static uint8_t wheel_adapter_display_state;
static UsbTuningMenuService usb_tuning_menu_service;
static UsbTransferRequest usb_transfer_request;
static UsbTransferResponse usb_transfer_response;
static bool usb_transfer_response_has_pedal_owner;
static UsbTuningProfileService usb_tuning_profile_service;
static TuningInteraction tuning_interaction;
static TuningMenu tuning_menu;
static WheelDisplayOutput tuning_display_output;
static TuningEntryAvailabilityContext tuning_availability;
static TuningEntryAdjustmentContext tuning_adjustment;
static TuningInteractionInput tuning_interaction_input;
static WheelInputSnapshot tuning_interaction_snapshot;
static SecurityCode security_code;
static SecurityCodeInput security_code_input;
static SecurityCodeUpdate security_code_update_state;
static WheelDisplayOutput security_code_display_output;
static UsbDiagnosticSnapshot usb_diagnostic_snapshot;
static UsbTuningStatusSnapshot usb_tuning_status_snapshot;
static UsbDeviceOutputReport usb_device_output_report;
static UsbConnectionMonitor usb_connection_monitor;
static UsbHostCapabilityRecovery usb_host_capability_recovery;
static UsbOutputCommand usb_output_command;
static UsbFallbackCommand usb_fallback_command;
static UsbPlaystationWheelValue usb_playstation_wheel_value;
static UsbPlaystationInputMapper usb_playstation_input_mapper;
static UsbOperatingModeCommand usb_operating_mode_command;
static RuntimeBridge runtime_bridge;
static UsbUpdaterService usb_updater_service;
static UsbRuntimeModeTransition usb_runtime_transition;
static RuntimeBridgeInput runtime_bridge_input;
static UsbUpdaterServiceInput usb_updater_input;
static bool usb_operating_status_enabled;
static uint8_t fallback_interface_mode_count;
static PedalCalibrationCommand pedal_calibration_command;
static PedalCalibrationActions pedal_calibration_actions;
static PedalProtocolCommand pedal_protocol_command;
static WheelSteeringLimitCommand wheel_steering_limit_command;
static uint8_t wheel_adjusted_bite_point_percent;
static uint8_t wheel_bite_point_report_percent;
static UsbVendorCommand usb_vendor_command;
static UsbXboxGipCommand usb_xbox_gip_command;
static UsbWheelTransferCommand usb_wheel_transfer_command;
static ForceFeedbackCommand force_feedback_command;
static ForceFeedbackState force_feedback_state;
static ForceFeedbackScriptSystem force_feedback_script_system;
static ForceFeedbackScriptOutputState force_feedback_script_output_state;
static ForceFeedbackScriptOutputConfig force_feedback_script_output_config;
static uint8_t force_feedback_script_response_sequence;
static ForceFeedbackScriptReportKind force_feedback_script_report_pending;
static uint16_t force_feedback_script_sample_report_index;
static uint8_t force_feedback_script_slot_report_index;
static uint32_t force_feedback_ramp_deadline_ms;
static ForceOutputReport motor_output_report;
static MotorLiveFrame motor_live_frame;
static MotorOutputStatus motor_output_status;
static MotorOutputStatusInput motor_output_status_input;
static MotorOutputTransport motor_output_transport;
static uint8_t motor_received_frame[MOTOR_LIVE_FRAME_SIZE];
static uint8_t motor_transmitted_frame[MOTOR_LIVE_FRAME_SIZE];
static uint8_t motor_malformed_frame_count;
static PowerController power_controller;
static SystemControlState system_control_state;
static SystemTorqueTransition system_torque_transition;
static SystemTorqueTransitionAction system_torque_action;
static uint16_t pending_system_status_code;
static SystemEventQueue system_event_queue;
static SystemEventDispatcher system_event_dispatcher;
static SystemNotice system_notice;
static CoolingController cooling_controller;
static CoolingEffectLimit cooling_effect_limit;
static CoolingEffectStrengths cooling_effect_strengths;
static CoolingTemperatureMonitor cooling_temperature_monitor;
static PlatformFanTachometer fan_tachometer;
static uint16_t fan_speed_rpm[2];
#if defined(__XC16__)
static __eds__ uint8_t display_framebuffer[DISPLAY_FRAMEBUFFER_SIZE] __attribute__((space(eds)));
#else
static uint8_t display_framebuffer[DISPLAY_FRAMEBUFFER_SIZE];
#endif
static DisplayPrompt force_output_display_prompt;
static DisplayPrompt torque_key_display_prompt;
static ForceOutputEnable force_output_enable;
static ForceOutputEnableAction force_output_enable_action;
static bool force_output_enabled;
static bool force_output_prompt_visible;
static LedPatternController led_pattern_controller;
static TorqueKey torque_key;
static TorqueKeyPrompt torque_key_prompt;
static bool torque_key_acknowledgement_available;
static bool torque_key_prompt_visible;
static bool torque_disabled_notice_visible;
static bool usb_disconnect_notice_visible;
static bool usb_motor_acknowledgement_ready;
static bool xbox_mode_startup_attempted;
static bool xbox_mode_startup_finished;
static uint8_t console_wheel_mode;
static uint8_t observed_wheel_mode;
static uint8_t selected_base_mode;
static bool playstation_authentication_response_published;
static bool usb_wheel_transfer_response_pending[WHEEL_TRANSFER_REQUEST_COUNT];
static uint8_t local_display_page;
static uint8_t local_display_rendered_bite_point_percent;
static SystemNoticeKind local_display_rendered_notice_kind;
static uint8_t local_display_tuning_revision;
static uint8_t local_display_rendered_tuning_revision;
static uint8_t wheel_bite_point_display_percent;
static DisplaySystemInformation local_display_system_information;
static DisplaySystemInformation local_display_rendered_system_information;
static uint32_t local_display_system_information_title_deadline_ms;
static bool local_display_system_information_content_active;
static DisplayForceFeedbackAnalysisPage local_display_force_feedback_analysis;
static uint32_t local_display_force_feedback_analysis_title_deadline_ms;
static bool local_display_force_feedback_analysis_content_active;
static DisplayMotorDataAnalysisPage local_display_motor_data_analysis;
static uint32_t local_display_motor_data_analysis_title_deadline_ms;
static bool local_display_motor_data_analysis_content_active;
static DisplayTemperatureAnalysisPage local_display_temperature_analysis;
static uint32_t local_display_temperature_analysis_title_deadline_ms;
static bool local_display_temperature_analysis_content_active;
static DisplayAuxiliaryCalibrationPage local_display_auxiliary_calibration;
static uint32_t local_display_auxiliary_calibration_title_deadline_ms;
static bool local_display_auxiliary_calibration_content_active;

typedef enum {
    USB_VENDOR_RESPONSE_NONE,
    USB_VENDOR_RESPONSE_REMOTE_TUNING,
    USB_VENDOR_RESPONSE_WHEEL_TRANSFER,
    USB_VENDOR_RESPONSE_SCRIPT_REPORT,
    USB_VENDOR_RESPONSE_TUNING_PROFILE,
    USB_VENDOR_RESPONSE_TUNING_MENU,
    USB_VENDOR_RESPONSE_PEDAL_TRANSFER,
    USB_VENDOR_RESPONSE_DIAGNOSTIC,
    USB_VENDOR_RESPONSE_TUNING_STATUS,
    USB_VENDOR_RESPONSE_MOTOR,
} UsbVendorResponseKind;

typedef enum {
    USB_XBOX_CONTROL_RESPONSE_NONE,
    USB_XBOX_CONTROL_RESPONSE_CAPABILITIES,
    USB_XBOX_CONTROL_RESPONSE_SNAPSHOT,
    USB_XBOX_CONTROL_RESPONSE_EXTENDED_STATUS,
    USB_XBOX_CONTROL_RESPONSE_TRANSFER_STATUS,
} UsbXboxControlResponseKind;

static UsbVendorResponseKind usb_vendor_response_kind;
static UsbXboxControlResponseKind usb_xbox_control_response_pending;
static uint8_t usb_xbox_control_request[2];
static WheelTransferRequest usb_vendor_wheel_response_request;

enum {
    FAN_STARTUP_DUTY_PERCENT = 25,
    COOLING_STARTUP_TEMPERATURE_C = 20,
    USB_MOTOR_BUFFER_SIZE = MEMORY_TRANSFER_MAX_READ_SIZE,
    LOCAL_DISPLAY_PAGE_TORQUE_DISABLED = 1,
    LOCAL_DISPLAY_PAGE_FORCE_OUTPUT_PROMPT = 2,
    LOCAL_DISPLAY_PAGE_TORQUE_KEY_PROMPT = 3,
    LOCAL_DISPLAY_PAGE_BITE_POINT = 4,
    LOCAL_DISPLAY_PAGE_SYSTEM_NOTICE = 5,
    LOCAL_DISPLAY_PAGE_TUNING = 6,
    LOCAL_DISPLAY_PAGE_IDENTITY = 7,
    LOCAL_DISPLAY_PAGE_SYSTEM_INFORMATION = 8,
    LOCAL_DISPLAY_PAGE_FORCE_FEEDBACK_ANALYSIS = 9,
    LOCAL_DISPLAY_PAGE_MOTOR_DATA_ANALYSIS = 10,
    LOCAL_DISPLAY_PAGE_TEMPERATURE_ANALYSIS = 11,
    LOCAL_DISPLAY_PAGE_AUXILIARY_CALIBRATION = 12,
    LOCAL_DISPLAY_DIAGNOSTIC_TITLE_MS = 1000,
    USB_DISCONNECT_STATUS_CODE = 0x1c,
    TUNING_MENU_RESET_EVENT_CODE = 1,
    WHEEL_CENTER_CALIBRATED_EVENT_CODE = 2,
    STANDARD_TUNING_MODE_EVENT_CODE = 0x12,
    ADVANCED_TUNING_MODE_EVENT_CODE = 0x13,
    MAXIMUM_ROTATIONS_EXCEEDED_EVENT_CODE = 0x17,
    TORQUE_REDUCED_EVENT_CODE = 6,
    TORQUE_REDUCED_STEERING_WHEEL_EVENT_CODE = 0x16,
    ALTERNATIVE_SHIFTER_ENABLED_EVENT_CODE = 0x20,
    ALTERNATIVE_SHIFTER_DISABLED_EVENT_CODE = 0x21,
    WHEEL_CENTER_CALIBRATED_STATUS_CODE = 0x1f,
    SHUTDOWN_EVENT_CODE = 0x0e,
    SYSTEM_DISPLAY_DISMISS_EVENT_CODE = 0x11,
    FORCE_OUTPUT_PROMPT_EVENT_CODE = 0x0c,
    FORCE_OUTPUT_PROMPT_DISMISS_EVENT_CODE = 0x1a,
    TORQUE_KEY_PROMPT_EVENT_CODE = 7,
    TORQUE_KEY_PROMPT_DISMISS_EVENT_CODE = 0x18,
    UNSUPPORTED_WHEEL_INVERTED_EVENT_CODE = 0x0f,
    UNSUPPORTED_WHEEL_OUTLINED_EVENT_CODE = 0x10,
    SHUTDOWN_STATUS_CODE = 0x2d,
    FORCE_FEEDBACK_FULL_STRENGTH_PERCENT = 100,
    FORCE_FEEDBACK_DD1_REDUCED_STRENGTH_PERCENT = 40,
    FORCE_FEEDBACK_DD2_REDUCED_STRENGTH_PERCENT = 32,
    FORCE_FEEDBACK_DD1_AUTOMATIC_STRENGTH_PERCENT = 35,
    FORCE_FEEDBACK_DD2_AUTOMATIC_STRENGTH_PERCENT = 30,
    FORCE_FEEDBACK_RAMP_INTERVAL_MS = 50,
    MOTOR_STARTUP_WHEEL_DISCOVERY_TIMEOUT_MS = 500,
    MOTOR_LINK_MALFORMED_FRAME_LIMIT = 100,
    WHEEL_STARTUP_VERSION_COMMAND = 0x0a,
    WHEEL_STARTUP_TEXT_METADATA = 0x10,
    WHEEL_DISPLAY_TORQUE_KEY_PROMPT = 0x1a,
    WHEEL_DISPLAY_TORQUE_KEY_CONFIRMED = 0x28,
    WHEEL_DISPLAY_ENABLE_TORQUE_PROMPT = 0x29,
    WHEEL_DISPLAY_ENABLE_TORQUE_CONFIRMED = 0x2a,
};

/** @brief Storage used only by the native and Xbox motor-command transports. */
typedef struct {
    uint8_t upload_assembly[USB_MOTOR_BUFFER_SIZE];
    uint8_t receive_assembly[USB_MOTOR_BUFFER_SIZE];
    uint8_t mailbox_receive[USB_MOTOR_BUFFER_SIZE];
    uint8_t transmit[USB_MOTOR_BUFFER_SIZE];
    uint8_t application_data[USB_MOTOR_BUFFER_SIZE];
} UsbMotorWorkspace;

/** @brief Storage used only by the PlayStation secure-element transport. */
typedef struct {
    A71chSessionService session;
    A71chAuthenticationService authentication;
    uint8_t request[USB_PLAYSTATION_AUTHENTICATION_REQUEST_SIZE];
} PlayStationAuthenticationWorkspace;

/** @brief Storage shared by mutually exclusive USB operating modes. */
typedef union {
    UsbMotorWorkspace motor;
    PlayStationAuthenticationWorkspace playstation;
} UsbOperatingModeWorkspace;

static UsbOperatingModeWorkspace usb_operating_mode_workspace;
static const MotorCommandChannelBuffers motor_command_channel_buffers = {
    .receive_assembly = usb_operating_mode_workspace.motor.receive_assembly,
    .receive_assembly_capacity = sizeof(usb_operating_mode_workspace.motor.receive_assembly),
    .transmit = usb_operating_mode_workspace.motor.transmit,
    .transmit_capacity = sizeof(usb_operating_mode_workspace.motor.transmit),
    .pending_payload = usb_operating_mode_workspace.motor.application_data,
    .pending_payload_capacity = sizeof(usb_operating_mode_workspace.motor.application_data),
};
static const UsbMotorVendorServiceBuffers usb_motor_buffers = {
    .upload_assembly = usb_operating_mode_workspace.motor.upload_assembly,
    .upload_assembly_capacity = sizeof(usb_operating_mode_workspace.motor.upload_assembly),
    .application_data = usb_operating_mode_workspace.motor.application_data,
    .application_data_capacity = sizeof(usb_operating_mode_workspace.motor.application_data),
};

/**
 * @brief Stores pending base settings immediately.
 *
 * Performs the synchronous storage pass used at explicit commit, shutdown, and restart boundaries.
 */
static void save_base_settings(void) {
    (void)base_settings_persistence_save(&settings_persistence, &base_settings);
}

/**
 * @brief Applies a power-controller transition to base hardware and retained settings.
 *
 * Enables the external power hold at startup. Shutdown start makes retained settings immediately
 * eligible for storage, performs one persistence pass, inhibits force output, publishes shutdown
 * state, and releases the power hold. Shutdown completion publishes its terminal event and
 * disconnects USB.
 *
 * @param[in] action Power transition produced by the controller.
 */
static void apply_power_action(PowerAction action) {
    switch (action) {
    case POWER_ACTION_ENABLE_LATCH:
        platform_power_latch_set(true);
        break;
    case POWER_ACTION_BEGIN_SHUTDOWN:
        save_base_settings();
        force_output_enabled = false;
        motor_output_report = (ForceOutputReport){0};
        pedal_service_request_configuration(&pedal_service, tuning_profile->alternate_brake_force,
                                            true);
        platform_cooling_set_duty(0, 0, true);
        (void)system_event_queue_try_push(&system_event_queue, SHUTDOWN_EVENT_CODE);
        system_control_state_set_status(&system_control_state, wheel_service_mode(&wheel_service),
                                        SHUTDOWN_STATUS_CODE);
        system_control_state_set_active_event(&system_control_state, SHUTDOWN_EVENT_CODE);
        platform_usb_detach();
        platform_power_latch_set(false);
        break;
    case POWER_ACTION_FINISH_SHUTDOWN:
        system_control_state_set_active_event(&system_control_state,
                                              SYSTEM_DISPLAY_DISMISS_EVENT_CODE);
        break;
    case POWER_ACTION_NONE:
    case POWER_ACTION_TORQUE_REQUEST_CHANGED:
        break;
    }
}

/**
 * @brief Services the wheel-base power button.
 *
 * Samples the active-high platform input, advances the short-press and shutdown controller only
 * while the retained security code is disabled, and applies any resulting hardware transition.
 *
 * @param[in] now_ms Current monotonic time in milliseconds.
 */
static void service_power(uint32_t now_ms) {
    PowerAction action = power_controller_update(&power_controller, platform_power_button_pressed(),
                                                 !base_settings.security_code.enabled, now_ms);
    apply_power_action(action);
}

/**
 * @brief Applies a pending power-button torque request.
 *
 * Waits for the single event slot, then publishes the event and applies its status, feature, and
 * attached-wheel response changes as one accepted transition.
 */
static void service_power_torque_request(void) {
    uint8_t wheel_mode = wheel_service_mode(&wheel_service);
    if (!system_torque_transition_update(
            &system_torque_transition, power_controller.torque_disabled,
            system_event_queue.pending_code == 0, wheel_mode, system_control_state.operating_status,
            &system_torque_action)) {
        return;
    }
    if (system_event_queue_try_push(&system_event_queue, system_torque_action.pending_event_code)) {
        system_control_state_apply_torque_transition(&system_control_state, wheel_mode,
                                                     usb_remote_tuning_service.setup_page,
                                                     &system_torque_action);
    }
}

/**
 * @brief Services queued system notice events.
 *
 * Expires timed notices, dispatches recognized events at the system cadence, and applies their
 * semantic presentation changes to the local display owners.
 *
 * @param[in] now_ms Current monotonic time in milliseconds.
 */
static void service_system_events(uint32_t now_ms) {
    system_notice_update(&system_notice, now_ms);
    SystemEventAction action =
        system_event_dispatcher_update(&system_event_dispatcher, &system_event_queue, now_ms);
    if (action == SYSTEM_EVENT_ACTION_SHOW_TUNING_MENU_RESET) {
        system_notice_show(&system_notice, SYSTEM_NOTICE_TUNING_MENU_RESET, now_ms);
    } else if (action == SYSTEM_EVENT_ACTION_SHOW_WHEEL_CENTER_CALIBRATED) {
        system_notice_show(&system_notice, SYSTEM_NOTICE_WHEEL_CENTER_CALIBRATED, now_ms);
    } else if (action == SYSTEM_EVENT_ACTION_SHOW_POSITION_SENSOR_TEST_SUCCEEDED) {
        system_notice_show(&system_notice, SYSTEM_NOTICE_POSITION_SENSOR_TEST_SUCCEEDED, now_ms);
    } else if (action == SYSTEM_EVENT_ACTION_SHOW_POSITION_SENSOR_TEST_STARTED) {
        system_notice_show(&system_notice, SYSTEM_NOTICE_POSITION_SENSOR_TEST_STARTED, now_ms);
    } else if (action == SYSTEM_EVENT_ACTION_SHOW_POSITION_SENSOR_TEST_FAILED) {
        system_notice_show(&system_notice, SYSTEM_NOTICE_POSITION_SENSOR_TEST_FAILED, now_ms);
    } else if (action == SYSTEM_EVENT_ACTION_SHOW_TORQUE_REDUCED) {
        system_notice_show(&system_notice, SYSTEM_NOTICE_TORQUE_REDUCED, now_ms);
    } else if (action == SYSTEM_EVENT_ACTION_SHOW_TORQUE_REDUCED_STEERING_WHEEL) {
        system_notice_show(&system_notice, SYSTEM_NOTICE_TORQUE_REDUCED_STEERING_WHEEL, now_ms);
    } else if (action == SYSTEM_EVENT_ACTION_SHOW_MOTOR_CALIBRATION_DISCONNECT_WHEEL) {
        system_notice_show(&system_notice, SYSTEM_NOTICE_MOTOR_CALIBRATION_DISCONNECT_WHEEL,
                           now_ms);
    } else if (action == SYSTEM_EVENT_ACTION_SHOW_MOTOR_CALIBRATION_UNSUPPORTED) {
        system_notice_show(&system_notice, SYSTEM_NOTICE_MOTOR_CALIBRATION_UNSUPPORTED, now_ms);
    } else if (action == SYSTEM_EVENT_ACTION_SHOW_MOTOR_CALIBRATION_ONGOING) {
        system_notice_show(&system_notice, SYSTEM_NOTICE_MOTOR_CALIBRATION_ONGOING, now_ms);
    } else if (action == SYSTEM_EVENT_ACTION_SHOW_MOTOR_CALIBRATION_COMPLETED) {
        system_notice_show(&system_notice, SYSTEM_NOTICE_MOTOR_CALIBRATION_COMPLETED, now_ms);
    } else if (action == SYSTEM_EVENT_ACTION_SHOW_MOTOR_CALIBRATION_ERASED) {
        system_notice_show(&system_notice, SYSTEM_NOTICE_MOTOR_CALIBRATION_ERASED, now_ms);
    } else if (action == SYSTEM_EVENT_ACTION_SHOW_STANDARD_TUNING_MODE) {
        system_notice_show(&system_notice, SYSTEM_NOTICE_STANDARD_TUNING_MODE, now_ms);
    } else if (action == SYSTEM_EVENT_ACTION_SHOW_ADVANCED_TUNING_MODE) {
        system_notice_show(&system_notice, SYSTEM_NOTICE_ADVANCED_TUNING_MODE, now_ms);
    } else if (action == SYSTEM_EVENT_ACTION_SHOW_MAXIMUM_ROTATIONS_EXCEEDED) {
        system_notice_show(&system_notice, SYSTEM_NOTICE_MAXIMUM_ROTATIONS_EXCEEDED, now_ms);
    } else if (action == SYSTEM_EVENT_ACTION_SHOW_SHUTDOWN) {
        system_notice_show(&system_notice, SYSTEM_NOTICE_SHUTDOWN, now_ms);
    } else if (action == SYSTEM_EVENT_ACTION_SHOW_UNSUPPORTED_WHEEL_INVERTED) {
        system_notice_show(&system_notice, SYSTEM_NOTICE_UNSUPPORTED_WHEEL_INVERTED, now_ms);
    } else if (action == SYSTEM_EVENT_ACTION_SHOW_UNSUPPORTED_WHEEL_OUTLINED) {
        system_notice_show(&system_notice, SYSTEM_NOTICE_UNSUPPORTED_WHEEL_OUTLINED, now_ms);
    } else if (action == SYSTEM_EVENT_ACTION_SHOW_ALTERNATIVE_SHIFTER_ENABLED) {
        system_notice_show(&system_notice, SYSTEM_NOTICE_ALTERNATIVE_SHIFTER_ENABLED, now_ms);
    } else if (action == SYSTEM_EVENT_ACTION_SHOW_ALTERNATIVE_SHIFTER_DISABLED) {
        system_notice_show(&system_notice, SYSTEM_NOTICE_ALTERNATIVE_SHIFTER_DISABLED, now_ms);
    } else if (action == SYSTEM_EVENT_ACTION_SHOW_FORCE_OUTPUT_PROMPT) {
        force_output_prompt_visible = true;
    } else if (action == SYSTEM_EVENT_ACTION_DISMISS_FORCE_OUTPUT_PROMPT) {
        force_output_prompt_visible = false;
    } else if (action == SYSTEM_EVENT_ACTION_SHOW_TORQUE_KEY_PROMPT) {
        torque_key_prompt_visible = true;
    } else if (action == SYSTEM_EVENT_ACTION_DISMISS_TORQUE_KEY_PROMPT) {
        torque_key_prompt_visible = false;
    } else if (action == SYSTEM_EVENT_ACTION_SHOW_TORQUE_DISABLED) {
        torque_disabled_notice_visible = true;
    } else if (action == SYSTEM_EVENT_ACTION_DISMISS_TORQUE_DISABLED) {
        torque_disabled_notice_visible = false;
    }
}

/**
 * @brief Applies a Torque Key prompt visibility action.
 *
 * Show and removal actions wait for the shared event slot and publish the corresponding display
 * state. An accepted acknowledgement hides the prompt immediately and publishes the common
 * dismissal state.
 *
 * @param[in] action Requested Torque Key prompt transition.
 */
static void apply_torque_key_prompt_action(TorqueKeyPromptAction action) {
    switch (action) {
    case TORQUE_KEY_PROMPT_ACTION_SHOW:
        if (system_event_queue_try_push(&system_event_queue, TORQUE_KEY_PROMPT_EVENT_CODE)) {
            system_control_state_set_active_event(&system_control_state,
                                                  TORQUE_KEY_PROMPT_EVENT_CODE);
            (void)wheel_service_queue_tuning_display_notification(&wheel_service,
                                                                  WHEEL_DISPLAY_TORQUE_KEY_PROMPT);
        }
        break;
    case TORQUE_KEY_PROMPT_ACTION_CANCEL:
        if (system_event_queue_try_push(&system_event_queue,
                                        TORQUE_KEY_PROMPT_DISMISS_EVENT_CODE)) {
            system_control_state_set_active_event(&system_control_state,
                                                  SYSTEM_DISPLAY_DISMISS_EVENT_CODE);
            (void)wheel_service_queue_tuning_display_notification(
                &wheel_service, WHEEL_DISPLAY_TORQUE_KEY_CONFIRMED);
        }
        break;
    case TORQUE_KEY_PROMPT_ACTION_DISMISS:
        torque_key_prompt_visible = false;
        system_control_state_set_active_event(&system_control_state,
                                              SYSTEM_DISPLAY_DISMISS_EVENT_CODE);
        (void)wheel_service_queue_tuning_display_notification(&wheel_service,
                                                              WHEEL_DISPLAY_TORQUE_KEY_CONFIRMED);
        break;
    case TORQUE_KEY_PROMPT_ACTION_NONE:
        break;
    }
}

/**
 * @brief Services the Torque Key safety acknowledgement.
 *
 * Filters the active-low board input for 500 milliseconds, starts or cancels the safety prompt on
 * stable key transitions, accepts a released wheel input only while the prompt owns the display,
 * and advances presentation through the shared event slot. Scan-mode polling or attached-wheel
 * calibration capability revokes acknowledgement until the condition clears. Acknowledgement
 * selects full base strength; removal or revocation restores the DD1 or DD2 reduced limit and
 * refreshes motor-side tuning.
 *
 * @param[in] now_ms Current monotonic time in milliseconds.
 */
static void service_torque_key(uint32_t now_ms) {
    TorqueKeyEvent event = torque_key_update(&torque_key, platform_torque_key_inserted(), now_ms);
    bool acknowledgement_available =
        wheel_service_torque_key_acknowledgement_available(&wheel_service);
    if (acknowledgement_available != torque_key_acknowledgement_available) {
        torque_key_acknowledgement_available = acknowledgement_available;
        torque_key_prompt_set_inserted(&torque_key_prompt, acknowledgement_available &&
                                                               torque_key.state_known &&
                                                               torque_key.inserted);
    } else if (event == TORQUE_KEY_EVENT_INSERTED) {
        torque_key_prompt_set_inserted(&torque_key_prompt, acknowledgement_available);
    } else if (event == TORQUE_KEY_EVENT_REMOVED) {
        torque_key_prompt_set_inserted(&torque_key_prompt, false);
    }

    bool visible = torque_key_prompt_visible && system_notice.kind == SYSTEM_NOTICE_NONE &&
                   !torque_disabled_notice_visible;
    if (display_prompt_update(&torque_key_display_prompt, visible,
                              wheel_service_acknowledgement_input_active(&wheel_service))) {
        torque_key_prompt_set_response(&torque_key_prompt, true);
    }
    apply_torque_key_prompt_action(
        torque_key_prompt_service(&torque_key_prompt, system_event_queue.pending_code == 0));

    uint8_t strength = torque_key_prompt.phase == TORQUE_KEY_PROMPT_ACKNOWLEDGED
                           ? FORCE_FEEDBACK_FULL_STRENGTH_PERCENT
                       : board_identity.variant == BOARD_VARIANT_DD1
                           ? FORCE_FEEDBACK_DD1_REDUCED_STRENGTH_PERCENT
                           : FORCE_FEEDBACK_DD2_REDUCED_STRENGTH_PERCENT;
    if (motor_tuning_context.strength_percent != strength) {
        bool torque_reduced =
            motor_tuning_context.strength_percent == FORCE_FEEDBACK_FULL_STRENGTH_PERCENT &&
            strength != FORCE_FEEDBACK_FULL_STRENGTH_PERCENT;
        motor_tuning_context.strength_percent = strength;
        if (motor_tuning_ready) {
            motor_tuning_service_refresh(&motor_tuning_service, tuning_profile,
                                         &motor_tuning_context);
        }
        if (torque_reduced) {
            WheelProtocolPhase phase = wheel_service_protocol_phase(&wheel_service);
            uint8_t event_code = phase == WHEEL_PROTOCOL_ACTIVE
                                     ? TORQUE_REDUCED_EVENT_CODE
                                     : TORQUE_REDUCED_STEERING_WHEEL_EVENT_CODE;
            (void)system_event_queue_try_push(&system_event_queue, event_code);
        }
    }
}

/**
 * @brief Initializes thermal control and fan measurement.
 *
 * Restores the controller defaults, clears thermal sampling and effect-limit state, configures the
 * board-specific fan hardware, and applies the 25-percent startup duty to both outputs.
 */
static void initialize_cooling(void) {
    cooling_controller_init(&cooling_controller, board_identity.mode_bits == 7);
    cooling_effect_limit_init(&cooling_effect_limit);
    cooling_temperature_monitor_init(&cooling_temperature_monitor);
    fan_speed_rpm[PLATFORM_FAN_PRIMARY] = 0;
    fan_speed_rpm[PLATFORM_FAN_SECONDARY] = 0;
    platform_cooling_init(board_identity.mode_bits != 7);
    platform_cooling_set_duty(FAN_STARTUP_DUTY_PERCENT, FAN_STARTUP_DUTY_PERCENT, false);
}

/**
 * @brief Services fan output, force derating, and thermal effect limits.
 *
 * Uses the retained 20-degree startup temperature until motor telemetry is available, advances the
 * cooling controllers, publishes effect-strength changes, and applies both fan duties.
 *
 * @param[in] now_ms Current monotonic time in milliseconds.
 */
static void service_cooling(uint32_t now_ms) {
    bool managed_motor_present = motor_probe_identity(&motor_probe) != 0;
    const MotorTelemetry *telemetry =
        motor_tuning_ready ? motor_telemetry_service_value(&motor_telemetry_service) : 0;
    float motor_temperature = telemetry != 0 && telemetry->motor_temperature_valid
                                  ? (float)(int16_t)telemetry->motor_temperature
                                  : (float)COOLING_STARTUP_TEMPERATURE_C;
    bool output_inhibited =
        motor_tuning_ready && motor_status_service_output_inhibited(&motor_status_service);
    cooling_controller_update(&cooling_controller, motor_temperature, managed_motor_present,
                              output_inhibited, now_ms);
    if (cooling_controller.automatic_control_suspended) {
        return;
    }
    CoolingEffectStrengths previous_strengths = cooling_effect_strengths;
    cooling_effect_limit_update(&cooling_effect_limit, &cooling_effect_strengths,
                                &cooling_controller, motor_temperature, managed_motor_present,
                                now_ms);
    if (cooling_effect_strengths.force != previous_strengths.force ||
        cooling_effect_strengths.spring != previous_strengths.spring ||
        cooling_effect_strengths.damper != previous_strengths.damper) {
        runtime_tuning_profile.force_effect_strength = cooling_effect_strengths.force;
        runtime_tuning_profile.spring_effect_strength = cooling_effect_strengths.spring;
        runtime_tuning_profile.damper_effect_strength = cooling_effect_strengths.damper;
        if (motor_tuning_ready) {
            motor_tuning_service_refresh(&motor_tuning_service, tuning_profile,
                                         &motor_tuning_context);
        }
    }
    platform_cooling_set_duty(cooling_controller.primary_duty_percent,
                              cooling_controller.secondary_duty_percent, false);
}

/**
 * @brief Consumes and converts one fan tachometer result.
 *
 * Publishes zero for a missing signal or converts two consecutive captures to RPM when a completed
 * result is available.
 *
 * @param[in] fan Fan channel to update.
 */
static void update_fan_speed(PlatformFan fan) {
    if (!platform_cooling_take_tachometer(fan, &fan_tachometer)) {
        return;
    }

    fan_speed_rpm[fan] =
        fan_tachometer.present
            ? fan_tachometer_rpm(fan_tachometer.previous_capture, fan_tachometer.current_capture)
            : 0;
}

/**
 * @brief Initializes the motor controller's live SPI exchange.
 *
 * Clears live output, status, position, transport, and transmit state before starting the platform
 * motor transport. The first completed exchange selects the initial protocol response.
 */
static void initialize_motor_link(void) {
    motor_output_report = (ForceOutputReport){0};
    motor_output_status_init(&motor_output_status);
    motor_output_transport_init(&motor_output_transport);
    memset(motor_transmitted_frame, 0, sizeof(motor_transmitted_frame));
    motor_malformed_frame_count = 0;
    platform_motor_link_init(motor_transmitted_frame);
    motor_position_ready = false;
}

/**
 * @brief Captures and persists the current absolute wheel center.
 *
 * Ignores capture before a valid motor-position report is available. Otherwise normalizes the
 * current sample with the identified controller modulus and stores a changed reference before
 * returning.
 *
 * @return True when a valid position sample was captured.
 */
static bool capture_current_wheel_center(void) {
    if (!motor_position_ready) {
        return false;
    }
    if (wheel_position_reference_capture(
            &base_settings.wheel_position, motor_position_report.wheel_position,
            motor_identity_position_modulus(motor_probe_identity(&motor_probe)))) {
        base_settings_persistence_mark_dirty(&settings_persistence);
        save_base_settings();
    }
    return true;
}

/**
 * @brief Builds the motor controller's force-feedback status byte.
 *
 * Selects direct force during motor and wheel startup and Xbox operation, or remote motor-side
 * effect processing in other modes. Enables force during startup centering or when motor safety,
 * USB connection, operator confirmation, and the power-button torque gate permit runtime output.
 * Host output gates and full-torque acknowledgement remain live in every mode, while Xbox direct
 * force retains the preceding motor-owned gate state.
 *
 * @return Current force-feedback status bits for the next motor-link packet.
 */
static uint8_t motor_force_feedback_status(void) {
    bool xbox_direct_force = usb_device_operating_mode() == USB_OPERATING_MODE_XBOX_GIP;
    bool startup_force_enabled = motor_startup_centering_active(&motor_startup_centering);
    bool runtime_force_enabled = motor_tuning_ready &&
                                 !motor_status_service_output_inhibited(&motor_status_service) &&
                                 !usb_connection_monitor.disconnected && force_output_enabled &&
                                 !system_torque_transition.applied_disabled;
    motor_output_status_input = (MotorOutputStatusInput){
        .direct_force = motor_startup_direct_force,
        .xbox_mode = xbox_direct_force,
        .force_enabled = startup_force_enabled || runtime_force_enabled,
        .override_active = system_torque_transition.applied_disabled,
        .transition_active = !motor_startup_direct_force &&
                             !force_feedback_state.primary_output_disabled &&
                             wheel_service_force_output_transition_active(&wheel_service),
        .primary_disabled = force_feedback_state.primary_output_disabled,
        .secondary_disabled = force_feedback_state.secondary_output_disabled,
        .usb_disconnected = usb_connection_monitor.disconnected,
        .full_torque = torque_key_prompt.phase == TORQUE_KEY_PROMPT_ACKNOWLEDGED,
    };
    return motor_output_status_update(&motor_output_status, &motor_output_status_input);
}

/**
 * @brief Processes one completed motor exchange and prepares its successor.
 *
 * Decodes valid live packets and publishes position reports. Normal exchanges retain the current
 * force-output response in a two-frame history; replay exchanges restore the older response
 * without consuming output state. CRC-valid packets clear delimiter-failure tracking, CRC failures
 * preserve it, and the transport restarts after the 101st packet with invalid delimiters.
 */
static void service_motor_link(void) {
    if (!platform_motor_link_take_received(motor_received_frame)) {
        return;
    }

    MotorLiveFrameResult frame_result =
        motor_live_frame_decode(motor_received_frame, &motor_live_frame);
    bool replay_requested = false;
    if (frame_result == MOTOR_LIVE_FRAME_VALID) {
        motor_malformed_frame_count = 0;
        platform_motor_link_confirm_synchronized();
        if (motor_position_report_decode(&motor_live_frame, &motor_position_report)) {
            motor_position_ready = true;
            replay_requested = motor_position_report.replay;
            if (!base_settings.wheel_position.calibrated) {
                (void)capture_current_wheel_center();
            }
        }
    } else if (frame_result == MOTOR_LIVE_FRAME_INVALID_BOUNDARY) {
        motor_malformed_frame_count++;
    }

    if (replay_requested &&
        motor_output_transport_replay_frame(&motor_output_transport, &motor_live_frame)) {
        motor_live_frame_encode(&motor_live_frame, motor_transmitted_frame);
        platform_motor_link_set_transmit(motor_transmitted_frame);
        return;
    }

    motor_output_transport_build_frame(&motor_output_transport, motor_force_feedback_status(),
                                       (int16_t)base_settings.wheel_position.center,
                                       &motor_output_report, &motor_live_frame);
    motor_output_transport_remember_frame(&motor_output_transport, &motor_live_frame);
    motor_live_frame_encode(&motor_live_frame, motor_transmitted_frame);
    platform_motor_link_set_transmit(motor_transmitted_frame);

    if (motor_malformed_frame_count > MOTOR_LINK_MALFORMED_FRAME_LIMIT) {
        motor_malformed_frame_count = 0;
        platform_motor_link_init(motor_transmitted_frame);
    }
}

/**
 * @brief Refreshes consumers of the current runtime tuning profile.
 *
 * Applies the current runtime values to cooling, pedal, wheel, and motor services without
 * changing retained setup storage.
 */
static void refresh_runtime_tuning_profile(void) {
    cooling_effect_strengths = (CoolingEffectStrengths){
        .force = tuning_profile->force_effect_strength,
        .spring = tuning_profile->spring_effect_strength,
        .damper = tuning_profile->damper_effect_strength,
    };
    pedal_service_set_brake_force(&pedal_service, tuning_profile->brake_force);
    if (pedal_service_calibration_active(&pedal_service)) {
        pedal_service_request_configuration(&pedal_service, tuning_profile->alternate_brake_force,
                                            false);
    }
    PedalV4Tuning pedal_tuning = {
        .brake_force = tuning_profile->brake_force,
        .clutch_curve = (uint8_t)tuning_profile->clutch_pedal_curve,
        .brake_curve = (uint8_t)tuning_profile->brake_pedal_curve,
        .throttle_curve = (uint8_t)tuning_profile->throttle_pedal_curve,
    };
    pedal_service_set_v4_tuning(&pedal_service, pedal_tuning);
    wheel_service_set_button_illumination(&wheel_service,
                                          tuning_profile->button_illumination_enabled != 0);
    motor_tuning_context.automatic_rotation_degrees = tuning_profile->rotation_degrees;
    if (motor_tuning_ready) {
        motor_tuning_service_refresh(&motor_tuning_service, tuning_profile, &motor_tuning_context);
    }
}

/**
 * @brief Applies the active retained tuning profile to runtime consumers.
 *
 * Copies the active slot into runtime storage and refreshes cooling, pedal, wheel, and motor
 * settings that take effect immediately.
 */
static void apply_active_tuning_profile(void) {
    runtime_tuning_profile = *tuning_profile_bank_active(&base_settings.tuning_profiles);
    tuning_profile = &runtime_tuning_profile;
    refresh_runtime_tuning_profile();
}

/**
 * @brief Applies a brake-force value reported by the attached V3 pedal controller.
 *
 * Updates the active runtime and retained tuning profiles when the controller reports a changed
 * alternate brake-force percentage, then schedules the shared settings record for persistence.
 *
 * @param[in] now_ms Current monotonic time in milliseconds.
 */
static void service_alternate_brake_force(uint32_t now_ms) {
    (void)now_ms;
    uint8_t brake_force = pedal_service_take_alternate_brake_force(&pedal_service);
    if (brake_force == PEDAL_ALTERNATE_BRAKE_FORCE_NO_UPDATE ||
        brake_force == tuning_profile->alternate_brake_force) {
        return;
    }

    runtime_tuning_profile.alternate_brake_force = brake_force;
    base_settings.tuning_profiles.slots[base_settings.tuning_profiles.active_slot]
        .alternate_brake_force = brake_force;
    base_settings_persistence_mark_dirty(&settings_persistence);
}

/**
 * @brief Publishes V4 pedal-adjustment progress on the attached wheel.
 *
 * Consumes each transfer-produced display command once and starts or replaces the temporary wheel
 * presentation at the current monotonic time.
 *
 * @param[in] now_ms Current monotonic time in milliseconds.
 */
static void service_pedal_adjustment_display(uint32_t now_ms) {
    PedalAdjustmentDisplay display = pedal_service_take_adjustment_display(&pedal_service);
    if (display != PEDAL_ADJUSTMENT_DISPLAY_IDLE) {
        wheel_service_begin_display_overlay(&wheel_service, (uint8_t)display, now_ms);
    }
}

/**
 * @brief Loads retained base settings and initializes their runtime consumers.
 *
 * Selects the newest valid settings record, restores the retained USB base mode, and initializes
 * the local auxiliary input and attached-wheel auxiliary output from their retained settings.
 */
static void initialize_base_settings(void) {
    base_settings_persistence_load(&settings_persistence, &base_settings);
    selected_base_mode =
        base_settings.operating_mode_valid ? base_settings.operating_mode : UINT8_MAX;
    save_base_settings();
    auxiliary_axis_init(&auxiliary_axis, &base_settings.auxiliary_axis);
    wheel_service_set_auxiliary_output_option(&wheel_service, base_settings.wheel_auxiliary_option);
}

/**
 * @brief Initializes motor force, tuning, calibration, and discovery state.
 *
 * Starts force output with a zero-percent ramp and the board variant's reduced Torque Key limit,
 * clears calibration and maximum-rotation monitoring, applies the active profile, and begins
 * asynchronous motor discovery.
 */
static void initialize_motor(void) {
    force_feedback_state_init(&force_feedback_state);
    motor_tuning_context = (MotorTuningContext){
        .ramp_percent = 0,
        .strength_percent = board_identity.variant == BOARD_VARIANT_DD1
                                ? FORCE_FEEDBACK_DD1_REDUCED_STRENGTH_PERCENT
                                : FORCE_FEEDBACK_DD2_REDUCED_STRENGTH_PERCENT,
        .xbox_mode = 0,
        .calibration_active = 0,
    };
    motor_tuning_ready = false;
    motor_startup_direct_force = true;
    motor_startup_wheel_discovery_started = false;
    motor_calibration_service_init(&motor_calibration_service);
    motor_rotation_guard_init(&motor_rotation_guard);
    apply_active_tuning_profile();
    motor_probe_init(&motor_probe);
    motor_probe_start(&motor_probe, platform_time_ms());
}

/**
 * @brief Initializes the host command bridge.
 *
 * Attaches report-6 mailbox storage and wheel-transfer requests to the shared type-four command
 * transport, restarts adapter discovery, initializes diagnostic and tuning vendor responses, and
 * resets console startup state from the retained base-mode selection.
 */
static void initialize_usb_command_bridge(void) {
    command_transport_init(&command_transport);
    wheel_protocol_bridge_service_init(&wheel_protocol_bridge_service, &command_transport);
    wheel_usb_bridge_gate_init(&wheel_usb_bridge_gate);
    usb_updater_service_init(&usb_updater_service, &command_transport);
    wheel_service_reset_adapter_commands(&wheel_service);
    (void)motor_command_mailbox_exchange_init(
        &motor_command_mailbox, usb_operating_mode_workspace.motor.mailbox_receive,
        sizeof(usb_operating_mode_workspace.motor.mailbox_receive));
    (void)motor_command_channel_init(&motor_command_channel, &motor_command_channel_buffers);
    motor_command_startup_service_init(&motor_command_startup_service);
    (void)usb_motor_vendor_service_init(&usb_motor_vendor_service, &motor_command_channel,
                                        &usb_motor_buffers);
    wheel_transfer_service_init(&wheel_transfer_service);
    wheel_accessory_service_init(&wheel_accessory_service);
    usb_diagnostic_report_service_init(&usb_diagnostic_report_service);
    usb_tuning_status_report_service_init(&usb_tuning_status_report_service);
    usb_remote_tuning_service_init(&usb_remote_tuning_service);
    wheel_command_forwarder_init(&wheel_command_forwarder);
    usb_tuning_menu_service_init(&usb_tuning_menu_service);
    usb_transfer_request_init(&usb_transfer_request);
    usb_transfer_response_init(&usb_transfer_response);
    usb_transfer_response_has_pedal_owner = false;
    usb_tuning_profile_service_init(&usb_tuning_profile_service);
    tuning_interaction_init(&tuning_interaction);
    tuning_menu_init(&tuning_menu);
    security_code_init(&security_code);
    usb_motor_acknowledgement_ready = false;
    xbox_mode_startup_attempted = false;
    xbox_mode_startup_finished = false;
    console_wheel_mode = UINT8_MAX;
    observed_wheel_mode = UINT8_MAX;
    selected_base_mode =
        base_settings.operating_mode_valid ? base_settings.operating_mode : UINT8_MAX;
    usb_motor_acknowledgement_length = 0;
    usb_vendor_response_kind = USB_VENDOR_RESPONSE_NONE;
    usb_vendor_response_length = 0;
    usb_xbox_control_response_pending = USB_XBOX_CONTROL_RESPONSE_NONE;
    xbox_profile_steering_range_degrees = 0;
    xbox_profile_force_feedback_percent = 0;
    for (uint8_t request = 0; request < WHEEL_TRANSFER_REQUEST_COUNT; request++) {
        usb_wheel_transfer_response_pending[request] = false;
    }
}

/**
 * @brief Synchronizes transient Xbox controls with the active tuning profile.
 *
 * Initializes the Xbox steering range and force level from the current profile. A changed profile
 * replaces prior host overrides, while unchanged profile values leave active Xbox requests intact.
 */
static void synchronize_xbox_controls_with_profile(void) {
    if (xbox_profile_steering_range_degrees != tuning_profile->rotation_degrees) {
        xbox_profile_steering_range_degrees = tuning_profile->rotation_degrees;
        xbox_steering_range_degrees = tuning_profile->rotation_degrees;
    }
    if (xbox_profile_force_feedback_percent != tuning_profile->force_feedback_strength) {
        xbox_profile_force_feedback_percent = tuning_profile->force_feedback_strength;
        xbox_force_feedback_percent = tuning_profile->force_feedback_strength;
    }
}

/**
 * @brief Builds the current host diagnostic snapshot.
 *
 * Collects board, motor, attached-wheel, cooling, fan, auxiliary-position, and centered wheel
 * motion state in the order used by vendor report route four.
 *
 * @param[in] now_ms Current monotonic time in milliseconds.
 */
static void update_usb_diagnostic_snapshot(uint32_t now_ms) {
    usb_diagnostic_snapshot = (UsbDiagnosticSnapshot){
        .base_mode = board_identity.mode_bits,
        .base_temperatures_c =
            {
                cooling_temperature_monitor.temperatures_c[0],
                cooling_temperature_monitor.temperatures_c[1],
            },
        .system_seconds = now_ms / 1000,
        .transport_error_count = serial_service_error_count(&serial_service),
        .cooling =
            {
                .phase = (uint8_t)cooling_controller.phase,
                .output_duty_percent = cooling_controller.primary_duty_percent,
                .primary_delay_seconds = (int8_t)(cooling_controller.primary_delay_ms / 1000),
                .secondary_delay_seconds = (int8_t)(cooling_controller.secondary_delay_ms / 1000),
                .low_threshold_offset = cooling_controller.low_threshold_offset,
                .high_threshold_offset = cooling_controller.high_threshold_offset,
            },
        .pwm =
            {
                .secondary_duty_percent = cooling_controller.secondary_duty_percent,
                .primary_duty_percent = cooling_controller.primary_duty_percent,
            },
        .pulse =
            {
                fan_speed_rpm[PLATFORM_FAN_PRIMARY],
                fan_speed_rpm[PLATFORM_FAN_SECONDARY],
            },
    };

    const MotorIdentity *identity = motor_probe_identity(&motor_probe);
    if (identity != 0) {
        usb_diagnostic_snapshot.motor.version = identity->transfer_code;
        usb_diagnostic_snapshot.motor.initial_status = (int8_t)identity->initial_status;
    }
    if (motor_tuning_ready) {
        const MotorTelemetry *telemetry = motor_telemetry_service_value(&motor_telemetry_service);
        if (telemetry->motor_temperature_valid) {
            usb_diagnostic_snapshot.motor.motor_temperature = telemetry->motor_temperature;
        }
        if (telemetry->driver_temperature_valid) {
            usb_diagnostic_snapshot.motor.driver_temperature = telemetry->driver_temperature;
        }
        if (telemetry->runtime_valid) {
            usb_diagnostic_snapshot.motor.runtime_seconds = telemetry->runtime_seconds;
        }
    }

    usb_diagnostic_snapshot.wheel_status = *wheel_status_service_snapshot(&wheel_status_service);
    if (motor_position_ready) {
        int32_t centered_position = wheel_position_center(motor_position_report.wheel_position,
                                                          base_settings.wheel_position.center);
        usb_diagnostic_snapshot.motor.motor_torque = motor_position_report.motor_torque;
        usb_diagnostic_snapshot.auxiliary_position.direction =
            motor_position_report.auxiliary_negative ? 1 : 0;
        usb_diagnostic_snapshot.auxiliary_position.position =
            motor_position_report.auxiliary_position;
        usb_diagnostic_snapshot.wheel_position = centered_position;
        usb_diagnostic_snapshot.wheel_velocity =
            wheel_velocity_update(&wheel_velocity_estimator, centered_position, now_ms);
    }
}

/**
 * @brief Refreshes the native USB tuning-status snapshot.
 *
 * Combines the current base, wheel, adapter, pedal, input, tuning, and system-control state into
 * the status layout returned by the vendor tuning-status report.
 */
static void update_usb_tuning_status_snapshot(void) {
    enum {
        BASE_STATUS_DD1 = 6,
        BASE_STATUS_DD1_OPTION = 7,
        BASE_STATUS_DD2 = 8,
        SYSTEM_STATUS_BASELINE = 0x01,
        SYSTEM_STATUS_VENDOR_ONE_AVAILABLE = 0x02,
        SYSTEM_STATUS_VENDOR_FOUR_AVAILABLE = 0x10,
        SYSTEM_STATUS_DD1_OPTION = 0x30,
        SYSTEM_STATUS_TUNING_AVAILABLE = 0x40,
        PEDAL_STATUS_INACTIVE = 0,
        PEDAL_STATUS_LEGACY = 1,
        PEDAL_STATUS_LEGACY_CALIBRATION = 2,
        PEDAL_STATUS_ANALOG = 3,
        PEDAL_STATUS_CALIBRATION = 4,
        PEDAL_STATUS_SECONDARY_CALIBRATION = 5,
        PEDAL_STATUS_TRANSFER = 6,
    };

    const WheelStatusSnapshot *wheel_status = wheel_status_service_snapshot(&wheel_status_service);
    const WheelAccessory *accessory = wheel_accessory_service_identity(&wheel_accessory_service);
    const WheelAdapterInput *adapter = wheel_service_adapter(&wheel_service);
    const PedalV3State *pedal_v3 = pedal_service_v3_state(&pedal_service);
    bool tuning_available = wheel_service_tuning_menu_available(&wheel_service);
    uint8_t system_flags = SYSTEM_STATUS_BASELINE | SYSTEM_STATUS_VENDOR_ONE_AVAILABLE;
    if (board_identity.variant == BOARD_VARIANT_DD1 && board_identity.hardware_option != 0) {
        system_flags |= SYSTEM_STATUS_DD1_OPTION;
    }
    if (tuning_available) {
        system_flags |= SYSTEM_STATUS_TUNING_AVAILABLE;
    }
    bool system_active = (system_flags & SYSTEM_STATUS_VENDOR_FOUR_AVAILABLE) != 0;

    uint16_t axis_values[2] = {0};
    bool axis_values_available = wheel_service_axis_report_enabled(&wheel_service) &&
                                 wheel_service_axis_values(&wheel_service, axis_values);
    WheelProtocolPhase wheel_phase = wheel_service_protocol_phase(&wheel_service);
    uint8_t button_mode = wheel_phase == WHEEL_PROTOCOL_SCANNING_PRIMARY ||
                                  wheel_phase == WHEEL_PROTOCOL_SCANNING_SECONDARY
                              ? wheel_service_mode_buttons(&wheel_service)
                              : (uint8_t)wheel_service_capability_flags(&wheel_service);
    uint32_t input = axis_values_available
                         ? (uint32_t)axis_values[0] | (uint32_t)axis_values[1] << 16
                         : wheel_service_axis_limit(&wheel_service) & 0x3fu;

    uint8_t pedal_status = PEDAL_STATUS_INACTIVE;
    if (pedal_service.phase == PEDAL_SERVICE_ANALOG) {
        pedal_status = PEDAL_STATUS_ANALOG;
    } else if (pedal_service_legacy_transport_active(&pedal_service)) {
        pedal_status = pedal_service_calibration_active(&pedal_service)
                           ? PEDAL_STATUS_LEGACY_CALIBRATION
                           : PEDAL_STATUS_LEGACY;
    } else if (pedal_v3->primary_calibration) {
        pedal_status = PEDAL_STATUS_CALIBRATION;
    } else if (pedal_v3->secondary_calibration) {
        pedal_status = PEDAL_STATUS_SECONDARY_CALIBRATION;
    } else if (pedal_v3->connection_flags != 0) {
        pedal_status = PEDAL_STATUS_TRANSFER;
    }

    uint8_t output_status = 0;
    if (!power_controller.torque_disabled &&
        (wheel_phase > WHEEL_PROTOCOL_SELECTING || force_feedback_state.primary_output_disabled)) {
        if (tuning_profile->force_feedback_strength > 99) {
            output_status = 0x0f;
        } else if (tuning_profile->force_feedback_strength == 40 ||
                   tuning_profile->force_feedback_strength == 32) {
            output_status = 1;
        }
    }

    uint8_t pedal_auxiliary = 0;
    uint8_t pedal_axis_low = 0;
    uint8_t pedal_axis_high = 0;
    if (pedal_status == PEDAL_STATUS_LEGACY || pedal_status == PEDAL_STATUS_LEGACY_CALIBRATION) {
        pedal_auxiliary = pedal_v3->shared_axes[2];
        pedal_axis_low = pedal_v3->shared_axes[0] & 0x3fu;
        pedal_axis_high = pedal_v3->shared_axes[1];
    }

    usb_tuning_status_snapshot = (UsbTuningStatusSnapshot){
        .base_status = board_identity.variant == BOARD_VARIANT_DD2 ? BASE_STATUS_DD2
                       : board_identity.hardware_option != 0       ? BASE_STATUS_DD1_OPTION
                                                                   : BASE_STATUS_DD1,
        .hardware_mode = board_identity.mode_bits,
        .auxiliary_mode = wheel_accessory_mode_code(accessory),
        .auxiliary_flags = wheel_accessory_mode_flags(accessory),
        .auxiliary_status = accessory->version,
        .wheel_status_low = wheel_status->status_low,
        .wheel_status_high = wheel_status->status_high,
        .wheel_mode = wheel_service_mode(&wheel_service),
        .button_mode = button_mode,
        .input = input,
        .adapter_mode = adapter->connected ? (uint8_t)(adapter->mode + 1u) : 0,
        .pedal_status = pedal_status,
        .pedal_auxiliary = pedal_auxiliary,
        .pedal_axis_low = pedal_axis_low,
        .pedal_axis_high = pedal_axis_high,
        .tuning_available = tuning_available,
        .system_active = system_active,
        .force_effect = accessory->accessory_type,
        .system_flags = system_flags,
        .output_status = output_status,
        .interface_gate =
            wheel_output_reports_interface_mode_gate(&wheel_service.protocol.output_reports),
    };
    if (adapter->connected) {
        if (adapter->mode == 0) {
            usb_tuning_status_snapshot.adapter[1] = adapter->profile_flags & 0x3fu;
        } else {
            memcpy(usb_tuning_status_snapshot.adapter + 1, adapter->information,
                   sizeof(adapter->information));
        }
    }
}

/**
 * @brief Accepts one host report for the motor-command bridge.
 *
 * Applies report-6 uploads, restart and release controls, and segmented response progress
 * acknowledgements before the generic HID output decoder sees the report.
 *
 * @param[in] report Host output or feature report.
 * @return True when the motor-command bridge owns the report.
 */
static bool accept_usb_motor_report(const UsbDeviceOutputReport *report) {
    if (report->length == USB_MOTOR_RESPONSE_ACKNOWLEDGEMENT_SIZE &&
        usb_motor_vendor_service_acknowledge_response(&usb_motor_vendor_service, report->data)) {
        return true;
    }
    if (report->report_id != USB_MOTOR_COMMAND_REPORT_ID ||
        report->length != USB_FEATURE_UPLOAD_PACKET_SIZE) {
        return false;
    }
    if (xbox_mode_startup_attempted && !xbox_mode_startup_finished) {
        return true;
    }
    UsbMotorVendorServiceResult result = usb_motor_vendor_service_accept_usb_mailbox(
        &usb_motor_vendor_service, &motor_command_mailbox, &command_transport, report->data,
        report->length, usb_motor_acknowledgement);
    if ((result.actions & USB_MOTOR_VENDOR_ACTION_WRITE_USB) != 0) {
        usb_motor_acknowledgement_ready = true;
        usb_motor_acknowledgement_length = result.usb_packet_length;
    }
    return (result.actions & USB_MOTOR_VENDOR_ACTION_CLAIM) != 0;
}

/**
 * @brief Starts the selected Xbox interface for a capable attached wheel.
 *
 * Runs only when retained base mode six is selected or no mode has been retained. It waits for a
 * supported wheel mode with tuning-menu capability, runs the motor-command identity exchange once,
 * and applies the resulting digest and wheel-specific product identity. An unavailable Xbox mode
 * falls back to native mode zero and persists that selection. The startup exchange has exclusive
 * use of the motor-command mailbox while active.
 *
 * @return True while the startup exchange owns the motor-command channel.
 */
static bool service_xbox_mode_startup(void) {
    if (xbox_mode_startup_finished || usb_device_operating_mode() != USB_OPERATING_MODE_FANATEC) {
        return false;
    }

    if (selected_base_mode != UINT8_MAX && selected_base_mode != 6) {
        return false;
    }

    uint8_t wheel_mode = wheel_service_mode(&wheel_service);
    if (!xbox_mode_startup_attempted) {
        if (wheel_service_protocol_phase(&wheel_service) != WHEEL_PROTOCOL_ACTIVE) {
            return false;
        }
        if (!wheel_service_tuning_menu_available(&wheel_service) ||
            usb_xbox_gip_mode_code(board_identity.variant, wheel_mode) == 0) {
            if (selected_base_mode == UINT8_MAX || selected_base_mode == 6) {
                selected_base_mode = 0;
                base_settings.operating_mode = 0;
                base_settings.operating_mode_valid = true;
                base_settings_persistence_mark_dirty(&settings_persistence);
                save_base_settings();
                xbox_mode_startup_finished = true;
            }
            return false;
        }
        motor_command_application_init(&motor_command_channel.application);
        motor_command_channel_reset(&motor_command_channel);
        motor_command_mailbox_exchange_reset(&motor_command_mailbox);
        motor_command_startup_service_init(&motor_command_startup_service);
        xbox_mode_startup_attempted = true;
    }

    MotorCommandStartupServiceResult result =
        motor_command_startup_service_run(&motor_command_startup_service, &motor_command_channel,
                                          &motor_command_mailbox, &command_transport);
    if (result == MOTOR_COMMAND_STARTUP_SERVICE_RUNNING) {
        return true;
    }
    xbox_mode_startup_finished = true;
    if (result == MOTOR_COMMAND_STARTUP_SERVICE_COMPLETE) {
        const MotorCommandApplication *application =
            motor_command_channel_application(&motor_command_channel);
        synchronize_xbox_controls_with_profile();
        if (usb_device_set_xbox_mode(wheel_mode, application->digest)) {
            console_wheel_mode = wheel_mode;
            selected_base_mode = 6;
            base_settings.operating_mode = 6;
            base_settings.operating_mode_valid = true;
            base_settings_persistence_mark_dirty(&settings_persistence);
            save_base_settings();
        }
    }
    return false;
}

/**
 * @brief Starts the selected PlayStation USB interface.
 *
 * Changes the base from its initial Fanatec interface when retained base mode two, four, or five is
 * selected. The change waits until the shared motor-command transport and retained response state
 * are idle, applies the matching PlayStation identity, and persists the active selection.
 *
 * @return True when PlayStation mode was selected; otherwise false.
 */
static bool service_playstation_mode_startup(void) {
    if (usb_device_operating_mode() != USB_OPERATING_MODE_FANATEC) {
        return false;
    }

    uint8_t base_mode = selected_base_mode;
    if (base_mode != 2 && base_mode != 4 && base_mode != 5) {
        return false;
    }
    if (command_transport.phase != COMMAND_TRANSPORT_IDLE ||
        motor_command_channel.command_pending || usb_motor_vendor_service.response_active) {
        return false;
    }
    if (!usb_device_set_playstation_wheel_mode(base_mode)) {
        return false;
    }
    console_wheel_mode = base_mode;
    selected_base_mode = base_mode;
    base_settings.operating_mode = base_mode;
    base_settings.operating_mode_valid = true;
    base_settings_persistence_mark_dirty(&settings_persistence);
    save_base_settings();
    return true;
}

/**
 * @brief Returns an invalid active console interface to native USB mode.
 *
 * Restarts Xbox startup when the attached-wheel mode changes. A PlayStation interface becomes stale
 * when its retained base mode changes, while an Xbox interface becomes stale when its
 * attached-wheel mode changes or loses Xbox support. The transition waits for all shared command
 * state to become idle before restoring the native interface.
 *
 * @return True when a stale console interface was returned to native USB mode.
 */
static bool service_console_mode_transition(void) {
    uint8_t wheel_mode = wheel_service_mode(&wheel_service);
    if (wheel_mode != observed_wheel_mode) {
        observed_wheel_mode = wheel_mode;
        xbox_mode_startup_attempted = false;
        xbox_mode_startup_finished = false;
    }

    UsbOperatingMode mode = usb_device_operating_mode();
    uint8_t base_mode = selected_base_mode;
    bool stale_playstation =
        mode == USB_OPERATING_MODE_PLAYSTATION &&
        ((base_mode != 2 && base_mode != 4 && base_mode != 5) || base_mode != console_wheel_mode);
    bool stale_xbox = mode == USB_OPERATING_MODE_XBOX_GIP &&
                      (usb_xbox_gip_mode_code(board_identity.variant, wheel_mode) == 0 ||
                       wheel_mode != console_wheel_mode);
    if (!stale_playstation && !stale_xbox) {
        return false;
    }
    if (command_transport.phase != COMMAND_TRANSPORT_IDLE ||
        motor_command_channel.command_pending || usb_motor_vendor_service.response_active ||
        !usb_device_set_operating_mode(USB_OPERATING_MODE_FANATEC)) {
        return false;
    }
    console_wheel_mode = UINT8_MAX;
    xbox_mode_startup_attempted = false;
    xbox_mode_startup_finished = false;
    return true;
}

/**
 * @brief Initializes PlayStation authentication and input services.
 *
 * Starts the A71CH SCI2C session sequence, clears authentication request and response state,
 * centers host-controlled wheel values, and resets retained input-button timing before the USB
 * interface can accept PlayStation traffic.
 */
static void initialize_playstation_services(void) {
    a71ch_session_service_init(&usb_operating_mode_workspace.playstation.session);
    a71ch_session_service_start(&usb_operating_mode_workspace.playstation.session);
    a71ch_authentication_service_init(&usb_operating_mode_workspace.playstation.authentication);
    usb_playstation_wheel_value_init(&usb_playstation_wheel_value);
    usb_playstation_input_mapper_init(&usb_playstation_input_mapper);
    wheel_service_set_legacy_axes(&wheel_service,
                                  usb_playstation_wheel_value_axes(&usb_playstation_wheel_value));
    playstation_authentication_response_published = false;
}

/**
 * @brief Services PlayStation authentication through the A71CH.
 *
 * Completes the SCI2C startup sequence, submits each assembled 256-byte host challenge without
 * LRC framing, and publishes the exact 1,040-byte response or an error status to USB.
 *
 * @param[in] now_ms Current monotonic time in milliseconds.
 */
static void service_playstation_authentication(uint32_t now_ms) {
    if (usb_device_operating_mode() != USB_OPERATING_MODE_PLAYSTATION) {
        return;
    }

    A71chSessionServiceStatus session_status =
        a71ch_session_service_status(&usb_operating_mode_workspace.playstation.session);
    if (session_status == A71CH_SESSION_SERVICE_RUNNING) {
        a71ch_session_service_run(&usb_operating_mode_workspace.playstation.session, now_ms);
        return;
    }
    if (session_status != A71CH_SESSION_SERVICE_COMPLETE) {
        return;
    }

    A71chAuthenticationServiceStatus authentication_status = a71ch_authentication_service_status(
        &usb_operating_mode_workspace.playstation.authentication);
    if (authentication_status == A71CH_AUTHENTICATION_SERVICE_RUNNING) {
        a71ch_authentication_service_run(&usb_operating_mode_workspace.playstation.authentication);
        return;
    }
    if (authentication_status == A71CH_AUTHENTICATION_SERVICE_COMPLETE) {
        if (!playstation_authentication_response_published) {
            const uint8_t *response = a71ch_authentication_service_response(
                &usb_operating_mode_workspace.playstation.authentication);
            playstation_authentication_response_published =
                response != 0 && usb_device_publish_playstation_authentication_response(
                                     response, A71CH_AUTHENTICATION_READ_SIZE);
        } else if (!usb_device_playstation_authentication_response_active()) {
            a71ch_authentication_service_init(
                &usb_operating_mode_workspace.playstation.authentication);
            playstation_authentication_response_published = false;
        }
        return;
    }
    if (authentication_status == A71CH_AUTHENTICATION_SERVICE_FAILED) {
        usb_device_fail_playstation_authentication();
        a71ch_authentication_service_init(&usb_operating_mode_workspace.playstation.authentication);
        return;
    }

    if (usb_device_take_playstation_authentication_request(
            usb_operating_mode_workspace.playstation.request)) {
        (void)a71ch_authentication_service_start(
            &usb_operating_mode_workspace.playstation.authentication,
            usb_operating_mode_workspace.playstation.request,
            sizeof(usb_operating_mode_workspace.playstation.request), false);
    }
}

/**
 * @brief Encodes the pending force-feedback script query response.
 *
 * Uses the shared script report formats for native and Xbox transports without consuming the
 * pending query. The caller commits it only after its transport accepts the response.
 *
 * @param[out] response Destination for the encoded response.
 * @return Encoded response length, or zero when no query is pending.
 */
static uint8_t encode_pending_force_feedback_script_report(uint8_t *response) {
    switch (force_feedback_script_report_pending) {
    case FORCE_FEEDBACK_SCRIPT_REPORT_AXES:
        return force_feedback_script_axes_report_encode(&force_feedback_script_system.values,
                                                        response, USB_DEVICE_REPORT_SIZE)
                   ? FORCE_FEEDBACK_SCRIPT_AXES_RESPONSE_SIZE
                   : 0;
    case FORCE_FEEDBACK_SCRIPT_REPORT_SAMPLES:
        return force_feedback_script_samples_report_encode(
                   &force_feedback_script_system.values, force_feedback_script_sample_report_index,
                   response, USB_DEVICE_REPORT_SIZE)
                   ? FORCE_FEEDBACK_SCRIPT_SAMPLES_RESPONSE_SIZE
                   : 0;
    case FORCE_FEEDBACK_SCRIPT_REPORT_SLOT:
        return force_feedback_script_slot_report_encode(&force_feedback_script_system.values,
                                                        force_feedback_script_slot_report_index,
                                                        response, USB_DEVICE_REPORT_SIZE)
                   ? FORCE_FEEDBACK_SCRIPT_SLOT_RESPONSE_SIZE
                   : 0;
    case FORCE_FEEDBACK_SCRIPT_REPORT_STATUS:
        return force_feedback_script_status_report_encode(&force_feedback_script_system.values,
                                                          force_feedback_script_system.mode,
                                                          response, USB_DEVICE_REPORT_SIZE)
                   ? FORCE_FEEDBACK_SCRIPT_STATUS_RESPONSE_SIZE
                   : 0;
    case FORCE_FEEDBACK_SCRIPT_REPORT_VALUES:
        return force_feedback_script_values_report_encode(&force_feedback_script_system.values,
                                                          response, USB_DEVICE_REPORT_SIZE)
                   ? FORCE_FEEDBACK_SCRIPT_VALUES_RESPONSE_SIZE
                   : 0;
    case FORCE_FEEDBACK_SCRIPT_REPORT_NONE:
        return 0;
    }
    return 0;
}

/**
 * @brief Builds the current Xbox GIP attached-device status.
 *
 * Collects the base identity, negotiated wheel capabilities, pedal state, shifter modes, thermal
 * limit, and motor-controller identity into the logical type-11 response model.
 *
 * @param[out] status Destination for the current logical extended status.
 */
static void build_xbox_extended_status(UsbXboxGipExtendedStatus *status) {
    const PedalV3State *pedal_state = pedal_service_v3_state(&pedal_service);
    bool h_pattern = shifter_input.primary_mode == SHIFTER_INPUT_H_PATTERN ||
                     shifter_input.secondary_mode == SHIFTER_INPUT_H_PATTERN;
    bool sequential = shifter_input.primary_mode == SHIFTER_INPUT_SEQUENTIAL ||
                      shifter_input.secondary_mode == SHIFTER_INPUT_SEQUENTIAL;
    *status = (UsbXboxGipExtendedStatus){
        .board_variant = board_identity.variant,
        .wheel_mode = wheel_service_mode(&wheel_service),
        .pedal_connection_flags = pedal_state->connection_flags,
        .auxiliary_axis_active = pedal_service.auxiliary_override_active ? 1 : 0,
        .axis_mode = h_pattern    ? 1
                     : sequential ? 2
                                  : 0,
        .transfer_code = motor_identity_input_transfer_code(motor_probe_identity(&motor_probe)),
        .multi_position_mode =
            wheel_service_multi_position_mode(&wheel_service, tuning_profile->multi_position_mode),
        .hardware_option = board_identity.hardware_option != 0,
        .h_pattern_available = h_pattern,
        .legacy_pedal_mode = pedal_service_legacy_transport_active(&pedal_service),
        .primary_pedal_calibration = pedal_state->primary_calibration,
        .secondary_pedal_calibration = pedal_state->secondary_calibration,
        .pedal_recovery_handshake = pedal_service.recovery_handshake,
        .thermal_effect_limit = cooling_effect_limit.active,
        .wheel_calibration_available = wheel_service_calibration_available(&wheel_service),
        .wheel_input_capability_available =
            wheel_service_input_capability_available(&wheel_service),
        .multi_position_supported = wheel_service_multi_position_supported(&wheel_service),
        .adapter_connected = wheel_service_adapter_connected(&wheel_service),
    };
}

/**
 * @brief Queues a pending Xbox GIP control response.
 *
 * Retains capability, attached-device status, and transfer-status requests until the active Xbox
 * endpoint can accept the corresponding response. Transfer status echoes the saved command packet
 * type and group.
 */
static void prepare_usb_xbox_control_response(void) {
    if (usb_device_operating_mode() != USB_OPERATING_MODE_XBOX_GIP) {
        return;
    }

    bool queued = false;
    switch (usb_xbox_control_response_pending) {
    case USB_XBOX_CONTROL_RESPONSE_CAPABILITIES:
        queued = usb_device_queue_xbox_capabilities();
        break;
    case USB_XBOX_CONTROL_RESPONSE_SNAPSHOT:
        queued = usb_device_queue_xbox_input(&xbox_input_snapshot);
        break;
    case USB_XBOX_CONTROL_RESPONSE_EXTENDED_STATUS: {
        build_xbox_extended_status(&xbox_extended_status);
        queued = usb_device_queue_xbox_extended_status(&xbox_extended_status);
        break;
    }
    case USB_XBOX_CONTROL_RESPONSE_TRANSFER_STATUS:
        queued = usb_device_queue_xbox_transfer_status(usb_xbox_control_request);
        break;
    case USB_XBOX_CONTROL_RESPONSE_NONE:
        return;
    }
    if (queued) {
        usb_xbox_control_response_pending =
            usb_xbox_control_response_pending == USB_XBOX_CONTROL_RESPONSE_CAPABILITIES
                ? USB_XBOX_CONTROL_RESPONSE_SNAPSHOT
                : USB_XBOX_CONTROL_RESPONSE_NONE;
    }
}

/**
 * @brief Queues a pending raw vendor report on Xbox GIP.
 *
 * Retains a marker-36 remote-tuning report until the active Xbox endpoint accepts all 64 bytes.
 * The report fields remain unchanged because this vendor format does not use the GIP sequence
 * envelope.
 */
static void prepare_usb_xbox_vendor_response(void) {
    if (usb_device_operating_mode() != USB_OPERATING_MODE_XBOX_GIP ||
        (usb_vendor_response_kind != USB_VENDOR_RESPONSE_NONE &&
         usb_vendor_response_kind != USB_VENDOR_RESPONSE_REMOTE_TUNING)) {
        return;
    }
    if (usb_vendor_response_kind == USB_VENDOR_RESPONSE_NONE) {
        if (!usb_remote_tuning_service_take_host_report(
                &usb_remote_tuning_service, wheel_service_mode(&wheel_service),
                USB_REMOTE_TUNING_HOST_XBOX, usb_vendor_response)) {
            return;
        }
        usb_vendor_response_length = USB_REMOTE_TUNING_HOST_REPORT_SIZE;
        usb_vendor_response_kind = USB_VENDOR_RESPONSE_REMOTE_TUNING;
    }
    if (usb_device_queue_xbox_vendor_report(usb_vendor_response)) {
        usb_vendor_response_kind = USB_VENDOR_RESPONSE_NONE;
    }
}

/**
 * @brief Publishes a pending remote-tuning report on PlayStation USB.
 *
 * Retains the 64-byte report until the PlayStation feature-report slot accepts it. Other vendor
 * response owners remain undisturbed while their response is pending.
 */
static void prepare_usb_playstation_remote_tuning_response(void) {
    if (usb_device_operating_mode() != USB_OPERATING_MODE_PLAYSTATION ||
        (usb_vendor_response_kind != USB_VENDOR_RESPONSE_NONE &&
         usb_vendor_response_kind != USB_VENDOR_RESPONSE_REMOTE_TUNING)) {
        return;
    }
    if (usb_vendor_response_kind == USB_VENDOR_RESPONSE_NONE) {
        if (!usb_remote_tuning_service_take_host_report(
                &usb_remote_tuning_service, wheel_service_mode(&wheel_service),
                USB_REMOTE_TUNING_HOST_PLAYSTATION, usb_vendor_response)) {
            return;
        }
        usb_vendor_response_length = USB_REMOTE_TUNING_HOST_REPORT_SIZE;
        usb_vendor_response_kind = USB_VENDOR_RESPONSE_REMOTE_TUNING;
    }
    if (usb_device_publish_playstation_remote_tuning_report(usb_vendor_response)) {
        usb_vendor_response_kind = USB_VENDOR_RESPONSE_NONE;
    }
}

/**
 * @brief Queues a pending force-feedback script response on Xbox GIP.
 *
 * Builds the common type-25 report and retains the query until the active Xbox endpoint accepts
 * it. The device layer assigns the shared GIP sequence.
 */
static void prepare_usb_xbox_script_response(void) {
    if (usb_device_operating_mode() != USB_OPERATING_MODE_XBOX_GIP ||
        force_feedback_script_report_pending == FORCE_FEEDBACK_SCRIPT_REPORT_NONE ||
        usb_vendor_response_kind != USB_VENDOR_RESPONSE_NONE) {
        return;
    }

    uint8_t length = encode_pending_force_feedback_script_report(usb_vendor_response);
    if (length != 0 && usb_device_queue_xbox_response(usb_vendor_response, length)) {
        force_feedback_script_report_pending = FORCE_FEEDBACK_SCRIPT_REPORT_NONE;
    }
}

/**
 * @brief Prepares the highest-priority pending vendor response.
 *
 * Retains sequence-bearing reports for endpoint retries. Native telemetry precedes wheel transfer,
 * pedal transfer, script, profile, menu, diagnostic, and motor responses in the same order used by
 * the host command service.
 */
static void prepare_usb_vendor_response(void) {
    if (usb_device_operating_mode() != USB_OPERATING_MODE_FANATEC) {
        return;
    }
    if (usb_vendor_response_kind == USB_VENDOR_RESPONSE_REMOTE_TUNING ||
        usb_vendor_response_kind == USB_VENDOR_RESPONSE_SCRIPT_REPORT) {
        return;
    }
    usb_vendor_response_kind = USB_VENDOR_RESPONSE_NONE;
    if (usb_remote_tuning_service_take_host_report(
            &usb_remote_tuning_service, wheel_service_mode(&wheel_service),
            USB_REMOTE_TUNING_HOST_NATIVE, usb_vendor_response)) {
        usb_vendor_response_length = USB_REMOTE_TUNING_HOST_REPORT_SIZE;
        usb_vendor_response_kind = USB_VENDOR_RESPONSE_REMOTE_TUNING;
        return;
    }

    WheelTransferRequest request = WHEEL_TRANSFER_READ;
    if (!usb_wheel_transfer_response_pending[request]) {
        request = WHEEL_TRANSFER_WRITE;
    }
    if (usb_wheel_transfer_response_pending[request]) {
        usb_vendor_command_encode_wheel_transfer_response(
            request, wheel_transfer_service_status(&wheel_transfer_service, request),
            usb_vendor_response);
        usb_vendor_response_length = USB_DEVICE_REPORT_SIZE;
        usb_vendor_wheel_response_request = request;
        usb_vendor_response_kind = USB_VENDOR_RESPONSE_WHEEL_TRANSFER;
        return;
    }
    if (usb_transfer_response_accepting(&usb_transfer_response) &&
        !usb_transfer_response_has_pedal_owner) {
        const PedalTransferResponse *response = pedal_service_transfer_response(&pedal_service);
        if (response != NULL &&
            usb_transfer_response_queue(&usb_transfer_response, response->data, response->length)) {
            usb_transfer_response_has_pedal_owner = true;
        }
    }
    if (usb_transfer_response_prepare(&usb_transfer_response, usb_vendor_response)) {
        usb_vendor_response_length = USB_DEVICE_REPORT_SIZE;
        usb_vendor_response_kind = USB_VENDOR_RESPONSE_PEDAL_TRANSFER;
        return;
    }
    usb_vendor_response_length = encode_pending_force_feedback_script_report(usb_vendor_response);
    if (usb_vendor_response_length != 0) {
        usb_vendor_response[2] =
            force_feedback_script_report_sequence_take(&force_feedback_script_response_sequence);
        force_feedback_script_report_pending = FORCE_FEEDBACK_SCRIPT_REPORT_NONE;
        usb_vendor_response_kind = USB_VENDOR_RESPONSE_SCRIPT_REPORT;
        return;
    }
    if (usb_tuning_profile_service_response_pending(&usb_tuning_profile_service)) {
        usb_tuning_profile_report_encode_response(&base_settings.tuning_profiles,
                                                  usb_vendor_response);
        usb_vendor_response_length = USB_DEVICE_REPORT_SIZE;
        usb_vendor_response_kind = USB_VENDOR_RESPONSE_TUNING_PROFILE;
        return;
    }
    if (usb_tuning_menu_service_response_pending(&usb_tuning_menu_service)) {
        usb_tuning_menu_service_encode_response(&usb_tuning_menu_service, usb_vendor_response);
        usb_vendor_response_length = USB_DEVICE_REPORT_SIZE;
        usb_vendor_response_kind = USB_VENDOR_RESPONSE_TUNING_MENU;
        return;
    }
    if (usb_diagnostic_report_prepare(&usb_diagnostic_report_service, &usb_diagnostic_snapshot,
                                      usb_vendor_response)) {
        usb_vendor_response_length = USB_DEVICE_REPORT_SIZE;
        usb_vendor_response_kind = USB_VENDOR_RESPONSE_DIAGNOSTIC;
        return;
    }
    if (usb_tuning_status_report_prepare(&usb_tuning_status_report_service,
                                         &usb_tuning_status_snapshot, usb_vendor_response)) {
        usb_vendor_response_length = USB_DEVICE_REPORT_SIZE;
        usb_vendor_response_kind = USB_VENDOR_RESPONSE_TUNING_STATUS;
        return;
    }
    usb_vendor_response_length =
        usb_motor_vendor_service_prepare_response(&usb_motor_vendor_service, usb_vendor_response);
    if (usb_vendor_response_length != 0) {
        usb_vendor_response_kind = USB_VENDOR_RESPONSE_MOTOR;
    }
}

/**
 * @brief Commits a submitted vendor response.
 *
 * Completes service-specific response state after the endpoint copies the retained report, then
 * releases the shared response image for the next pending owner.
 */
static void complete_usb_vendor_response(void) {
    if (usb_vendor_response_kind == USB_VENDOR_RESPONSE_WHEEL_TRANSFER) {
        usb_wheel_transfer_response_pending[usb_vendor_wheel_response_request] = false;
    } else if (usb_vendor_response_kind == USB_VENDOR_RESPONSE_TUNING_PROFILE) {
        usb_tuning_profile_service_response_sent(&usb_tuning_profile_service);
    } else if (usb_vendor_response_kind == USB_VENDOR_RESPONSE_TUNING_MENU) {
        usb_tuning_menu_service_response_sent(&usb_tuning_menu_service);
    } else if (usb_vendor_response_kind == USB_VENDOR_RESPONSE_PEDAL_TRANSFER) {
        usb_transfer_response_commit(&usb_transfer_response);
        if (usb_transfer_response_has_pedal_owner &&
            usb_transfer_response_accepting(&usb_transfer_response)) {
            pedal_service_release_transfer_response(&pedal_service);
            usb_transfer_response_has_pedal_owner = false;
        }
    } else if (usb_vendor_response_kind == USB_VENDOR_RESPONSE_DIAGNOSTIC) {
        usb_diagnostic_report_commit(&usb_diagnostic_report_service, usb_vendor_response);
    } else if (usb_vendor_response_kind == USB_VENDOR_RESPONSE_TUNING_STATUS) {
        usb_tuning_status_report_commit(&usb_tuning_status_report_service, usb_vendor_response);
    } else if (usb_vendor_response_kind == USB_VENDOR_RESPONSE_MOTOR) {
        (void)usb_motor_vendor_service_next_response(&usb_motor_vendor_service,
                                                     usb_vendor_response);
    }
    usb_vendor_response_kind = USB_VENDOR_RESPONSE_NONE;
}

/**
 * @brief Advances host command services over serial message type four.
 *
 * Queues remote-tuning responses and telemetry for the attached wheel, forwards system display
 * states, polls attached-adapter state, batches generic tuning records, advances wheel-transfer and
 * mailbox requests, and applies the retained console-mode selection at an idle transport boundary.
 * Motor mailbox and vendor-report work stop while PlayStation mode owns the shared workspace.
 *
 * @param[in] now_ms Current monotonic time in milliseconds.
 */
static void service_usb_command_bridge(uint32_t now_ms) {
    uint8_t remote_tuning_controls[REMOTE_TELEMETRY_REPORT_SIZE];
    if (wheel_service_take_remote_tuning_controls(&wheel_service, remote_tuning_controls)) {
        (void)usb_remote_tuning_service_queue_host_controls(&usb_remote_tuning_service,
                                                            remote_tuning_controls);
    }
    if (!wheel_service_remote_tuning_response_pending(&wheel_service) &&
        usb_remote_tuning_service_take_response(&usb_remote_tuning_service,
                                                wheel_service_mode(&wheel_service),
                                                &usb_remote_tuning_response)) {
        (void)wheel_service_queue_remote_tuning_response(&wheel_service,
                                                         &usb_remote_tuning_response);
    }
    if (!wheel_service_remote_telemetry_pending(&wheel_service) &&
        usb_remote_tuning_service_take_telemetry_report(&usb_remote_tuning_service,
                                                        wheel_service_mode(&wheel_service),
                                                        wheel_remote_telemetry_report)) {
        (void)wheel_service_queue_remote_telemetry(&wheel_service, wheel_remote_telemetry_report);
    }
    (void)motor_command_serial_receive(&command_transport, &serial_service);
    wheel_transfer_service_run(&wheel_transfer_service, &command_transport);
    wheel_accessory_service_run(&wheel_accessory_service, &command_transport);
    bool adapter_active_synchronization_allowed =
        wheel_service_mode(&wheel_service) != WHEEL_MODE_REMOTE_TUNING_EXTENDED ||
        !tuning_interaction_blocks_adapter_synchronization(&tuning_interaction);
    if (usb_remote_tuning_service_take_adapter_active(&usb_remote_tuning_service,
                                                      adapter_active_synchronization_allowed,
                                                      &wheel_adapter_remote_tuning_active)) {
        wheel_service_queue_adapter_remote_tuning_active(&wheel_service,
                                                         wheel_adapter_remote_tuning_active);
    }
    if (usb_remote_tuning_service_take_adapter_refresh_state(&usb_remote_tuning_service,
                                                             &wheel_adapter_refresh_state)) {
        wheel_service_queue_adapter_refresh_state(&wheel_service, wheel_adapter_refresh_state);
    }
    if (usb_remote_tuning_service_take_adapter_setup_selection(&usb_remote_tuning_service,
                                                               &wheel_adapter_setup_selection)) {
        wheel_service_queue_adapter_setup_selection(&wheel_service, wheel_adapter_setup_selection);
    }
    if (system_control_state_take_display_state(&system_control_state,
                                                &wheel_adapter_display_state)) {
        wheel_service_queue_adapter_display_state(&wheel_service, wheel_adapter_display_state);
    }
    wheel_service_run_adapter_commands(&wheel_service, &command_transport);
    if (wheel_service_take_adapter_host_controls(&wheel_service, wheel_adapter_host_controls)) {
        (void)usb_remote_tuning_service_queue_host_controls(&usb_remote_tuning_service,
                                                            wheel_adapter_host_controls);
    }
    if (wheel_command_forwarder_accepting(&wheel_command_forwarder) &&
        usb_remote_tuning_service_take_forward_batch(
            &usb_remote_tuning_service, wheel_service_mode(&wheel_service), wheel_command_batch,
            &wheel_command_batch_length)) {
        (void)wheel_command_forwarder_queue(&wheel_command_forwarder, wheel_command_batch,
                                            wheel_command_batch_length);
    }
    wheel_command_forwarder_run(&wheel_command_forwarder, &command_transport);
    (void)service_console_mode_transition();
    bool playstation_mode_selected = service_playstation_mode_startup();
    if (playstation_mode_selected) {
        initialize_playstation_services();
    }
    bool playstation_mode_active = usb_device_operating_mode() == USB_OPERATING_MODE_PLAYSTATION;
    if (!playstation_mode_active && !service_xbox_mode_startup()) {
        (void)usb_motor_vendor_service_run_mailbox(&usb_motor_vendor_service,
                                                   &motor_command_mailbox, &command_transport);
    }
    if (serial_service.status == SERIAL_SERVICE_IDLE) {
        (void)motor_command_serial_submit(&command_transport, &serial_service, now_ms);
    }
    if (playstation_mode_active) {
        prepare_usb_playstation_remote_tuning_response();
        return;
    }

    if (usb_motor_acknowledgement_ready &&
        usb_device_send_vendor_report(usb_motor_acknowledgement,
                                      usb_motor_acknowledgement_length)) {
        usb_motor_acknowledgement_ready = false;
    }
    update_usb_diagnostic_snapshot(now_ms);
    update_usb_tuning_status_snapshot();
    prepare_usb_xbox_control_response();
    prepare_usb_xbox_vendor_response();
    prepare_usb_xbox_script_response();
    prepare_usb_vendor_response();
    if (!usb_motor_acknowledgement_ready && usb_vendor_response_kind != USB_VENDOR_RESPONSE_NONE &&
        usb_device_send_vendor_report(usb_vendor_response, usb_vendor_response_length)) {
        complete_usb_vendor_response();
    }
}

/**
 * @brief Reports whether a type-four command is waiting for the shared serial link.
 *
 * Checks the command transport's queued request state without changing its owner or phase.
 *
 * @return True when a read or write request is queued for submission.
 */
static bool serial_command_waiting(void) {
    return command_transport.phase == COMMAND_TRANSPORT_WRITE_QUEUED ||
           command_transport.phase == COMMAND_TRANSPORT_READ_QUEUED;
}

/**
 * @brief Converts the updater route probe result to runtime transition input.
 *
 * Maps idle, pending, complete, and failed probe states without exposing updater transport
 * internals to the runtime transition controller.
 *
 * @param[in] status Current updater route probe result.
 * @return Corresponding runtime bridge transfer status.
 */
static RuntimeBridgeTransferStatus runtime_bridge_transfer_status(UsbUpdaterProbeStatus status) {
    switch (status) {
    case USB_UPDATER_PROBE_PENDING:
        return RUNTIME_BRIDGE_TRANSFER_PENDING;
    case USB_UPDATER_PROBE_COMPLETE:
        return RUNTIME_BRIDGE_TRANSFER_COMPLETE;
    case USB_UPDATER_PROBE_FAILED:
        return RUNTIME_BRIDGE_TRANSFER_FAILED;
    case USB_UPDATER_PROBE_IDLE:
    default:
        return RUNTIME_BRIDGE_TRANSFER_IDLE;
    }
}

/**
 * @brief Applies runtime bridge operations to clean platform services.
 *
 * Requests the auxiliary shutdown handshake, marks the status handshake, detaches USB during its
 * settling interval, selects the updater link, starts the common route probe, and activates the
 * updater descriptor and request service, or restores the prepared normal profile after a failed
 * startup recovery probe. Transfer timer actions require no separate operation because the
 * selected transport owns its timing source.
 *
 * @param[in] actions Independent runtime bridge action flags.
 */
static void apply_runtime_bridge_actions(uint16_t actions) {
    if ((actions & RUNTIME_BRIDGE_ACTION_REQUEST_AUXILIARY_HANDSHAKE) != 0) {
        usb_updater_service_request_auxiliary_handshake(&usb_updater_service);
    }
    if ((actions & RUNTIME_BRIDGE_ACTION_MARK_WHEEL_STATUS) != 0) {
        wheel_status_service_mark_next_request(&wheel_status_service);
    }
    if ((actions & RUNTIME_BRIDGE_ACTION_PREPARE_USB) != 0) {
        platform_usb_detach();
    }
    if ((actions & RUNTIME_BRIDGE_ACTION_INITIALIZE_DIRECT_TRANSFER) != 0) {
        platform_serial_link_enter_direct_mode();
    }
    if ((actions & RUNTIME_BRIDGE_ACTION_START_TRANSFER) != 0) {
        (void)usb_updater_service_start_probe(&usb_updater_service);
    }
    if ((actions & RUNTIME_BRIDGE_ACTION_REQUEST_PROTOCOL_COMMAND) != 0) {
        (void)wheel_protocol_bridge_service_request(&wheel_protocol_bridge_service);
    }
    if ((actions & RUNTIME_BRIDGE_ACTION_ACTIVATE_UPDATER_USB) != 0 &&
        usb_device_set_operating_mode(USB_OPERATING_MODE_UPDATER)) {
        usb_updater_service_set_usb_active(&usb_updater_service, true);
    }
    if ((actions & RUNTIME_BRIDGE_ACTION_RESTORE_NORMAL_USB) != 0) {
        platform_usb_attach();
    }
}

/**
 * @brief Transfers control to the wheel-base bootloader.
 *
 * Disconnects USB, completes the display-controller reset sequence, waits through the required
 * settling interval, records the bootloader handoff request, and restarts the processor.
 *
 */
static void enter_bootloader(void) {
    platform_usb_detach();
    platform_display_reset();
    platform_system_enter_bootloader();
}

/**
 * @brief Returns the wheel to its retained center before USB startup.
 *
 * Polls motor readiness for up to five seconds, then applies the four-second restoring-force
 * profile when readiness is reported. The loop continues motor packets, auxiliary transfers, and
 * power-button handling; force uses full command availability and the board's reduced Torque Key
 * limit.
 *
 */
static void run_motor_startup_centering(void) {
    motor_startup_centering_init(&motor_startup_centering, platform_time_ms());
    ForceOutputScale scale = {
        .available_percent = cooling_controller.force_scale_percent,
        .tuning_strength_percent = FORCE_FEEDBACK_FULL_STRENGTH_PERCENT,
        .output_strength_percent = motor_tuning_context.strength_percent,
        .secondary_output_disabled = false,
    };

    while (!motor_startup_centering_complete(&motor_startup_centering)) {
        platform_aux_bus_service();
        service_motor_link();
        uint32_t now_ms = platform_time_ms();
        int32_t centered_position =
            motor_position_ready ? wheel_position_center(motor_position_report.wheel_position,
                                                         base_settings.wheel_position.center)
                                 : 0;
        int32_t force = motor_startup_centering_run(&motor_startup_centering, now_ms,
                                                    motor_position_ready, centered_position);
        force_output_scale_apply(force, 0, scale, &motor_output_report);
        service_power(now_ms);
    }
    motor_output_report = (ForceOutputReport){0};
}

/**
 * @brief Selects the startup USB path after motor-controller discovery.
 *
 * Services the shared auxiliary bus through the one-second discovery window. A recognized
 * controller starts the normal USB profile immediately. A missing controller leaves that profile
 * detached while runtime mode two probes the auxiliary updater endpoint; a rejected recovery setup
 * falls back to the prepared normal profile.
 */
static void initialize_startup_usb(void) {
    while (motor_probe.phase != MOTOR_PROBE_COMPLETE && motor_probe.phase != MOTOR_PROBE_FAILED) {
        platform_aux_bus_service();
        motor_probe_run(&motor_probe, platform_time_ms());
    }
    if (motor_probe_identity(&motor_probe) != NULL) {
        run_motor_startup_centering();
        usb_device_init(board_identity.variant);
        motor_startup_direct_force_deadline_ms =
            platform_time_ms() + MOTOR_STARTUP_WHEEL_DISCOVERY_TIMEOUT_MS;
        motor_startup_wheel_discovery_started = true;
        return;
    }

    platform_aux_bus_clear();
    usb_device_prepare(board_identity.variant);
    if (!usb_updater_service_select_startup_recovery(&usb_updater_service)) {
        platform_usb_attach();
        return;
    }
    apply_runtime_bridge_actions(
        runtime_bridge_start_auxiliary_recovery(&runtime_bridge, platform_time_ms()));
}

/**
 * @brief Starts an accepted runtime transition.
 *
 * Handles reset requests immediately. For updater modes, decodes the route against the active
 * host, wheel, and motor modes, persists settings when required, initializes the selected service,
 * disables force output, and applies the transition's initial action.
 *
 * @param[in] command Decoded operating-mode command.
 * @return True when an accepted runtime transition started; otherwise false.
 */
static bool start_runtime_bridge(const UsbOperatingModeCommand *command) {
    if (!usb_operating_mode_command_decode_runtime(
            command, (uint8_t)usb_device_operating_mode(), wheel_service_mode(&wheel_service),
            motor_identity_runtime_state(motor_probe_identity(&motor_probe)),
            &usb_runtime_transition)) {
        return false;
    }
    if (usb_runtime_transition.mode == USB_RUNTIME_MODE_RESET) {
        enter_bootloader();
        return true;
    }
    if ((usb_runtime_transition.mode != USB_RUNTIME_MODE_AUXILIARY &&
         usb_runtime_transition.mode != USB_RUNTIME_MODE_AUXILIARY_RECOVERY &&
         usb_runtime_transition.mode != USB_RUNTIME_MODE_STATUS_BRIDGE &&
         usb_runtime_transition.mode != USB_RUNTIME_MODE_USB_BRIDGE &&
         usb_runtime_transition.mode != USB_RUNTIME_MODE_PROTOCOL_BRIDGE) ||
        !usb_updater_service_select_mode(&usb_updater_service, usb_runtime_transition.mode)) {
        return false;
    }
    if (usb_runtime_transition.save_settings) {
        save_base_settings();
    }
    if (usb_runtime_transition.mode == USB_RUNTIME_MODE_USB_BRIDGE) {
        wheel_usb_bridge_gate_init(&wheel_usb_bridge_gate);
        (void)wheel_service_take_protocol_exchange_completed(&wheel_service);
    }
    force_output_enabled = false;
    motor_output_report = (ForceOutputReport){0};
    platform_cooling_set_duty(0, 0, true);
    apply_runtime_bridge_actions(
        runtime_bridge_start(&runtime_bridge, usb_runtime_transition.mode));
    return true;
}

/**
 * @brief Advances an active updater transition and service.
 *
 * Keeps the motor link on disabled zero-force frames, finishes any in-flight wheel exchange,
 * advances auxiliary-bus, raw-link, or command-routed updater probes, handles the protocol
 * fallback callback, services updater USB after activation, and handles a guarded updater reset
 * request by disabling interrupts and immediately restarting the processor.
 *
 * @param[in] now_ms Current monotonic time in milliseconds.
 * @return True while runtime bridge mode owns the main loop; otherwise false.
 */
static bool service_runtime_bridge(uint32_t now_ms) {
    if (runtime_bridge.phase == RUNTIME_BRIDGE_IDLE) {
        return false;
    }
    service_motor_link();
    bool serial_route = runtime_bridge.mode == USB_RUNTIME_MODE_USB_BRIDGE ||
                        runtime_bridge.mode == USB_RUNTIME_MODE_PROTOCOL_BRIDGE ||
                        runtime_bridge.mode == USB_RUNTIME_MODE_PROTOCOL_RECOVERY;
    if (runtime_bridge.phase == RUNTIME_BRIDGE_WAIT_WHEEL_STATUS || serial_route) {
        serial_service_run(&serial_service, now_ms);
        (void)motor_command_serial_receive(&command_transport, &serial_service);
        wheel_service_run(&wheel_service, now_ms, false);
    }
    if (runtime_bridge.phase == RUNTIME_BRIDGE_WAIT_WHEEL_STATUS) {
        wheel_status_service_run(&wheel_status_service, now_ms, true);
    }

    usb_updater_input = (UsbUpdaterServiceInput){
        .now_ms = now_ms,
        .board_variant = board_identity.variant,
        .wheel_mode = wheel_service_mode(&wheel_service),
        .adapter_connected = wheel_service_adapter_connected(&wheel_service),
    };
    usb_updater_service_run(&usb_updater_service, &usb_updater_input);
    wheel_protocol_bridge_service_run(&wheel_protocol_bridge_service);
    WheelUsbBridgeGateResult usb_bridge_gate_result = WHEEL_USB_BRIDGE_GATE_NONE;
    if (runtime_bridge.phase == RUNTIME_BRIDGE_WAIT_USB_READY) {
        usb_bridge_gate_result = wheel_usb_bridge_gate_step(
            &wheel_usb_bridge_gate, now_ms,
            wheel_service_take_protocol_exchange_completed(&wheel_service));
        if (usb_bridge_gate_result == WHEEL_USB_BRIDGE_GATE_CLEAR_ACKNOWLEDGEMENT ||
            usb_bridge_gate_result == WHEEL_USB_BRIDGE_GATE_SET_ACKNOWLEDGEMENT) {
            wheel_protocol_set_response_acknowledged(&wheel_service.protocol,
                                                     usb_bridge_gate_result ==
                                                         WHEEL_USB_BRIDGE_GATE_SET_ACKNOWLEDGEMENT);
        }
    }
    runtime_bridge_input = (RuntimeBridgeInput){
        .now_ms = now_ms,
        .transfer_status =
            runtime_bridge_transfer_status(usb_updater_service_probe_status(&usb_updater_service)),
        .marked_wheel_status_received =
            wheel_status_service_take_marked_response(&wheel_status_service),
        .auxiliary_handshake_complete =
            usb_updater_service_auxiliary_handshake_complete(&usb_updater_service),
        .protocol_command_acknowledged =
            wheel_protocol_bridge_service_take_acknowledgement(&wheel_protocol_bridge_service),
        .usb_bridge_ready = usb_bridge_gate_result == WHEEL_USB_BRIDGE_GATE_RELEASE,
    };
    apply_runtime_bridge_actions(runtime_bridge_step(&runtime_bridge, &runtime_bridge_input));
    if (serial_route && serial_service.status == SERIAL_SERVICE_IDLE) {
        if (runtime_bridge.phase == RUNTIME_BRIDGE_WAIT_USB_READY &&
            command_transport.phase == COMMAND_TRANSPORT_IDLE) {
            (void)wheel_service_start_protocol_exchange(&wheel_service, now_ms);
        } else {
            (void)motor_command_serial_submit(&command_transport, &serial_service, now_ms);
        }
    }
    if (usb_updater_service_take_reset(&usb_updater_service)) {
        platform_system_interrupts_set(false);
        platform_system_reset();
    }
    return true;
}

/**
 * @brief Advance the force-feedback script clocks for one Timer 2 interrupt.
 *
 * Advances the shared engine clock in active and zero-output modes, advances the selected slot
 * clock while a script is executing, and always advances the motion-sampling clock.
 *
 * @param[in,out] context Initialized force-feedback script runtime supplied to the timer service.
 */
static void handle_force_feedback_timer_tick(void *context) {
    ForceFeedbackScriptSystem *system = context;
    force_feedback_script_clock_tick(&system->clock, system->mode);
}

/**
 * @brief Resets force-feedback script scheduling and output state.
 *
 * Restores position-only mode, clears scripts, samples, inputs, timing, smoothing, range-limit
 * history, report selection, and response sequencing, and starts the force ramp at zero.
 *
 */
static void reset_force_feedback_script(void) {
    force_feedback_script_runtime_init(&force_feedback_script_system);
    force_feedback_script_output_init(&force_feedback_script_output_state);
    force_feedback_script_response_sequence = 1;
    force_feedback_script_report_pending = FORCE_FEEDBACK_SCRIPT_REPORT_NONE;
    force_feedback_script_sample_report_index = 0;
    force_feedback_script_slot_report_index = 0;
    force_feedback_ramp_deadline_ms = 0;
}

/**
 * @brief Initializes force-feedback script processing and its hardware clock.
 *
 * Resets the complete script subsystem, then starts the dedicated Timer 2 clock used for engine,
 * slot, and wheel-motion timing.
 *
 */
static void initialize_force_feedback_script(void) {
    reset_force_feedback_script();
    platform_force_feedback_timer_init(handle_force_feedback_timer_tick,
                                       &force_feedback_script_system);
}

/**
 * @brief Applies pending Xbox GIP session side effects.
 *
 * Resets script processing without restarting its clock, or completes host-requested shutdown and
 * processor-reset transitions after the endpoint service queues its ready response. Shutdown and
 * reset store pending settings first. Shutdown also removes force output, disconnects USB, and
 * releases the external power hold.
 *
 */
static void service_usb_xbox_session_actions(void) {
    UsbXboxGipSessionAction actions = usb_device_take_xbox_session_actions();
    if ((actions & USB_XBOX_GIP_SESSION_ACTION_RESET_FORCE_FEEDBACK) != 0) {
        reset_force_feedback_script();
    }
    if ((actions & USB_XBOX_GIP_SESSION_ACTION_SUSPEND_OUTPUT) != 0) {
        platform_system_interrupts_set(false);
        save_base_settings();
        platform_system_interrupts_set(true);
        force_output_enabled = false;
        motor_output_report = (ForceOutputReport){0};
        platform_usb_detach();
        platform_power_latch_set(false);
    }
    if ((actions & USB_XBOX_GIP_SESSION_ACTION_RESET_DEVICE) != 0) {
        platform_system_interrupts_set(false);
        save_base_settings();
        platform_system_reset();
    }
}

/**
 * @brief Advances script-generated live force output.
 *
 * Raises the startup ramp by one percent after each elapsed 50-millisecond deadline, keeps the
 * motor's natural-friction tuning synchronized, and services due script ticks after a motor
 * position is available. A latched motor fault clears primary force and skips script evaluation.
 * Output uses the active profile, thermal limit, Torque Key strength, centered wheel travel, and
 * secondary-output gate.
 *
 * @param[in] now_ms Current monotonic time in milliseconds.
 */
static void service_force_feedback_script(uint32_t now_ms) {
    if (motor_tuning_context.ramp_percent < FORCE_FEEDBACK_FULL_STRENGTH_PERCENT &&
        now_ms > force_feedback_ramp_deadline_ms) {
        ++motor_tuning_context.ramp_percent;
        force_feedback_ramp_deadline_ms = now_ms + FORCE_FEEDBACK_RAMP_INTERVAL_MS;
        if (motor_tuning_ready) {
            motor_tuning_service_refresh(&motor_tuning_service, tuning_profile,
                                         &motor_tuning_context);
        }
    }

    if (motor_tuning_ready && motor_status_service_output_inhibited(&motor_status_service)) {
        motor_output_report.primary_magnitude = 0;
        return;
    }

    if (!motor_position_ready) {
        return;
    }

    uint32_t travel = wheel_position_travel_from_degrees(tuning_profile->rotation_degrees);
    if (travel == 0) {
        return;
    }

    int32_t position = wheel_position_center(motor_position_report.wheel_position,
                                             base_settings.wheel_position.center);
    uint16_t rotation_range = tuning_profile->rotation_degrees / 10u;
    force_feedback_script_system.values.extended_rotation_range = rotation_range;
    force_feedback_script_system.values.rotation_range_code =
        tuning_profile->automatic_rotation ? 126u
        : rotation_range > 125u            ? 127u
                                           : (uint8_t)rotation_range;
    force_feedback_script_output_config = (ForceFeedbackScriptOutputConfig){
        .soft_stop = {.travel_limit = (int32_t)travel},
        .available_percent = cooling_controller.force_scale_percent,
        .output_strength_percent = motor_tuning_context.strength_percent,
        .automatic_strength = board_identity.variant == BOARD_VARIANT_DD1
                                  ? FORCE_FEEDBACK_DD1_AUTOMATIC_STRENGTH_PERCENT
                                  : FORCE_FEEDBACK_DD2_AUTOMATIC_STRENGTH_PERCENT,
        .ramp_percent = motor_tuning_context.ramp_percent,
        .smoothing_intensity = tuning_profile->force_effect_intensity,
        .tuning_strength = (int8_t)tuning_profile->force_feedback_strength,
        .secondary_output_disabled = force_feedback_state.secondary_output_disabled,
    };
    ForceFeedbackScriptTickResult result = force_feedback_script_tick_output(
        &force_feedback_script_system, &force_feedback_script_output_state, now_ms, position,
        travel, &force_feedback_script_output_config, &motor_output_report);
    if (result.slot_faulted) {
        force_feedback_script_report_pending = FORCE_FEEDBACK_SCRIPT_REPORT_STATUS;
    }
}

/**
 * @brief Publishes one pending motor status event to the system event queue.
 *
 * Waits for the single event slot, transfers the motor event exactly once, and retains the accepted
 * code in shared control state for status consumers.
 */
static void publish_motor_status_event(void) {
    if (system_event_queue.pending_code != 0) {
        return;
    }
    MotorStatusEvent event = motor_status_service_take_event(&motor_status_service);
    if (event != MOTOR_STATUS_EVENT_NONE &&
        system_event_queue_try_push(&system_event_queue, (uint8_t)event)) {
        system_control_state_set_active_event(&system_control_state, (uint8_t)event);
    }
}

/**
 * @brief Publishes one pending motor-calibration event to the system event queue.
 *
 * Waits for the single event slot, transfers the calibration display event exactly once, and
 * retains the accepted code in shared control state for status consumers.
 */
static void publish_motor_calibration_event(void) {
    if (system_event_queue.pending_code != 0) {
        return;
    }

    MotorCalibrationEvent event = motor_calibration_service_take_event(&motor_calibration_service);
    if (event == MOTOR_CALIBRATION_EVENT_NONE) {
        return;
    }

    if (system_event_queue_try_push(&system_event_queue, (uint8_t)event)) {
        system_control_state_set_active_event(&system_control_state, (uint8_t)event);
    }
}

/**
 * @brief Refreshes motor tuning when its runtime context changes.
 *
 * Tracks Xbox operation and motor-calibration ownership, then reapplies the active tuning profile
 * only when either context flag changes and motor tuning is available.
 */
static void refresh_motor_tuning_context(void) {
    uint8_t xbox_mode = usb_device_operating_mode() == USB_OPERATING_MODE_XBOX_GIP;
    uint8_t calibration_active = motor_calibration_service_pending(&motor_calibration_service);
    if (motor_tuning_context.xbox_mode == xbox_mode &&
        motor_tuning_context.calibration_active == calibration_active) {
        return;
    }
    motor_tuning_context.xbox_mode = xbox_mode;
    motor_tuning_context.calibration_active = calibration_active;
    if (motor_tuning_ready) {
        motor_tuning_service_refresh(&motor_tuning_service, tuning_profile, &motor_tuning_context);
    }
}

/**
 * @brief Services motor discovery, configuration, telemetry, and status exchange.
 *
 * Initializes protocol-specific services after discovery, schedules calibration or normal bus
 * users, and publishes motor-originated operator events through the shared event boundary.
 */
static void service_motor(void) {
    motor_probe_run(&motor_probe, platform_time_ms());
    const MotorIdentity *identity = motor_probe_identity(&motor_probe);
    if (!motor_tuning_ready && identity != 0) {
        motor_tuning_context.extended_parameters = motor_identity_has_extended_parameters(identity);
        motor_telemetry_service_init(&motor_telemetry_service, identity);
        motor_status_service_init(&motor_status_service, identity);
        motor_tuning_service_init(&motor_tuning_service, tuning_profile, &motor_tuning_context);
        motor_tuning_ready = true;
    }
    if (motor_tuning_ready && motor_command_request_pending) {
        motor_status_service_request_command(&motor_status_service);
        motor_command_request_pending = false;
    }
    if (motor_tuning_ready) {
        refresh_motor_tuning_context();
        publish_motor_status_event();
        publish_motor_calibration_event();
        bool calibration_pending = motor_calibration_service_pending(&motor_calibration_service);
        bool calibration_can_run = motor_calibration_service_owns_bus(&motor_calibration_service) ||
                                   platform_aux_bus_status() == PLATFORM_AUX_BUS_IDLE;
        if (calibration_pending && calibration_can_run) {
            motor_calibration_service_run(
                &motor_calibration_service, wheel_service_mode(&wheel_service),
                motor_telemetry_service_value(&motor_telemetry_service), platform_time_ms());
        }
        if (!motor_calibration_service_owns_bus(&motor_calibration_service)) {
            motor_telemetry_service_run(&motor_telemetry_service, platform_time_ms());
            motor_status_service_run(&motor_status_service, platform_time_ms());
            motor_tuning_service_run(&motor_tuning_service);
        }
        publish_motor_status_event();
        publish_motor_calibration_event();
        refresh_motor_tuning_context();
    }
}

/**
 * @brief Monitors a stalled motor counter at the minimum steering limit.
 *
 * Converts the current wheel position to its published steering axis, supplies the latest motor
 * runtime counter, and queues the persistent restart warning after an unchanged 4.5-second hold.
 *
 * @param[in] now_ms Current monotonic time in milliseconds.
 */
static void service_motor_rotation_guard(uint32_t now_ms) {
    int32_t steering_axis = 1;
    if (motor_position_ready && wheel_position_calibration.travel != 0) {
        steering_axis = wheel_position_hid_axis(motor_position_report.wheel_position,
                                                &wheel_position_calibration);
    }
    const MotorTelemetry *telemetry =
        motor_tuning_ready ? motor_telemetry_service_value(&motor_telemetry_service) : NULL;
    uint32_t runtime_seconds =
        telemetry != NULL && telemetry->runtime_valid ? telemetry->runtime_seconds : 0;
    if (motor_rotation_guard_update(&motor_rotation_guard, steering_axis, runtime_seconds,
                                    now_ms)) {
        (void)system_event_queue_try_push(&system_event_queue,
                                          MAXIMUM_ROTATIONS_EXCEEDED_EVENT_CODE);
    }
}

/**
 * @brief Forwards a host force-feedback command to the motor controller.
 *
 * Queues full seven-byte records for configuration and position-effect activation. Individual
 * clears carry only their opcode, while a reset queues one clear for each host effect. Primary and
 * secondary output commands are represented by status bits in the next motor-link packet.
 *
 * @param[in] command Decoded force-feedback command kind.
 * @param[in] payload Original seven-byte host command.
 */
static void forward_force_feedback_command(const ForceFeedbackCommand *command,
                                           const uint8_t payload[MOTOR_OUTPUT_COMMAND_SIZE]) {
    switch (command->kind) {
    case FORCE_FEEDBACK_COMMAND_CONFIGURE_KIND_1:
    case FORCE_FEEDBACK_COMMAND_CONFIGURE_KIND_2:
    case FORCE_FEEDBACK_COMMAND_CONFIGURE_KIND_3:
    case FORCE_FEEDBACK_COMMAND_ACTIVATE_POSITION_EFFECT:
        motor_output_transport_enqueue_command(&motor_output_transport, payload);
        break;

    case FORCE_FEEDBACK_COMMAND_CLEAR_EFFECT:
    case FORCE_FEEDBACK_COMMAND_CLEAR_POSITION_EFFECT:
        motor_output_transport_enqueue_opcode(&motor_output_transport, payload[0]);
        break;

    case FORCE_FEEDBACK_COMMAND_RESET_EFFECTS:
        (void)motor_output_transport_enqueue_host_effect_clears(&motor_output_transport);
        break;

    case FORCE_FEEDBACK_COMMAND_SET_PRIMARY_OUTPUT:
    case FORCE_FEEDBACK_COMMAND_SET_SECONDARY_OUTPUT:
        break;
    }
}

/**
 * @brief Clears protocol outputs associated with a host force-feedback session.
 *
 * Centers the retained wheel-value override, clears the pedal protocol tuple and scale, and resets
 * the attached-wheel legacy axes, compact reports, and shared auxiliary report.
 */
static void reset_host_force_feedback_outputs(void) {
    usb_playstation_wheel_value_init(&usb_playstation_wheel_value);
    pedal_service_reset_protocol_status(&pedal_service);
    wheel_service_reset_host_protocol_outputs(&wheel_service);
}

/**
 * @brief Applies a transient automatic steering-range update.
 *
 * Changes range only for the official 1300-degree or automatic sensitivity gates, rescales the
 * base-side position effects, and refreshes the motor tuning service with the new concrete range.
 *
 * @param[in] degrees Requested lock-to-lock steering range.
 */
static void apply_fallback_steering_range(uint16_t degrees) {
    if (!usb_fallback_tuning_range_allowed(tuning_profile)) {
        return;
    }
    uint32_t previous = wheel_position_travel_from_degrees(tuning_profile->rotation_degrees);
    if (degrees < TUNING_ROTATION_MIN_DEGREES) {
        degrees = TUNING_ROTATION_MIN_DEGREES;
    } else if (degrees > TUNING_ROTATION_MAX_DEGREES) {
        degrees = TUNING_ROTATION_MAX_DEGREES;
    }
    runtime_tuning_profile.rotation_degrees =
        (uint16_t)(degrees / TUNING_ROTATION_STEP_DEGREES * TUNING_ROTATION_STEP_DEGREES);
    uint32_t current = wheel_position_travel_from_degrees(tuning_profile->rotation_degrees);
    (void)force_feedback_state_rescale_positions(&force_feedback_state, (int32_t)previous,
                                                 (int32_t)current);
    motor_tuning_context.automatic_rotation_degrees = tuning_profile->rotation_degrees;
    if (motor_tuning_ready) {
        motor_tuning_service_refresh(&motor_tuning_service, tuning_profile, &motor_tuning_context);
    }
}

/**
 * @brief Applies an active-slot fallback tuning command.
 *
 * The direct tuning interface accepts official setup one only. Values use their command-specific
 * clamps and update runtime consumers without changing retained setup storage. Sensitivity also
 * rescales base-side position effects.
 *
 * @param[in] command Decoded fallback tuning command.
 */
static void apply_fallback_tuning(const UsbFallbackCommand *command) {
    uint32_t previous = wheel_position_travel_from_degrees(tuning_profile->rotation_degrees);
    if (!usb_fallback_tuning_apply(command, base_settings.tuning_profiles.active_slot,
                                   &runtime_tuning_profile)) {
        return;
    }
    refresh_runtime_tuning_profile();
    if (command->kind == USB_FALLBACK_SENSITIVITY) {
        uint32_t current = wheel_position_travel_from_degrees(tuning_profile->rotation_degrees);
        (void)force_feedback_state_rescale_positions(&force_feedback_state, (int32_t)previous,
                                                     (int32_t)current);
    }
}

/**
 * @brief Expands compact display flags to the nine-bit auxiliary report.
 *
 * The five input flags select the top indicator and four adjacent two-indicator bands.
 *
 * @param[in] flags Compact display flag byte.
 * @return Expanded nine-bit auxiliary report.
 */
static uint16_t fallback_display_report(uint8_t flags) {
    uint16_t report = (flags & 1U) != 0 ? 0x100U : 0U;
    if ((flags & 2U) != 0) {
        report |= 0x0c0U;
    }
    if ((flags & 4U) != 0) {
        report |= 0x030U;
    }
    if ((flags & 8U) != 0) {
        report |= 0x00cU;
    }
    if ((flags & 16U) != 0) {
        report |= 0x003U;
    }
    return report;
}

/**
 * @brief Applies one decoded direct fallback command.
 *
 * Routes steering, tuning, display, cooling, and security operations to their existing subsystem
 * boundaries. Security disable is stored immediately; tuning changes remain transient.
 *
 * @param[in] command Decoded direct fallback command.
 */
static void apply_fallback_command(const UsbFallbackCommand *command) {
    switch (command->kind) {
    case USB_FALLBACK_STEERING_RANGE_LOW:
        apply_fallback_steering_range(200);
        break;
    case USB_FALLBACK_STEERING_RANGE_HIGH:
        apply_fallback_steering_range(TUNING_ROTATION_MAX_DEGREES);
        break;
    case USB_FALLBACK_DISPLAY_FLAGS:
        wheel_service_set_auxiliary_report(&wheel_service,
                                           fallback_display_report(command->parameters[0]));
        break;
    case USB_FALLBACK_STEERING_LIMIT: {
        uint16_t degrees = command->value <= 89 ? TUNING_ROTATION_MIN_DEGREES
                                                : (uint16_t)(command->value / 10U * 10U);
        apply_fallback_steering_range(degrees);
        break;
    }
    case USB_FALLBACK_COOLING_OVERRIDE:
        cooling_controller_apply_service_override(&cooling_controller, command->parameters[0],
                                                  command->parameters[1], command->parameters[2],
                                                  command->parameters[3]);
        if (cooling_controller.automatic_control_suspended) {
            platform_cooling_set_duty(cooling_controller.primary_duty_percent,
                                      cooling_controller.secondary_duty_percent, false);
        }
        break;
    case USB_FALLBACK_SECURITY_DISABLE:
        base_settings.security_code.enabled = false;
        base_settings_persistence_mark_dirty(&settings_persistence);
        save_base_settings();
        break;
    default:
        apply_fallback_tuning(command);
        break;
    }
}

/**
 * @brief Applies one nested F8 09 device-control command.
 *
 * Routes direct wheel display and protocol values, pedal discovery, tuning presentation, shifter
 * display, force-feedback reset, and repeated host-interface activation through existing services.
 *
 * @param[in] command Decoded F8 09 operating-mode command.
 * @return True when opcode one selected a supported device-control operation.
 */
static bool apply_fallback_device_command(const UsbOperatingModeCommand *command) {
    if (command->opcode != 1) {
        return false;
    }

    switch (command->parameters[0]) {
    case 0x02: {
        if (!usb_remote_tuning_service.active) {
            WheelDisplayOutput output = *wheel_service_default_display_output(&wheel_service);
            output.glyphs[0] = command->parameters[1];
            output.glyphs[1] = command->parameters[2];
            output.glyphs[2] = command->parameters[3];
            wheel_service_set_display_output(&wheel_service, &output);
        }
        return true;
    }
    case 0x03:
        if (tuning_profile->brake_indicator_level == 101) {
            usb_playstation_wheel_value_set(&usb_playstation_wheel_value, command->parameters[1],
                                            command->parameters[2], platform_time_ms());
            usb_playstation_wheel_value_set_axis_copy(&usb_playstation_wheel_value,
                                                      command->parameters[3] == 1);
            wheel_service_set_legacy_axes(
                &wheel_service, usb_playstation_wheel_value_axes(&usb_playstation_wheel_value));
        }
        return true;
    case 0x05:
        pedal_service_request_startup(&pedal_service);
        return true;
    case 0x18:
        wheel_service_set_display_character_mode(&wheel_service, command->parameters[1] == 1);
        return true;
    case 0x19:
        if (command->parameters[1] == 1) {
            shifter_display_request_refresh(&shifter_display);
        } else if (command->parameters[1] == 2) {
            h_pattern_calibration_service_request(
                &h_pattern_calibration_service, H_PATTERN_CALIBRATION_COMMAND_ADVANCE,
                wheel_service_mode(&wheel_service), platform_time_ms());
        }
        return true;
    case 0x1a:
        force_feedback_state_deactivate_host_effects(&force_feedback_state);
        (void)motor_output_transport_enqueue_host_effect_clears(&motor_output_transport);
        reset_host_force_feedback_outputs();
        return true;
    case 0x50:
        if (fallback_interface_mode_count <= 4) {
            if (wheel_service_activate_interface_presentation(&wheel_service,
                                                              command->parameters[1])) {
                tuning_interaction_init(&tuning_interaction);
            }
            fallback_interface_mode_count++;
        }
        if (fallback_interface_mode_count == 4) {
            tuning_interaction_request_close(&tuning_interaction);
            fallback_interface_mode_count = 0;
        }
        return true;
    default:
        return false;
    }
}

/**
 * @brief Applies routed pedal and local auxiliary calibration actions.
 *
 * Queues attached-pedal control or input reports and translates the independent local action into
 * endpoint capture or reset requests.
 *
 * @param[in] actions Routed calibration actions to apply.
 */
static void apply_pedal_calibration_actions(const PedalCalibrationActions *actions) {
    if (actions->pedal_control != 0) {
        pedal_service_request_control(&pedal_service, actions->pedal_control);
    }
    if (actions->pedal_input_pending) {
        pedal_service_request_input_command(&pedal_service, actions->pedal_input);
    }
    switch (actions->auxiliary_action) {
    case PEDAL_AUXILIARY_CALIBRATION_MINIMUM:
        auxiliary_axis_request_adjustment(&auxiliary_axis, AUXILIARY_AXIS_ADJUST_MINIMUM);
        break;
    case PEDAL_AUXILIARY_CALIBRATION_MAXIMUM:
        auxiliary_axis_request_adjustment(&auxiliary_axis, AUXILIARY_AXIS_ADJUST_MAXIMUM);
        break;
    case PEDAL_AUXILIARY_CALIBRATION_RESET:
        auxiliary_axis_reset(&auxiliary_axis);
        break;
    case PEDAL_AUXILIARY_CALIBRATION_NONE:
        break;
    }
}

/**
 * @brief Applies an active-profile wheel percentage command.
 *
 * Updates the selected profile or resets all profile values, then schedules settings persistence
 * when the effective configuration changes.
 *
 * @param[in] command Decoded percentage update or reset request.
 */
static void apply_wheel_steering_limit_command(const WheelSteeringLimitCommand *command) {
    if (wheel_steering_limits_apply(&base_settings.steering_limits,
                                    base_settings.tuning_profiles.active_slot,
                                    command) == WHEEL_STEERING_LIMIT_CHANGED) {
        base_settings_persistence_mark_dirty(&settings_persistence);
        save_base_settings();
    }
}

/**
 * @brief Routes one Xbox GIP application command.
 *
 * Decodes group-zero command packet 0A, applies transient steering-range, force-level, and
 * attached-wheel capability controls, and schedules capability, attached-device status,
 * transfer-status, or script responses. Earlier pending responses are retained until they reach
 * the endpoint.
 *
 * @param[in] report Complete USB output report containing the GIP packet.
 * @return True when the report belongs to the Xbox application-command path.
 */
static bool route_xbox_gip_command(const UsbDeviceOutputReport *report) {
    if (usb_device_operating_mode() != USB_OPERATING_MODE_XBOX_GIP ||
        !usb_xbox_gip_command_decode(report->data, report->length, &usb_xbox_gip_command)) {
        return false;
    }

    switch (usb_xbox_gip_command.kind) {
    case USB_XBOX_GIP_COMMAND_CAPABILITIES:
        if (usb_xbox_control_response_pending == USB_XBOX_CONTROL_RESPONSE_NONE) {
            usb_xbox_control_response_pending = USB_XBOX_CONTROL_RESPONSE_CAPABILITIES;
        }
        break;
    case USB_XBOX_GIP_COMMAND_STEERING_RANGE:
        xbox_steering_range_degrees =
            usb_xbox_gip_steering_range_normalize(usb_xbox_gip_command.parameter);
        break;
    case USB_XBOX_GIP_COMMAND_FORCE_FEEDBACK_STRENGTH:
        xbox_force_feedback_percent =
            usb_xbox_gip_force_feedback_strength_normalize((uint8_t)usb_xbox_gip_command.parameter);
        break;
    case USB_XBOX_GIP_COMMAND_HOST_CAPABILITY:
        if (usb_xbox_gip_command.parameter <= 1) {
            wheel_service_set_host_capability(&wheel_service, usb_xbox_gip_command.parameter != 0);
        }
        break;
    case USB_XBOX_GIP_COMMAND_TRANSFER_STATUS:
        if (usb_xbox_control_response_pending == USB_XBOX_CONTROL_RESPONSE_NONE) {
            usb_xbox_control_request[0] = report->data[0];
            usb_xbox_control_request[1] = report->data[1];
            usb_xbox_control_response_pending = USB_XBOX_CONTROL_RESPONSE_TRANSFER_STATUS;
        }
        break;
    case USB_XBOX_GIP_COMMAND_SCRIPT_SAMPLES:
        if (force_feedback_script_report_pending == FORCE_FEEDBACK_SCRIPT_REPORT_NONE) {
            force_feedback_script_sample_report_index = usb_xbox_gip_command.parameter;
            force_feedback_script_report_pending = FORCE_FEEDBACK_SCRIPT_REPORT_SAMPLES;
        }
        break;
    case USB_XBOX_GIP_COMMAND_SCRIPT_SLOT:
        if (force_feedback_script_report_pending == FORCE_FEEDBACK_SCRIPT_REPORT_NONE) {
            force_feedback_script_slot_report_index = (uint8_t)usb_xbox_gip_command.parameter;
            force_feedback_script_report_pending = FORCE_FEEDBACK_SCRIPT_REPORT_SLOT;
        }
        break;
    case USB_XBOX_GIP_COMMAND_SCRIPT_STATUS:
        if (force_feedback_script_report_pending == FORCE_FEEDBACK_SCRIPT_REPORT_NONE) {
            force_feedback_script_report_pending = FORCE_FEEDBACK_SCRIPT_REPORT_STATUS;
        }
        break;
    case USB_XBOX_GIP_COMMAND_SCRIPT_VALUES:
        if (force_feedback_script_report_pending == FORCE_FEEDBACK_SCRIPT_REPORT_NONE) {
            force_feedback_script_report_pending = FORCE_FEEDBACK_SCRIPT_REPORT_VALUES;
        }
        break;
    case USB_XBOX_GIP_COMMAND_SCRIPT_AXES:
        if (force_feedback_script_report_pending == FORCE_FEEDBACK_SCRIPT_REPORT_NONE) {
            force_feedback_script_report_pending = FORCE_FEEDBACK_SCRIPT_REPORT_AXES;
        }
        break;
    case USB_XBOX_GIP_COMMAND_EXTENDED_STATUS:
        if (usb_xbox_control_response_pending == USB_XBOX_CONTROL_RESPONSE_NONE) {
            usb_xbox_control_response_pending = USB_XBOX_CONTROL_RESPONSE_EXTENDED_STATUS;
        }
        break;
    }
    return true;
}

/**
 * @brief Routes one pending host report to its owning device subsystem.
 *
 * Gives complete script-system and motor-mailbox reports their dedicated packet paths before
 * decoding shared short commands and native vendor transfers. Each accepted report is consumed by
 * the first matching subsystem.
 */
static void service_usb_output(void) {
    if (!usb_device_take_output(&usb_device_output_report)) {
        return;
    }
    if (accept_usb_motor_report(&usb_device_output_report)) {
        return;
    }
    if (route_xbox_gip_command(&usb_device_output_report)) {
        return;
    }
    if (usb_device_operating_mode() == USB_OPERATING_MODE_PLAYSTATION &&
        usb_playstation_wheel_value_apply(&usb_playstation_wheel_value, &usb_device_output_report,
                                          platform_time_ms())) {
        wheel_service_set_legacy_axes(
            &wheel_service, usb_playstation_wheel_value_axes(&usb_playstation_wheel_value));
        return;
    }
    if (force_feedback_script_runtime_apply_packet(&force_feedback_script_system,
                                                   usb_device_output_report.data,
                                                   usb_device_output_report.length)) {
        return;
    }
    bool xbox_vendor_tunnel =
        usb_device_operating_mode() == USB_OPERATING_MODE_XBOX_GIP &&
        usb_xbox_gip_vendor_tunnel_decode(usb_device_output_report.data,
                                          usb_device_output_report.length, &usb_output_command);
    if (!xbox_vendor_tunnel &&
        !usb_output_command_decode(&usb_device_output_report, &usb_output_command)) {
        return;
    }

    if (force_feedback_command_decode(&usb_output_command, &force_feedback_command)) {
        if (force_feedback_state_apply(
                &force_feedback_state, &force_feedback_command,
                (int32_t)wheel_position_travel_from_degrees(tuning_profile->rotation_degrees))) {
            forward_force_feedback_command(&force_feedback_command, usb_output_command.payload);
            if (force_feedback_command.kind == FORCE_FEEDBACK_COMMAND_RESET_EFFECTS) {
                reset_host_force_feedback_outputs();
            }
        }
        return;
    }

    if (usb_fallback_command_decode(&usb_output_command, &usb_fallback_command)) {
        apply_fallback_command(&usb_fallback_command);
        return;
    }

    WheelCenterCaptureAction center_capture_action = wheel_center_capture_command_apply(
        &wheel_center_capture_command, &usb_output_command, platform_time_ms());
    if (center_capture_action != WHEEL_CENTER_CAPTURE_UNHANDLED) {
        if (center_capture_action == WHEEL_CENTER_CAPTURE_REQUESTED) {
            uint32_t now_ms = platform_time_ms();
            if (capture_current_wheel_center()) {
                system_control_state_set_status(&system_control_state,
                                                wheel_service_mode(&wheel_service),
                                                WHEEL_CENTER_CALIBRATED_STATUS_CODE);
                if (wheel_center_capture_command_notification_due(&wheel_center_capture_command,
                                                                  now_ms)) {
                    (void)system_event_queue_try_push(&system_event_queue,
                                                      WHEEL_CENTER_CALIBRATED_EVENT_CODE);
                }
            }
        }
        return;
    }

    if (motor_calibration_command_decode(&usb_output_command, &motor_calibration_operation)) {
        motor_calibration_service_request(&motor_calibration_service, motor_calibration_operation);
        return;
    }

    if (usb_operating_mode_command_decode(&usb_output_command, &usb_operating_mode_command)) {
        if (apply_fallback_device_command(&usb_operating_mode_command)) {
            return;
        }
        if (usb_operating_mode_command_requests_native_reset(&usb_operating_mode_command)) {
            bool leaving_playstation =
                usb_device_operating_mode() == USB_OPERATING_MODE_PLAYSTATION;
            if (leaving_playstation) {
                platform_aux_bus_init();
            }
            if (usb_device_set_input_mode(USB_INPUT_REPORT_MODE_FANATEC) && leaving_playstation) {
                initialize_usb_command_bridge();
            }
        } else if (usb_operating_mode_command_decode_status(&usb_operating_mode_command,
                                                            &usb_operating_status_enabled)) {
            system_control_state_set_operating_status(&system_control_state,
                                                      wheel_service_mode(&wheel_service),
                                                      usb_operating_status_enabled);
        } else if (start_runtime_bridge(&usb_operating_mode_command)) {
            return;
        } else if (usb_operating_mode_command_requests_led_pattern(&usb_operating_mode_command)) {
            platform_led_pattern_set_duty(
                led_pattern_pwm_duty(usb_operating_mode_command.parameters[1]));
        } else if (h_pattern_calibration_command_decode(&usb_operating_mode_command,
                                                        &h_pattern_calibration_command)) {
            h_pattern_calibration_service_request(
                &h_pattern_calibration_service, h_pattern_calibration_command,
                wheel_service_mode(&wheel_service), platform_time_ms());
        } else if (pedal_calibration_command_decode(&usb_operating_mode_command,
                                                    &pedal_calibration_command)) {
            pedal_calibration_actions = pedal_calibration_command_route(
                &pedal_calibration_command, pedal_service_calibration_active(&pedal_service),
                auxiliary_axis.active);
            apply_pedal_calibration_actions(&pedal_calibration_actions);
        } else if (pedal_protocol_command_decode(&usb_operating_mode_command,
                                                 &pedal_protocol_command)) {
            pedal_service_apply_protocol_command(&pedal_service, &pedal_protocol_command);
        } else if (wheel_service_apply_auxiliary_output_command(&wheel_service,
                                                                &usb_operating_mode_command)) {
            if (usb_operating_mode_command.opcode == WHEEL_AUXILIARY_OPTION_OPCODE &&
                base_settings.wheel_auxiliary_option != wheel_service.auxiliary_output.option) {
                base_settings.wheel_auxiliary_option = wheel_service.auxiliary_output.option;
                base_settings_persistence_mark_dirty(&settings_persistence);
                save_base_settings();
            }
            return;
        } else if (wheel_service_apply_packed_report_command(&wheel_service,
                                                             &usb_operating_mode_command)) {
            return;
        } else if (wheel_service_apply_report_six_command(&wheel_service,
                                                          &usb_operating_mode_command)) {
            return;
        } else if (wheel_service_apply_interface_mode_command(&wheel_service,
                                                              &usb_operating_mode_command)) {
            return;
        } else if (wheel_service_apply_multi_position_command(&wheel_service,
                                                              &usb_operating_mode_command)) {
            return;
        } else if (wheel_steering_limit_command_decode(&usb_operating_mode_command,
                                                       &wheel_steering_limit_command)) {
            apply_wheel_steering_limit_command(&wheel_steering_limit_command);
        }
        return;
    }

    if (usb_vendor_command_decode(&usb_output_command, &usb_vendor_command)) {
        if (usb_vendor_command.kind == USB_VENDOR_COMMAND_SCRIPT_AXES) {
            force_feedback_script_report_pending = FORCE_FEEDBACK_SCRIPT_REPORT_AXES;
            return;
        }
        if (usb_vendor_command_script_sample_index(&usb_vendor_command,
                                                   &force_feedback_script_sample_report_index)) {
            force_feedback_script_report_pending = FORCE_FEEDBACK_SCRIPT_REPORT_SAMPLES;
            return;
        }
        if (usb_vendor_command_script_slot_index(&usb_vendor_command,
                                                 &force_feedback_script_slot_report_index)) {
            force_feedback_script_report_pending = FORCE_FEEDBACK_SCRIPT_REPORT_SLOT;
            return;
        }
        if (usb_vendor_command.kind == USB_VENDOR_COMMAND_SCRIPT_STATUS) {
            force_feedback_script_report_pending = FORCE_FEEDBACK_SCRIPT_REPORT_STATUS;
            return;
        }
        if (usb_vendor_command.kind == USB_VENDOR_COMMAND_SCRIPT_VALUES) {
            force_feedback_script_report_pending = FORCE_FEEDBACK_SCRIPT_REPORT_VALUES;
            return;
        }
        if (usb_vendor_command.kind == USB_VENDOR_COMMAND_WHEEL_OUTPUT_REPORT) {
            wheel_service_apply_output_report(&wheel_service, usb_vendor_command.arguments);
            return;
        }
        const uint8_t *wheel_report_seventeen =
            usb_vendor_command_decode_wheel_report_seventeen(&usb_vendor_command);
        if (wheel_report_seventeen != 0) {
            wheel_service_queue_report_seventeen(&wheel_service, wheel_report_seventeen);
            return;
        }
        if (usb_transfer_request_apply(&usb_transfer_request, &usb_vendor_command)) {
            const UsbTransferRequestPayload *request =
                usb_transfer_request_payload(&usb_transfer_request);
            if (request != NULL) {
                pedal_service_queue_host_transfer(&pedal_service, request->data, request->length);
                usb_transfer_request_release(&usb_transfer_request);
            }
            return;
        }
        if (usb_tuning_menu_service_apply(&usb_tuning_menu_service, &usb_vendor_command)) {
            if (usb_tuning_menu_service_response_pending(&usb_tuning_menu_service) &&
                usb_vendor_response_kind == USB_VENDOR_RESPONSE_TUNING_MENU) {
                usb_vendor_response_kind = USB_VENDOR_RESPONSE_NONE;
            }
            return;
        }
        if (usb_diagnostic_report_apply_command(&usb_diagnostic_report_service,
                                                &usb_vendor_command)) {
            if (usb_vendor_response_kind == USB_VENDOR_RESPONSE_DIAGNOSTIC) {
                usb_vendor_response_kind = USB_VENDOR_RESPONSE_NONE;
            }
            return;
        }
        if (usb_tuning_status_report_apply_command(&usb_tuning_status_report_service,
                                                   &usb_vendor_command)) {
            if (usb_vendor_response_kind == USB_VENDOR_RESPONSE_TUNING_STATUS) {
                usb_vendor_response_kind = USB_VENDOR_RESPONSE_NONE;
            }
            return;
        }
        uint32_t now_ms = platform_time_ms();
        if (usb_remote_tuning_service_apply(&usb_remote_tuning_service, &usb_vendor_command, now_ms,
                                            wheel_service_mode(&wheel_service), true,
                                            wheel_service_adapter_connected(&wheel_service))) {
            return;
        }
        UsbTuningProfileAction tuning_action = usb_tuning_profile_service_apply(
            &usb_tuning_profile_service, &base_settings.tuning_profiles, &usb_vendor_command,
            now_ms);
        if ((tuning_action & USB_TUNING_PROFILE_ACTION_CLAIM) != 0) {
            if ((tuning_action & USB_TUNING_PROFILE_ACTION_PROFILE_CHANGED) != 0) {
                apply_active_tuning_profile();
            }
            if ((tuning_action & USB_TUNING_PROFILE_ACTION_SETTINGS_CHANGED) != 0) {
                base_settings_persistence_mark_dirty(&settings_persistence);
            }
            if ((tuning_action & USB_TUNING_PROFILE_ACTION_SAVE) != 0) {
                save_base_settings();
            }
            if ((tuning_action & USB_TUNING_PROFILE_ACTION_RESET_COMPLETED) != 0) {
                (void)system_event_queue_try_push(&system_event_queue,
                                                  TUNING_MENU_RESET_EVENT_CODE);
                system_control_state_set_active_event(&system_control_state,
                                                      TUNING_MENU_RESET_EVENT_CODE);
            }
            if ((tuning_action & USB_TUNING_PROFILE_ACTION_MODE_TOGGLED) != 0) {
                uint8_t event_code = base_settings.tuning_profiles.standard_mode_enabled
                                         ? STANDARD_TUNING_MODE_EVENT_CODE
                                         : ADVANCED_TUNING_MODE_EVENT_CODE;
                (void)system_event_queue_try_push(&system_event_queue, event_code);
                system_control_state_set_active_event(&system_control_state, event_code);
            }
            if (usb_tuning_profile_service_response_pending(&usb_tuning_profile_service) &&
                usb_vendor_response_kind == USB_VENDOR_RESPONSE_TUNING_PROFILE) {
                usb_vendor_response_kind = USB_VENDOR_RESPONSE_NONE;
            }
            return;
        }
        if (usb_vendor_command_requests_motor_command(&usb_vendor_command)) {
            motor_command_request_pending = true;
        } else if (usb_vendor_command_decode_wheel_transfer(&usb_vendor_command,
                                                            &usb_wheel_transfer_command)) {
            if (usb_wheel_transfer_command.action == USB_WHEEL_TRANSFER_START) {
                (void)wheel_transfer_service_start(&wheel_transfer_service,
                                                   usb_wheel_transfer_command.request);
            }
            usb_wheel_transfer_response_pending[usb_wheel_transfer_command.request] = true;
        }
    }
}

/**
 * @brief Applies a security-code presentation to attached-wheel output.
 *
 * Publishes prompt or digit glyphs through the interaction override, applies selected-digit report
 * masks, and restores lower-priority display state when entry completes.
 *
 */
static void apply_security_code_presentation(void) {
    if (security_code_update_state.presentation.kind == SECURITY_CODE_PRESENTATION_KEEP) {
        return;
    }
    if (security_code_update_state.presentation.kind == SECURITY_CODE_PRESENTATION_CLEAR) {
        wheel_service_set_auxiliary_report(&wheel_service, 0);
        wheel_service_clear_display_override(&wheel_service);
        return;
    }

    security_code_display_output = *wheel_service_default_display_output(&wheel_service);
    for (uint8_t digit = 0; digit < SECURITY_CODE_DIGIT_COUNT; digit++) {
        security_code_display_output.glyphs[digit] =
            security_code_update_state.presentation.glyphs[digit];
    }
    security_code_display_output.third_glyph_marker = false;
    wheel_service_set_display_override(&wheel_service, &security_code_display_output);
    if (security_code_update_state.presentation.report != 0) {
        wheel_service_set_auxiliary_report(&wheel_service,
                                           security_code_update_state.presentation.report);
    }
}

/**
 * @brief Publishes a tuning interaction event to local and attached-wheel presentation.
 *
 * Queues the event for local display and makes it the active system-control event sent to the
 * attached wheel.
 *
 * @param[in] event_code Tuning reset or profile-mode event code.
 */
static void publish_tuning_interaction_event(uint8_t event_code) {
    (void)system_event_queue_try_push(&system_event_queue, event_code);
    system_control_state_set_active_event(&system_control_state, event_code);
}

/**
 * @brief Applies one action emitted by the wheel-side tuning interaction.
 *
 * Routes independent center, pedal, shifter-display, profile-mode, and profile-reset effects.
 * Profile changes are applied to runtime behavior and retained for persistence.
 *
 * @param[in] action Interaction action to apply.
 */
static void apply_tuning_interaction_action(TuningInteractionAction action) {
    if ((action & TUNING_INTERACTION_ACTION_PEDAL_ADJUSTMENT) != 0) {
        if (pedal_service_adjustment_available(&pedal_service)) {
            pedal_service_request_button_adjustment(&pedal_service);
        }
    }
    if ((action & TUNING_INTERACTION_ACTION_SHOW_SHIFTER) != 0 ||
        (action & TUNING_INTERACTION_ACTION_SHOW_EXTENDED_SHIFTER) != 0) {
        shifter_display_request_refresh(&shifter_display);
    }
    if ((action & TUNING_INTERACTION_ACTION_SHOW_CENTER_CAPTURE) != 0) {
        (void)wheel_service_queue_tuning_display_command(&wheel_service,
                                                         WHEEL_CENTER_CALIBRATED_STATUS_CODE);
    }
    if ((action & TUNING_INTERACTION_ACTION_CAPTURE_CENTER) != 0 &&
        capture_current_wheel_center()) {
        system_control_state_set_status(&system_control_state, wheel_service_mode(&wheel_service),
                                        WHEEL_CENTER_CALIBRATED_STATUS_CODE);
        publish_tuning_interaction_event(WHEEL_CENTER_CALIBRATED_EVENT_CODE);
    }
    if ((action & TUNING_INTERACTION_ACTION_PEDAL_UP) != 0) {
        pedal_service_request_control(&pedal_service, PEDAL_V3_CONTROL_UP);
    }
    if ((action & TUNING_INTERACTION_ACTION_PEDAL_DOWN) != 0) {
        pedal_service_request_control(&pedal_service, PEDAL_V3_CONTROL_DOWN);
    }
    if ((action & TUNING_INTERACTION_ACTION_PEDAL_AUTOMATIC) != 0) {
        pedal_service_request_control(&pedal_service, PEDAL_V3_CONTROL_AUTOMATIC);
    }
    if ((action & (TUNING_INTERACTION_ACTION_PEDAL_UP_COMPLETE |
                   TUNING_INTERACTION_ACTION_PEDAL_DOWN_COMPLETE |
                   TUNING_INTERACTION_ACTION_PEDAL_AUTOMATIC_COMPLETE)) != 0) {
        local_display_tuning_revision++;
    }
    if ((action & TUNING_INTERACTION_ACTION_TOGGLE_PROFILE_MODE) != 0) {
        bool enable_standard = !base_settings.tuning_profiles.standard_mode_enabled;
        if (tuning_profile_bank_set_standard_mode(&base_settings.tuning_profiles,
                                                  enable_standard)) {
            if (enable_standard) {
                apply_active_tuning_profile();
            }
            base_settings_persistence_mark_dirty(&settings_persistence);
            publish_tuning_interaction_event(enable_standard ? STANDARD_TUNING_MODE_EVENT_CODE
                                                             : ADVANCED_TUNING_MODE_EVENT_CODE);
        }
    }
    if ((action & TUNING_INTERACTION_ACTION_RESET_PROFILES) != 0) {
        tuning_profile_bank_defaults(&base_settings.tuning_profiles);
        apply_active_tuning_profile();
        base_settings_persistence_mark_dirty(&settings_persistence);
        publish_tuning_interaction_event(TUNING_MENU_RESET_EVENT_CODE);
    }
}

/**
 * @brief Reports whether a wheel mode exposes vibration-strength tuning.
 *
 * Selects the five attached-wheel modes that route a local vibration setting to the wheel.
 *
 * @param[in] wheel_mode Active attached-wheel mode.
 * @return True when vibration-strength tuning is available.
 */
static bool wheel_mode_supports_vibration_tuning(uint8_t wheel_mode) {
    switch (wheel_mode) {
    case 0x01:
    case 0x02:
    case 0x03:
    case 0x0a:
    case 0x16:
        return true;
    default:
        return false;
    }
}

/**
 * @brief Classifies the attached pedal path for local tuning controls.
 *
 * Retains legacy brake-force control during legacy calibration and exposes full pedal tuning only
 * while a digital pedal path and the attached-wheel protocol are both active.
 *
 * @param[in] state Current V3 calibration state.
 * @param[in] pedal_interface_active True when a digital pedal path is receiving input.
 * @param[in] wheel_protocol_active True when the attached-wheel protocol session is active.
 * @return Pedal capability level used by tuning entry availability.
 */
static TuningPedalConnection local_tuning_pedal_connection(const PedalV3State *state,
                                                           bool pedal_interface_active,
                                                           bool wheel_protocol_active) {
    if (!pedal_interface_active || state->primary_calibration) {
        return TUNING_PEDALS_UNAVAILABLE;
    }
    if (state->legacy_calibration) {
        return TUNING_PEDALS_LEGACY;
    }
    if (state->secondary_calibration || !wheel_protocol_active) {
        return TUNING_PEDALS_UNAVAILABLE;
    }
    return TUNING_PEDALS_TRANSFER;
}

/**
 * @brief Builds current attached-device capabilities for local tuning navigation.
 *
 * Combines the host interface, wheel identity and input features, wheel protocol state, and pedal
 * calibration state into the hardware gates used by the local tuning catalog.
 *
 * Updates the retained context consumed by the local tuning-menu controller.
 */
static void update_local_tuning_availability(void) {
    uint8_t wheel_mode = wheel_service_mode(&wheel_service);
    const PedalV3State *pedal_state = pedal_service_v3_state(&pedal_service);
    const WheelAccessory *accessory = wheel_accessory_service_identity(&wheel_accessory_service);
    bool pedal_interface_active =
        pedal_service.connected && pedal_service.phase != PEDAL_SERVICE_ANALOG;
    bool wheel_protocol_active =
        wheel_service_protocol_phase(&wheel_service) == WHEEL_PROTOCOL_ACTIVE;
    tuning_availability = (TuningEntryAvailabilityContext){
        .interface_mode = (uint8_t)usb_device_operating_mode(),
        .wheel_mode = wheel_mode,
        .wheel_accessory_kind = accessory != NULL ? accessory->kind : WHEEL_ACCESSORY_DISCONNECTED,
        .pedal_connection = local_tuning_pedal_connection(pedal_state, pedal_interface_active,
                                                          wheel_protocol_active),
        .legacy_pedal_mode = pedal_state->legacy_calibration,
        .motor_calibration_active = motor_calibration_service_pending(&motor_calibration_service),
        .primary_pedal_calibration_active = pedal_state->primary_calibration,
        .secondary_pedal_calibration_active = pedal_state->secondary_calibration,
        .multi_position_supported = wheel_service_multi_position_supported(&wheel_service),
        .wheel_axis_report_enabled = wheel_service_axis_report_enabled(&wheel_service),
        .vibration_mode_compatible = wheel_mode_supports_vibration_tuning(wheel_mode),
    };
}

/**
 * @brief Builds current restrictions and dynamic limits for local tuning adjustment.
 *
 * Protects the automatic setup, selects the pedal controller's active brake-force increment, and
 * permits automatic multi-position mode only for its supporting wheel mode.
 *
 * Updates the retained context consumed by the local tuning-menu controller.
 */
static void update_local_tuning_adjustment(void) {
    const PedalV3State *pedal_state = pedal_service_v3_state(&pedal_service);
    if (wheel_service_mode(&wheel_service) == 0x09) {
        TuningProfile *selected =
            &base_settings.tuning_profiles.slots[base_settings.tuning_profiles.selected_slot];
        if (selected->multi_position_mode == TUNING_MULTI_POSITION_ENCODER) {
            selected->multi_position_mode = TUNING_MULTI_POSITION_PULSE;
            if (base_settings.tuning_profiles.selected_slot ==
                base_settings.tuning_profiles.active_slot) {
                apply_active_tuning_profile();
            }
            base_settings_persistence_mark_dirty(&settings_persistence);
        }
    }
    tuning_adjustment = (TuningEntryAdjustmentContext){
        .security_code_active = security_code_update_state.active,
        .automatic_setup_selected = base_settings.tuning_profiles.selected_slot == 0,
        .alternate_brake_fine_step =
            (pedal_state->primary_calibration && !pedal_state->legacy_calibration) ||
            pedal_state->secondary_calibration,
        .multi_position_automatic_available = wheel_service_mode(&wheel_service) != 0x09,
        .xbox_mode = usb_device_operating_mode() == USB_OPERATING_MODE_XBOX_GIP,
    };
}

/**
 * @brief Applies the current local tuning page to attached-wheel output.
 *
 * Renders the selected entry over the default wheel page while retaining its auxiliary output and
 * restores lower-priority output when the local menu closes.
 *
 */
static void apply_tuning_menu_presentation(void) {
    tuning_display_output = *wheel_service_default_display_output(&wheel_service);
    if (!tuning_display_render(&tuning_menu, &base_settings.tuning_profiles,
                               &tuning_display_output)) {
        wheel_service_clear_display_override(&wheel_service);
        return;
    }
    wheel_service_set_display_override(&wheel_service, &tuning_display_output);
}

/**
 * @brief Services the security gate and wheel-side tuning interaction.
 *
 * Feeds the security-code state machine from direct or adapter controls and gives active code entry
 * priority over tuning interaction. Otherwise it advances local navigation, entry selection and
 * adjustment, pedal adjustment, profile-mode selection, and profile-reset holds. Changed profile
 * values are applied to active consumers and scheduled for persistence.
 *
 * @param[in] now_ms Current monotonic time in milliseconds.
 */
static void service_tuning_interaction(uint32_t now_ms) {
    const uint8_t *buttons = wheel_service_buttons(&wheel_service);
    const WheelAdapterInput *adapter = wheel_service_adapter(&wheel_service);
    bool available = wheel_service_input_snapshot(&wheel_service, &tuning_interaction_snapshot);
    if (available) {
        (void)usb_remote_tuning_service_update_legacy_encoder(
            &usb_remote_tuning_service, wheel_service_mode(&wheel_service),
            tuning_interaction_snapshot.packed_rotary_positions >> 4);
    }
    security_code_input = (SecurityCodeInput){
        .wheel_mode = wheel_service_mode(&wheel_service),
        .primary_buttons = (uint16_t)buttons[0] | (uint16_t)buttons[1] << 8,
        .secondary_buttons = available ? tuning_interaction_snapshot.secondary_buttons : 0,
        .adapter_buttons = {adapter->buttons[0], adapter->buttons[1], adapter->buttons[2]},
        .adapter_connected = adapter->connected,
    };
    security_code_update_state = security_code_update(&security_code, &base_settings.security_code,
                                                      &security_code_input, now_ms);
    if (security_code_update_state.prompt_command != 0) {
        wheel_service_queue_adapter_display_state(&wheel_service,
                                                  security_code_update_state.prompt_command);
        (void)wheel_service_queue_tuning_display_command(&wheel_service,
                                                         security_code_update_state.prompt_command);
    }
    apply_security_code_presentation();
    if (security_code_update_state.presentation.kind == SECURITY_CODE_PRESENTATION_PROMPT) {
        tuning_interaction_init(&tuning_interaction);
    }
    if (security_code_update_state.settings_changed) {
        base_settings_persistence_mark_dirty(&settings_persistence);
        save_base_settings();
    }
    if (security_code_update_state.active) {
        return;
    }

    tuning_interaction_input = (TuningInteractionInput){
        .wheel_mode = wheel_service_mode(&wheel_service),
        .primary_buttons = (uint16_t)buttons[0] | (uint16_t)buttons[1] << 8,
        .secondary_buttons = available ? tuning_interaction_snapshot.secondary_buttons : 0,
        .analog_scale = available ? tuning_interaction_snapshot.tuning_input : 0,
        .auxiliary_report = {available ? tuning_interaction_snapshot.auxiliary_report[0] : 0,
                             available ? tuning_interaction_snapshot.auxiliary_report[1] : 0,
                             available ? tuning_interaction_snapshot.auxiliary_report[2] : 0},
        .adapter_buttons = {adapter->buttons[0], adapter->buttons[1], adapter->buttons[2]},
        .adapter_mode = adapter->mode,
        .adapter_profile_shortcut =
            adapter->connected && adapter->mode == 1 && (adapter->buttons[1] & 1u) != 0,
        .adapter_connected = adapter->connected,
        .profile_selector_active =
            available && (tuning_interaction_snapshot.auxiliary_report[1] & 0x0fu) != 0,
        .entry_showing_label = tuning_menu.view == TUNING_MENU_VIEW_LABEL,
        .legacy_pedal_calibration_available =
            pedal_service.connected && pedal_service_v3_state(&pedal_service)->legacy_calibration,
        .pedal_operation_pending = pedal_service_control_pending(&pedal_service),
        .available = available,
    };
    TuningInteractionPhase previous_phase = tuning_interaction.phase;
    TuningInteractionAction action =
        tuning_interaction_update(&tuning_interaction, &tuning_interaction_input, now_ms);
    apply_tuning_interaction_action(action);
    bool profile_mode = tuning_interaction.phase == TUNING_INTERACTION_MENU_HELD;
    if (available) {
        (void)usb_remote_tuning_service_update_physical_selection(
            &usb_remote_tuning_service, wheel_service_mode(&wheel_service), profile_mode,
            wheel_service_tuning_display_supported(&wheel_service), adapter->connected,
            tuning_interaction_snapshot.tuning_input,
            tuning_interaction_snapshot.auxiliary_report[1]);
        (void)usb_remote_tuning_service_update_setup_navigation(
            &usb_remote_tuning_service, wheel_service_mode(&wheel_service), profile_mode,
            tuning_interaction_snapshot.motion);
    }
    if (tuning_interaction.phase == TUNING_INTERACTION_CLOSING &&
        previous_phase != TUNING_INTERACTION_CLOSING) {
        pedal_service_request_configuration(&pedal_service, tuning_profile->alternate_brake_force,
                                            true);
        save_base_settings();
    }
    if (tuning_interaction.phase != previous_phase) {
        local_display_tuning_revision++;
    }
    TuningNavigationEvent navigation = tuning_interaction_take_navigation(&tuning_interaction);
    update_local_tuning_availability();
    update_local_tuning_adjustment();
    TuningMenu previous_menu = tuning_menu;
    TuningMenuUpdate menu_update = tuning_menu_update(&tuning_menu, tuning_interaction.phase,
                                                      navigation, &base_settings.tuning_profiles,
                                                      &tuning_availability, &tuning_adjustment);
    if (menu_update.entry_changed || menu_update.value_changed ||
        tuning_menu.view != previous_menu.view) {
        local_display_tuning_revision++;
    }
    if (menu_update.value_changed) {
        if (base_settings.tuning_profiles.selected_slot ==
            base_settings.tuning_profiles.active_slot) {
            apply_active_tuning_profile();
        }
        base_settings_persistence_mark_dirty(&settings_persistence);
    }
    apply_tuning_menu_presentation();
}

/**
 * @brief Advances the attached wheel's alternative-shifter selection.
 *
 * Keeps profile-related selection pending through the tuning close phase, applies the selected
 * mode to the active profile, and persists a changed selection.
 *
 * @param[in] now_ms Current monotonic time in milliseconds.
 */
static void service_alternative_shifter(uint32_t now_ms) {
    bool profile_context_pending = tuning_interaction.phase == TUNING_INTERACTION_ENTRY_OPEN ||
                                   tuning_interaction.phase == TUNING_INTERACTION_CLOSING;
    WheelAlternativeShifterEvent event =
        wheel_service_update_alternative_shifter(&wheel_service, profile_context_pending, now_ms);
    if (event == WHEEL_ALTERNATIVE_SHIFTER_ENABLED) {
        (void)system_event_queue_try_push(&system_event_queue,
                                          ALTERNATIVE_SHIFTER_ENABLED_EVENT_CODE);
    } else if (event == WHEEL_ALTERNATIVE_SHIFTER_DISABLED) {
        (void)system_event_queue_try_push(&system_event_queue,
                                          ALTERNATIVE_SHIFTER_DISABLED_EVENT_CODE);
    }
}

/**
 * @brief Builds and submits the current USB input report.
 *
 * Combines calibrated motor position, attached-wheel controls and rotary selectors, shifter
 * state, thermal limit state, pedal axes, and pending bite-point updates into the active native,
 * Xbox, or PlayStation USB input format.
 *
 * @param[in] now_ms Current monotonic time in milliseconds.
 */
static void service_usb_input(uint32_t now_ms) {
    if (!motor_position_ready) {
        return;
    }

    bool tuning_host_suppressed = tuning_interaction_suppresses_host_input(&tuning_interaction);
    bool tuning_system_button_suppressed =
        tuning_interaction_suppresses_system_button(&tuning_interaction);
    if (tuning_host_suppressed) {
        wheel_service_discard_host_motion(&wheel_service);
    }

    bool xbox_mode = usb_device_operating_mode() == USB_OPERATING_MODE_XBOX_GIP;
    const WheelPositionCalibration *input_calibration = &wheel_position_calibration;
    if (xbox_mode) {
        synchronize_xbox_controls_with_profile();
        xbox_position_calibration = wheel_position_calibration_build(
            &base_settings.wheel_position, xbox_steering_range_degrees,
            tuning_profile->steering_deadzone);
        input_calibration = &xbox_position_calibration;
    }

    const MotorIdentity *motor_identity = motor_probe_identity(&motor_probe);
    usb_input_state = (UsbInputReportState){
        .fanatec =
            {
                .steering = wheel_position_hid_axis(motor_position_report.wheel_position,
                                                    input_calibration),
                .transfer_code = motor_identity_input_transfer_code(motor_identity),
                .wheel_mode = FANATEC_INPUT_DIRECT_DRIVE_MODE,
                .axis_limit = wheel_service_axis_limit(&wheel_service),
            },
    };
    const uint8_t *clutch_paddles = wheel_service_clutch_paddles(&wheel_service);
    if (clutch_paddles != 0) {
        usb_input_state.fanatec.clutch_paddles[0] = clutch_paddles[0];
        usb_input_state.fanatec.clutch_paddles[1] = clutch_paddles[1];
    }
    uint8_t wheel_controls[8] = {0};
    if (!tuning_host_suppressed && wheel_service_controls(&wheel_service, wheel_controls)) {
        bool include_extended = wheel_service_extended_report_fields(&wheel_service);
        fanatec_input_apply_wheel_controls(&usb_input_state.fanatec, wheel_controls,
                                           include_extended);
        if (include_extended) {
            fanatec_input_apply_wheel_accessory(&usb_input_state.fanatec,
                                                wheel_service_accessory_flags(&wheel_service));
        }
    }
    uint8_t multi_position_mode =
        wheel_service_multi_position_mode(&wheel_service, tuning_profile->multi_position_mode);
    fanatec_input_apply_multi_position_mode(&usb_input_state.fanatec, multi_position_mode);
    fanatec_input_apply_alternative_shifter(
        &usb_input_state.fanatec, wheel_service_alternative_shifter_enabled(&wheel_service));
    wheel_multi_position_input = (WheelMultiPositionInput){0};
    if (!tuning_host_suppressed &&
        wheel_service_multi_position_input(&wheel_service, now_ms, &wheel_multi_position_input)) {
        fanatec_multi_position_input_state = (fanatec_multi_position_input){
            .remap_selectors = wheel_multi_position_input.remap_selectors,
        };
        for (uint8_t channel = 0; channel < FANATEC_INPUT_MULTI_POSITION_CHANNELS; channel++) {
            fanatec_multi_position_input_state.channels[channel].position =
                wheel_multi_position_input.channels[channel].position;
            fanatec_multi_position_input_state.channels[channel].event =
                (uint8_t)wheel_multi_position_input.channels[channel].event;
            fanatec_multi_position_input_state.channels[channel].active =
                wheel_multi_position_input.channels[channel].active;
        }
        fanatec_input_apply_multi_position_rotaries(&usb_input_state.fanatec, multi_position_mode,
                                                    &fanatec_multi_position_input_state);
    }
    const uint8_t *wheel_buttons = wheel_service_buttons(&wheel_service);
    for (uint8_t bank = 0; bank < WHEEL_BUTTON_BANK_COUNT; bank++) {
        usb_input_state.fanatec.button_banks[bank] = wheel_buttons[bank];
    }
    if (tuning_host_suppressed) {
        usb_input_state.fanatec.button_banks[0] &= 0xf0u;
        usb_input_state.fanatec.button_banks[2] &= 0xfdu;
        uint8_t wheel_mode = wheel_service_mode(&wheel_service);
        if (wheel_mode != 0x0fu && wheel_mode != 0x17u) {
            usb_input_state.fanatec.button_banks[2] &= 0xfbu;
        }
    }
    if (!tuning_host_suppressed &&
        fanatec_encoder_update(&fanatec_encoder, wheel_service_encoder_direction(&wheel_service),
                               now_ms, &usb_input_state.fanatec)) {
        (void)wheel_service_take_encoder_step(&wheel_service);
    }
    fanatec_input_apply_shifter(&usb_input_state.fanatec, &shifter_input, h_pattern_shifter.gear);
    fanatec_input_apply_thermal_limit(&usb_input_state.fanatec, cooling_effect_limit.active);
    fanatec_input_apply_wheel_calibration(&usb_input_state.fanatec,
                                          wheel_service_calibration_available(&wheel_service));
    fanatec_input_apply_wheel_input_capability(
        &usb_input_state.fanatec, wheel_service_input_capability_available(&wheel_service));
    if (wheel_service_take_bite_point_report(&wheel_service, &wheel_bite_point_report_percent)) {
        fanatec_input_apply_bite_point_update(&usb_input_state.fanatec,
                                              wheel_bite_point_report_percent);
    }
    const PedalInput *pedal_input = pedal_service_input(&pedal_service);
    for (uint8_t axis = 0; axis < FANATEC_INPUT_PEDAL_AXES; axis++) {
        usb_input_state.fanatec.pedals[axis] = pedal_input_hid_axis(pedal_input->axes[axis]);
    }
    usb_input_state.fanatec.auxiliary_pedal = pedal_input_hid_auxiliary(pedal_input->auxiliary);
    const PedalV3State *pedal_status = pedal_service_v3_state(&pedal_service);
    bool pedal_legacy = pedal_service_legacy_transport_active(&pedal_service);
    bool pedal_auxiliary = (pedal_legacy || pedal_status->primary_calibration ||
                            pedal_status->secondary_calibration) &&
                           (pedal_status->connection_flags & 0xaau) == 0;
    uint8_t legacy_pedal_first = 0;
    uint8_t legacy_pedal_second = 0;
    if (pedal_legacy) {
        legacy_pedal_first = pedal_status->primary_calibration ? 4 : 0x3b;
        legacy_pedal_second = pedal_status->primary_calibration ? 0x62 : 0x18;
    } else if (pedal_status->primary_calibration) {
        legacy_pedal_first = 5;
        legacy_pedal_second = 0x62;
    } else if (pedal_status->secondary_calibration) {
        legacy_pedal_first = 6;
        legacy_pedal_second = 0x62;
    }
    wheel_service_set_legacy_pedal_status(&wheel_service, legacy_pedal_first, legacy_pedal_second);
    fanatec_input_apply_pedal_status(&usb_input_state.fanatec, pedal_legacy, pedal_auxiliary,
                                     pedal_service_handshake_active(&pedal_service), false,
                                     pedal_status->primary_calibration);
    const WheelAxisOverrides *axis_overrides = wheel_service_axis_overrides(&wheel_service);
    fanatec_input_apply_wheel_axis_overrides(&usb_input_state.fanatec, axis_overrides);
    if (usb_device_operating_mode() == USB_OPERATING_MODE_PLAYSTATION) {
        UsbPlaystationInputWorkspace *workspace = &usb_console_input_workspace.playstation;
        (void)wheel_service_input_snapshot(&wheel_service, &workspace->sources.wheel);
        const WheelAdapterInput *adapter = wheel_service_adapter(&wheel_service);
        uint8_t wheel_clutch_first = workspace->sources.wheel.clutch_paddles[0];
        uint8_t wheel_clutch_second = workspace->sources.wheel.clutch_paddles[1];
        bool wheel_axis_enabled = workspace->sources.wheel.axis_report_enabled;
        bool shifter_display_active = h_pattern_calibration_service.active;
        workspace->sources.buttons = (UsbPlaystationButtonInput){
            .secondary_buttons = workspace->sources.wheel.secondary_buttons,
            .adapter_mode = adapter->mode,
            .wheel_mode = wheel_service_mode(&wheel_service),
            .directional_buttons = workspace->sources.wheel.directional_buttons,
            .adapter_buttons = {adapter->buttons[0], adapter->buttons[1], adapter->buttons[2]},
            .auxiliary_buttons = {shifter_input.primary_transition,
                                  shifter_input.secondary_transition},
            .auxiliary_history = workspace->sources.wheel.auxiliary_report[0],
            .extended_buttons = workspace->sources.wheel.auxiliary_report[2],
            .axis_modes = {(uint8_t)shifter_input.primary_mode,
                           (uint8_t)shifter_input.secondary_mode},
            .adapter_connected = adapter->connected,
            .hat_suppressed = shifter_display_active || tuning_host_suppressed,
            .system_button_suppressed = shifter_display_active || tuning_system_button_suppressed,
        };
        workspace->state = (UsbPlaystationInputState){
            .steering = usb_input_state.fanatec.steering,
            .pedals = {usb_input_state.fanatec.pedals[0], usb_input_state.fanatec.pedals[1],
                       usb_input_state.fanatec.pedals[2]},
            .wheel_hat = (uint8_t)h_pattern_shifter.gear,
            .auxiliary_axis = (uint16_t)usb_input_state.fanatec.auxiliary_pedal * 0x0101u,
        };
        (void)usb_playstation_input_map_buttons(
            &usb_playstation_input_mapper, &workspace->sources.buttons, now_ms, &workspace->state);
        workspace->sources.clutch = (UsbPlaystationClutchInput){
            .wheel_mode = wheel_service_mode(&wheel_service),
            .paddle_mode = (uint8_t)tuning_profile->paddle_mode,
            .wheel_axes = {wheel_clutch_first, wheel_clutch_second},
            .adapter_axes = {adapter->axes[0], adapter->axes[1]},
            .wheel_axis_enabled = wheel_axis_enabled,
            .adapter_connected = adapter->connected,
        };
        usb_playstation_input_map_clutch(workspace->state.clutch_axes, &workspace->sources.clutch);
        (void)usb_device_send_playstation_input(&workspace->state);
        return;
    }
    if (xbox_mode) {
        UsbXboxGipInputState *xbox_input = &usb_console_input_workspace.xbox;
        *xbox_input = (UsbXboxGipInputState){
            .mode_buttons = wheel_service_mode_buttons(&wheel_service),
            .steering = usb_input_state.fanatec.steering,
            .auxiliary_pedal = usb_input_state.fanatec.auxiliary_pedal,
            .encoder_direction = (usb_input_state.fanatec.button_banks[3] & 0x08u) != 0   ? 1
                                 : (usb_input_state.fanatec.button_banks[3] & 0x04u) != 0 ? -1
                                                                                          : 0,
            .wheel_mode = wheel_service_mode(&wheel_service),
            .axis_mode = shifter_input.primary_mode == SHIFTER_INPUT_H_PATTERN ||
                                 shifter_input.secondary_mode == SHIFTER_INPUT_H_PATTERN
                             ? 1
                         : shifter_input.primary_mode == SHIFTER_INPUT_SEQUENTIAL ||
                                 shifter_input.secondary_mode == SHIFTER_INPUT_SEQUENTIAL
                             ? 2
                             : 0,
            .led_state = (uint8_t)(base_settings.tuning_profiles.active_slot + 1u),
            .steering_range_degrees = xbox_steering_range_degrees,
            .force_feedback_percent = xbox_force_feedback_percent,
            .auxiliary_pedal_active =
                pedal_service.auxiliary_override_active || axis_overrides->auxiliary.enabled,
        };
        bool pedals_active = pedal_service.connected || pedal_service.analog.active;
        for (uint8_t button = 0; button < USB_XBOX_GIP_WHEEL_BUTTON_COUNT; button++) {
            xbox_input->buttons[button] = usb_input_state.fanatec.button_banks[button];
        }
        for (uint8_t control = 0; control < USB_XBOX_GIP_WHEEL_CONTROL_COUNT; control++) {
            xbox_input->controls[control] = wheel_controls[control];
        }
        for (uint8_t rotary = 0; rotary < USB_XBOX_GIP_ROTARY_COUNT; rotary++) {
            xbox_input->rotary[rotary] = usb_input_state.fanatec.rotary[rotary];
        }
        const WheelAxisOverride *pedal_overrides[USB_XBOX_GIP_INPUT_PEDAL_COUNT] = {
            &axis_overrides->axis_5,
            &axis_overrides->axis_6,
            &axis_overrides->axis_7,
        };
        for (uint8_t axis = 0; axis < USB_XBOX_GIP_INPUT_PEDAL_COUNT; axis++) {
            xbox_input->pedals[axis] = usb_input_state.fanatec.pedals[axis];
            xbox_input->pedal_active[axis] = pedals_active || pedal_overrides[axis]->enabled;
        }
        xbox_input->clutch_paddles[0] = usb_input_state.fanatec.clutch_paddles[0];
        xbox_input->clutch_paddles[1] = usb_input_state.fanatec.clutch_paddles[1];
        usb_xbox_gip_input_build(&xbox_input_builder, xbox_input, &xbox_input_snapshot);
        (void)usb_device_queue_xbox_input(&xbox_input_snapshot);
        return;
    }
    uint8_t report_size =
        usb_input_report_encode(usb_device_input_mode(), usb_input_report, &usb_input_state);
    if (report_size != 0) {
        usb_device_send_input(usb_input_report, report_size);
    }
}

/**
 * @brief Publishes the native USB feature-report snapshots.
 *
 * Encodes reports 31, 32, 33, and 36 from current system, tuning, wheel, pedal, shifter, and rotary
 * state. Reading report 33 consumes the queued per-axis motion pulses represented by that snapshot.
 */
static void service_usb_feature_reports(void) {
    const PedalV3State *pedal_status = pedal_service_v3_state(&pedal_service);
    bool pedal_legacy = pedal_service_legacy_transport_active(&pedal_service);
    bool pedal_io_active = (pedal_legacy || pedal_status->primary_calibration ||
                            pedal_status->secondary_calibration) &&
                           (pedal_status->connection_flags & 0xaau) == 0;
    uint8_t pedal_active =
        pedal_status->secondary_calibration
            ? 0x10
            : (uint8_t)((pedal_status->primary_calibration ? 2 : 0) | (pedal_legacy ? 1 : 0));
    const WheelAccessory *accessory = wheel_accessory_service_identity(&wheel_accessory_service);
    uint8_t rotary_mode =
        wheel_service_multi_position_mode(&wheel_service, tuning_profile->multi_position_mode);
    UsbFeatureReport31State report31 = {
        .status = system_control_state.status_code,
        .wheel_mode = wheel_service_mode(&wheel_service),
        .pedal_active = pedal_active,
        .auxiliary_profile = pedal_service.auxiliary_override_active,
        .axis_modes = {(uint8_t)shifter_input.primary_mode, (uint8_t)shifter_input.secondary_mode},
        .transfer_code = accessory != 0 ? wheel_accessory_transfer_code(accessory) : 0,
        .rotary_mode = rotary_mode,
        .pedal_legacy = pedal_legacy,
        .pedal_io_active = pedal_io_active,
        .pedal_handshake_active = pedal_service_handshake_active(&pedal_service),
        .pedal_calibration_active = pedal_status->primary_calibration,
        .wheel_calibration_available = wheel_service_calibration_available(&wheel_service),
        .wheel_axis_report_enabled = wheel_service_axis_report_enabled(&wheel_service),
        .adapter_connected = wheel_service_adapter_connected(&wheel_service),
    };
    usb_feature_report_31_encode(&report31, usb_feature_reports[0]);
    usb_feature_report_32_encode(&base_settings.tuning_profiles, settings_persistence.dirty,
                                 usb_feature_reports[1]);

    WheelInputSnapshot snapshot = {0};
    (void)wheel_service_input_snapshot(&wheel_service, &snapshot);
    UsbFeatureReport33State report33 = {
        .pulse_directions =
            {
                wheel_service_axis_motion_direction(&wheel_service, 0),
                wheel_service_axis_motion_direction(&wheel_service, 1),
                wheel_service_axis_motion_direction(&wheel_service, 2),
                wheel_service_axis_motion_direction(&wheel_service, 3),
            },
        .extended_buttons = snapshot.auxiliary_report[2],
        .auxiliary_buttons = {shifter_input.primary_transition, shifter_input.secondary_transition,
                              snapshot.auxiliary_report[0]},
        .rotary_mode = rotary_mode,
        .remap_selectors = wheel_multi_position_input.remap_selectors,
    };
    for (uint8_t channel = 0; channel < WHEEL_MULTI_POSITION_CHANNEL_COUNT; channel++) {
        report33.positions[channel] = wheel_multi_position_input.channels[channel].position;
        report33.events[channel] = (int8_t)wheel_multi_position_input.channels[channel].event;
    }
    report33.tertiary_active = wheel_multi_position_input.channels[2].active;
    usb_feature_report_33_encode(&report33, usb_feature_reports[2]);
    usb_feature_report_36_encode((uint8_t)usb_tuning_menu_service.active_page,
                                 usb_feature_reports[3]);
    static const uint8_t report_ids[4] = {0x31, 0x32, 0x33, 0x36};
    for (uint8_t index = 0; index < 4; index++) {
        (void)usb_device_publish_feature_report(report_ids[index], usb_feature_reports[index],
                                                USB_DEVICE_REPORT_SIZE);
    }
    if (usb_device_take_feature_report_request(0x33)) {
        for (uint8_t axis = 0; axis < WHEEL_MOTION_AXIS_COUNT; axis++) {
            (void)wheel_service_take_axis_motion(&wheel_service, axis);
        }
    }
}

/**
 * @brief Samples and publishes all base-side analog inputs.
 *
 * Updates cooling temperatures, pedal fallback samples, the local auxiliary override, and the
 * active H-pattern shifter. An uncalibrated H-pattern input opens a fresh capture session when the
 * attached-wheel display is ready. Queued calibration captures use the selected shifter axes and
 * request immediate persistence after seventh gear. Changed auxiliary endpoint settings enter the
 * shared delayed persistence path.
 *
 * @param[in] now_ms Current monotonic time in milliseconds.
 */
static void service_analog_input(uint32_t now_ms) {
    platform_shifter_read(&shifter_input);
    h_pattern_calibration_service_set_advance_input(
        &h_pattern_calibration_service,
        wheel_service_calibration_advance_input_active(&wheel_service));
    if (platform_adc_read(&analog_samples)) {
        cooling_temperature_monitor_add(&cooling_temperature_monitor,
                                        analog_samples.primary_thermistor,
                                        analog_samples.secondary_thermistor);
        pedal_service_set_analog_samples(&pedal_service, analog_samples.pedal_axes);
        AuxiliaryAxisCalibrationMode auxiliary_mode =
            pedal_service_auxiliary_automatic_calibration(&pedal_service)
                ? AUXILIARY_AXIS_AUTOMATIC_CALIBRATION
                : AUXILIARY_AXIS_MANUAL_CALIBRATION;
        AuxiliaryAxisReading auxiliary = auxiliary_axis_update(
            &auxiliary_axis, analog_samples.auxiliary_axis, auxiliary_mode, now_ms);
        pedal_service_set_auxiliary_override(&pedal_service, auxiliary.active, auxiliary.value);
        if (auxiliary_axis_take_settings(&auxiliary_axis, auxiliary_mode,
                                         &base_settings.auxiliary_axis)) {
            base_settings_persistence_mark_dirty(&settings_persistence);
        }
        uint16_t lateral_position;
        uint16_t longitudinal_position;
        bool secondary_h_pattern = false;
        bool h_pattern_input_available = shifter_input.primary_mode == SHIFTER_INPUT_H_PATTERN ||
                                         shifter_input.secondary_mode == SHIFTER_INPUT_H_PATTERN;
        bool calibration_start_allowed =
            h_pattern_input_available && wheel_startup_display_ready(&wheel_startup_display) &&
            wheel_service_protocol_phase(&wheel_service) == WHEEL_PROTOCOL_ACTIVE &&
            shifter_display.refresh_requested;
        if (h_pattern_calibration_service_start_if_required(
                &h_pattern_calibration_service, calibration_start_allowed,
                base_settings.h_pattern_shifter.calibrated, wheel_service_mode(&wheel_service),
                now_ms)) {
            shifter_display.refresh_requested = false;
        }
        if (shifter_input.primary_mode == SHIFTER_INPUT_H_PATTERN) {
            lateral_position = analog_samples.primary_shifter_x;
            longitudinal_position = analog_samples.primary_shifter_y;
        } else if (shifter_input.secondary_mode == SHIFTER_INPUT_H_PATTERN) {
            lateral_position = analog_samples.secondary_shifter_x;
            longitudinal_position = analog_samples.secondary_shifter_y;
            secondary_h_pattern = true;
        } else {
            h_pattern_shifter = (HPatternShifter){0};
            return;
        }

        bool average_longitudinal = h_pattern_calibration_service.active ||
                                    (base_settings.h_pattern_shifter.calibrated &&
                                     h_pattern_shifter_update_due(&h_pattern_shifter, now_ms));
        if (average_longitudinal) {
            longitudinal_position = platform_adc_average_shifter_y(secondary_h_pattern);
        }

        if (h_pattern_calibration_service_capture(
                &h_pattern_calibration_service, now_ms, lateral_position, longitudinal_position,
                &base_settings.h_pattern_shifter) == H_PATTERN_CALIBRATION_COMPLETED) {
            base_settings_persistence_mark_dirty(&settings_persistence);
            save_base_settings();
        }

        if (base_settings.h_pattern_shifter.calibrated && !h_pattern_calibration_service.active) {
            h_pattern_shifter_update(&h_pattern_shifter,
                                     &base_settings.h_pattern_shifter.calibration, lateral_position,
                                     longitudinal_position, now_ms);
        } else {
            h_pattern_shifter = (HPatternShifter){0};
        }
    }
}

/**
 * @brief Updates the attached-wheel glyph display.
 *
 * Gives the USB disconnect label priority over H-pattern calibration and temporary gear glyphs,
 * then publishes changed display output through the wheel service.
 *
 * @param[in] now_ms Current monotonic time in milliseconds.
 */
static void service_shifter_display(uint32_t now_ms) {
    WheelDisplayOutput *output = wheel_service_default_display_output(&wheel_service);
    bool changed = usb_disconnect_display_update(&usb_disconnect_display,
                                                 usb_disconnect_notice_visible, output);
    if (changed) {
        wheel_service_set_display_output(&wheel_service, output);
    }
    if (usb_disconnect_notice_visible || changed) {
        return;
    }
    if (!wheel_startup_display_ready(&wheel_startup_display)) {
        return;
    }

    bool wheel_active = wheel_service_protocol_phase(&wheel_service) == WHEEL_PROTOCOL_ACTIVE;
    HPatternCalibrationPrompt calibration_prompt =
        h_pattern_calibration_service_prompt(&h_pattern_calibration_service, now_ms);
    if (shifter_display_update(
            &shifter_display, h_pattern_shifter.gear, wheel_active, calibration_prompt,
            h_pattern_calibration_service.session.next_position, now_ms, output)) {
        wheel_service_set_display_output(&wheel_service, output);
    }
}

/**
 * @brief Queues the tuning-display startup version presentation.
 *
 * Sends native wheels display command 0x0A. For a connected extended adapter, builds the four
 * version lines and queues them through its offset-0x1A line transport instead.
 */
static void queue_wheel_startup_version_presentation(void) {
    const WheelAdapterInput *adapter = wheel_service_adapter(&wheel_service);
    if (wheel_startup_adapter_version_page_build(motor_probe_identity(&motor_probe), adapter,
                                                 &wheel_startup_version_page)) {
        for (uint8_t index = 0; index < WHEEL_STARTUP_VERSION_LINE_COUNT; index++) {
            (void)wheel_service_queue_adapter_text_line(
                &wheel_service, index + 1u, WHEEL_STARTUP_TEXT_METADATA,
                wheel_startup_version_page.lines[index].text,
                wheel_startup_version_page.lines[index].length);
        }
        return;
    }
    (void)wheel_service_queue_tuning_display_command(&wheel_service, WHEEL_STARTUP_VERSION_COMMAND);
}

/**
 * @brief Services the attached-wheel startup glyph presentation.
 *
 * Advances the startup sequence only after protocol activation, selects the display-capability
 * timing, supplies the identified motor-controller version and position readiness, and publishes
 * changed glyphs through the shared wheel output.
 *
 * @param[in] now_ms Current monotonic time in milliseconds.
 */
static void service_wheel_startup_display(uint32_t now_ms) {
    bool wheel_active = wheel_service_protocol_phase(&wheel_service) == WHEEL_PROTOCOL_ACTIVE;
    WheelDisplayOutput *output = wheel_service_default_display_output(&wheel_service);
    if (wheel_startup_display_update(&wheel_startup_display, wheel_active,
                                     wheel_service_tuning_display_supported(&wheel_service),
                                     motor_position_ready, motor_probe_identity(&motor_probe),
                                     now_ms, output)) {
        wheel_service_set_display_output(&wheel_service, output);
    }
    if (wheel_startup_display_take_version_presentation(&wheel_startup_display)) {
        queue_wheel_startup_version_presentation();
    }
    if (wheel_startup_display_take_version_presentation_close(&wheel_startup_display)) {
        (void)wheel_service_queue_adapter_text_close(&wheel_service);
    }
}

/**
 * @brief Applies a force-output prompt visibility action.
 *
 * Retains the current display state when no action is requested. Show and cancellation actions use
 * the shared event slot and publish their attached-wheel display state. An accepted response hides
 * the prompt immediately and publishes the common dismissal state.
 *
 * @param[in] action Requested prompt visibility transition.
 */
static void apply_force_output_prompt_action(ForceOutputEnableAction action) {
    switch (action) {
    case FORCE_OUTPUT_ENABLE_ACTION_SHOW_PROMPT:
        if (system_event_queue_try_push(&system_event_queue, FORCE_OUTPUT_PROMPT_EVENT_CODE)) {
            system_control_state_set_active_event(&system_control_state,
                                                  FORCE_OUTPUT_PROMPT_EVENT_CODE);
            (void)wheel_service_queue_tuning_display_notification(
                &wheel_service, WHEEL_DISPLAY_ENABLE_TORQUE_PROMPT);
        }
        break;
    case FORCE_OUTPUT_ENABLE_ACTION_CANCEL_PROMPT:
        if (system_event_queue_try_push(&system_event_queue,
                                        FORCE_OUTPUT_PROMPT_DISMISS_EVENT_CODE)) {
            system_control_state_set_active_event(&system_control_state,
                                                  SYSTEM_DISPLAY_DISMISS_EVENT_CODE);
            (void)wheel_service_queue_tuning_display_notification(
                &wheel_service, WHEEL_DISPLAY_ENABLE_TORQUE_CONFIRMED);
        }
        break;
    case FORCE_OUTPUT_ENABLE_ACTION_DISMISS_PROMPT:
        force_output_prompt_visible = false;
        system_control_state_set_active_event(&system_control_state,
                                              SYSTEM_DISPLAY_DISMISS_EVENT_CODE);
        (void)wheel_service_queue_tuning_display_notification(
            &wheel_service, WHEEL_DISPLAY_ENABLE_TORQUE_CONFIRMED);
        break;
    case FORCE_OUTPUT_ENABLE_ACTION_NONE:
        break;
    }
}

/**
 * @brief Builds the local system-information presentation.
 *
 * Collects the base identity straps, motor identity and telemetry, and wheel quick-release status
 * used by the three diagnostic columns.
 *
 * @param[in] now_ms Current monotonic time in milliseconds.
 * @return Current system-information values.
 */
static DisplaySystemInformation build_system_information(uint32_t now_ms) {
    DisplaySystemInformation information = {
        .main_hardware = board_identity.mode_bits,
        .main_runtime_seconds = now_ms / 1000u,
    };
    const MotorIdentity *identity = motor_probe_identity(&motor_probe);
    if (identity != NULL) {
        for (uint8_t index = 0; index < sizeof(information.motor_firmware); index++) {
            information.motor_firmware[index] = identity->version[index];
        }
        information.motor_hardware = identity->model;
    }
    if (motor_tuning_ready) {
        const MotorTelemetry *telemetry = motor_telemetry_service_value(&motor_telemetry_service);
        information.motor_accessory_type_available = telemetry->accessory_type_valid;
        information.motor_accessory_type = telemetry->accessory_type;
        if (telemetry->runtime_valid) {
            information.motor_runtime_seconds = telemetry->runtime_seconds;
        }
    }
    const WheelStatusSnapshot *quick_release = wheel_status_service_snapshot(&wheel_status_service);
    information.quick_release_firmware = quick_release->status_high;
    information.quick_release_hardware = quick_release->status_low;
    information.quick_release_runtime_seconds = quick_release->runtime_seconds;
    return information;
}

/**
 * @brief Compares two system-information presentations.
 *
 * Tests every rendered version, capability, and operating-time field so unchanged content does not
 * trigger a display transfer.
 *
 * @param[in] left First presentation to compare.
 * @param[in] right Second presentation to compare.
 * @return True when both presentations render the same values.
 */
static bool system_information_equal(const DisplaySystemInformation *left,
                                     const DisplaySystemInformation *right) {
    return left->main_hardware == right->main_hardware &&
           left->main_runtime_seconds == right->main_runtime_seconds &&
           left->motor_firmware[0] == right->motor_firmware[0] &&
           left->motor_firmware[1] == right->motor_firmware[1] &&
           left->motor_firmware[2] == right->motor_firmware[2] &&
           left->motor_hardware == right->motor_hardware &&
           left->motor_accessory_type_available == right->motor_accessory_type_available &&
           left->motor_accessory_type == right->motor_accessory_type &&
           left->motor_runtime_seconds == right->motor_runtime_seconds &&
           left->quick_release_firmware == right->quick_release_firmware &&
           left->quick_release_hardware == right->quick_release_hardware &&
           left->quick_release_runtime_seconds == right->quick_release_runtime_seconds;
}

/** @brief Latest accepted motor and driver temperatures. */
typedef struct {
    int16_t motor;
    int16_t driver;
} LocalMotorTemperatures;

/**
 * @brief Reads the latest valid motor temperatures.
 *
 * Publishes zero for either channel until its motor-telemetry response has been accepted.
 *
 * @return Motor and driver temperatures in degrees Celsius.
 */
static LocalMotorTemperatures read_motor_temperatures(void) {
    const MotorTelemetry *telemetry =
        motor_tuning_ready ? motor_telemetry_service_value(&motor_telemetry_service) : NULL;
    return (LocalMotorTemperatures){
        .motor = telemetry != NULL && telemetry->motor_temperature_valid
                     ? (int16_t)telemetry->motor_temperature
                     : 0,
        .driver = telemetry != NULL && telemetry->driver_temperature_valid
                      ? (int16_t)telemetry->driver_temperature
                      : 0,
    };
}

/**
 * @brief Updates the local display when its active page changes.
 *
 * Gives motor-originated notices priority over the persistent torque-disabled notice,
 * Torque Key prompt, force-output prompt, paddle bite-point adjustment, and local tuning page.
 * The persistent diagnostic selection presents its system-information title for one second and
 * refreshes changed component values after the title closes. The force-feedback and motor-data
 * analyzers retain their histories and refresh at their own sample cadences. The auxiliary page
 * mirrors the wheel's Legacy display while showing whether remote tuning owns it. Leaving all
 * temporary display owners restores the selected diagnostic page.
 */
static void service_local_display(void) {
    uint32_t now_ms = platform_time_ms();
    bool bite_point_visible =
        wheel_service_bite_point_adjustment(&wheel_service, &wheel_bite_point_display_percent);
    bool system_information_selected =
        usb_tuning_menu_service.active_page == USB_TUNING_MENU_PAGE_SYSTEM_INFORMATION;
    bool force_feedback_analysis_selected =
        usb_tuning_menu_service.active_page == USB_TUNING_MENU_PAGE_FORCE_FEEDBACK_ANALYSIS;
    bool motor_data_analysis_selected =
        usb_tuning_menu_service.active_page == USB_TUNING_MENU_PAGE_MOTOR_DATA_ANALYSIS;
    bool temperature_analysis_selected =
        usb_tuning_menu_service.active_page == USB_TUNING_MENU_PAGE_THERMAL_POWER;
    bool auxiliary_calibration_selected =
        usb_tuning_menu_service.active_page == USB_TUNING_MENU_PAGE_AUXILIARY_CALIBRATION;
    uint8_t page = system_notice.kind != SYSTEM_NOTICE_NONE ? LOCAL_DISPLAY_PAGE_SYSTEM_NOTICE
                   : torque_disabled_notice_visible         ? LOCAL_DISPLAY_PAGE_TORQUE_DISABLED
                   : torque_key_prompt_visible              ? LOCAL_DISPLAY_PAGE_TORQUE_KEY_PROMPT
                   : force_output_prompt_visible            ? LOCAL_DISPLAY_PAGE_FORCE_OUTPUT_PROMPT
                   : bite_point_visible                     ? LOCAL_DISPLAY_PAGE_BITE_POINT
                   : tuning_menu.selected_entry < TUNING_ENTRY_COUNT ? LOCAL_DISPLAY_PAGE_TUNING
                   : system_information_selected      ? LOCAL_DISPLAY_PAGE_SYSTEM_INFORMATION
                   : force_feedback_analysis_selected ? LOCAL_DISPLAY_PAGE_FORCE_FEEDBACK_ANALYSIS
                   : motor_data_analysis_selected     ? LOCAL_DISPLAY_PAGE_MOTOR_DATA_ANALYSIS
                   : temperature_analysis_selected    ? LOCAL_DISPLAY_PAGE_TEMPERATURE_ANALYSIS
                   : auxiliary_calibration_selected   ? LOCAL_DISPLAY_PAGE_AUXILIARY_CALIBRATION
                                                      : LOCAL_DISPLAY_PAGE_IDENTITY;
    local_display_system_information = build_system_information(now_ms);
    bool system_information_changed =
        page == LOCAL_DISPLAY_PAGE_SYSTEM_INFORMATION && page == local_display_page &&
        ((!local_display_system_information_content_active &&
          platform_time_reached(now_ms, local_display_system_information_title_deadline_ms)) ||
         (local_display_system_information_content_active &&
          !system_information_equal(&local_display_system_information,
                                    &local_display_rendered_system_information)));
    bool force_feedback_sampled = false;
    if (page == LOCAL_DISPLAY_PAGE_FORCE_FEEDBACK_ANALYSIS && page == local_display_page) {
        int32_t torque = motor_position_ready ? (int16_t)motor_position_report.motor_torque : 0;
        uint32_t torque_limit =
            board_identity.variant == BOARD_VARIANT_DD1 ? UINT32_C(20000) : UINT32_C(25000);
        uint32_t magnitude = torque < 0 ? (uint32_t)-torque : (uint32_t)torque;
        magnitude = magnitude >= torque_limit ? UINT16_MAX : magnitude * UINT16_MAX / torque_limit;
        force_feedback_sampled = display_force_feedback_analysis_page_update(
            &local_display_force_feedback_analysis, now_ms, torque >= 0, (uint16_t)magnitude);
    }
    bool force_feedback_analysis_changed =
        page == LOCAL_DISPLAY_PAGE_FORCE_FEEDBACK_ANALYSIS && page == local_display_page &&
        ((!local_display_force_feedback_analysis_content_active &&
          platform_time_reached(now_ms, local_display_force_feedback_analysis_title_deadline_ms)) ||
         (local_display_force_feedback_analysis_content_active && force_feedback_sampled));
    bool motor_data_changed = false;
    if (page == LOCAL_DISPLAY_PAGE_MOTOR_DATA_ANALYSIS && page == local_display_page) {
        LocalMotorTemperatures temperatures = read_motor_temperatures();
        int16_t torque = motor_position_ready ? (int16_t)motor_position_report.motor_torque : 0;
        motor_data_changed = display_motor_data_analysis_page_update(
            &local_display_motor_data_analysis, now_ms, torque, temperatures.motor,
            temperatures.driver, fan_speed_rpm[PLATFORM_FAN_PRIMARY]);
    }
    bool motor_data_analysis_changed =
        page == LOCAL_DISPLAY_PAGE_MOTOR_DATA_ANALYSIS && page == local_display_page &&
        ((!local_display_motor_data_analysis_content_active &&
          platform_time_reached(now_ms, local_display_motor_data_analysis_title_deadline_ms)) ||
         (local_display_motor_data_analysis_content_active && motor_data_changed));
    bool temperature_data_changed = false;
    if (page == LOCAL_DISPLAY_PAGE_TEMPERATURE_ANALYSIS && page == local_display_page) {
        int16_t temperatures[DISPLAY_TEMPERATURE_ANALYSIS_CHANNEL_COUNT];
        LocalMotorTemperatures motor_temperatures = read_motor_temperatures();
        temperatures[DISPLAY_TEMPERATURE_ANALYSIS_MOTOR] = motor_temperatures.motor;
        temperatures[DISPLAY_TEMPERATURE_ANALYSIS_DRIVER] = motor_temperatures.driver;
        temperatures[DISPLAY_TEMPERATURE_ANALYSIS_BASE] =
            cooling_temperature_monitor.temperatures_c[0];
        temperatures[DISPLAY_TEMPERATURE_ANALYSIS_QUICK_RELEASE] =
            (int16_t)wheel_status_service_snapshot(&wheel_status_service)->accessory_value;
        temperature_data_changed = display_temperature_analysis_page_update(
            &local_display_temperature_analysis, now_ms, temperatures,
            fan_speed_rpm[PLATFORM_FAN_PRIMARY], cooling_controller.force_scale_percent);
    }
    bool temperature_analysis_changed =
        page == LOCAL_DISPLAY_PAGE_TEMPERATURE_ANALYSIS && page == local_display_page &&
        ((!local_display_temperature_analysis_content_active &&
          platform_time_reached(now_ms, local_display_temperature_analysis_title_deadline_ms)) ||
         (local_display_temperature_analysis_content_active && temperature_data_changed));
    bool auxiliary_calibration_data_changed = false;
    if (page == LOCAL_DISPLAY_PAGE_AUXILIARY_CALIBRATION && page == local_display_page) {
        auxiliary_calibration_data_changed = display_auxiliary_calibration_page_update(
            &local_display_auxiliary_calibration, wheel_service.display_output.glyphs,
            usb_remote_tuning_service.active);
    }
    bool auxiliary_calibration_changed =
        page == LOCAL_DISPLAY_PAGE_AUXILIARY_CALIBRATION && page == local_display_page &&
        ((!local_display_auxiliary_calibration_content_active &&
          platform_time_reached(now_ms, local_display_auxiliary_calibration_title_deadline_ms)) ||
         (local_display_auxiliary_calibration_content_active &&
          auxiliary_calibration_data_changed));
    if (page == local_display_page &&
        (page != LOCAL_DISPLAY_PAGE_BITE_POINT ||
         wheel_bite_point_display_percent == local_display_rendered_bite_point_percent) &&
        (page != LOCAL_DISPLAY_PAGE_SYSTEM_NOTICE ||
         system_notice.kind == local_display_rendered_notice_kind) &&
        (page != LOCAL_DISPLAY_PAGE_TUNING ||
         local_display_tuning_revision == local_display_rendered_tuning_revision) &&
        !system_information_changed && !force_feedback_analysis_changed &&
        !motor_data_analysis_changed && !temperature_analysis_changed &&
        !auxiliary_calibration_changed) {
        return;
    }

    if (page == LOCAL_DISPLAY_PAGE_SYSTEM_NOTICE) {
        display_notice_render_system(display_framebuffer, system_notice.kind);
    } else if (page == LOCAL_DISPLAY_PAGE_TORQUE_DISABLED) {
        display_notice_render_torque_disabled(display_framebuffer, true);
    } else if (page == LOCAL_DISPLAY_PAGE_TORQUE_KEY_PROMPT) {
        display_prompt_render_torque_key(display_framebuffer, true);
    } else if (page == LOCAL_DISPLAY_PAGE_FORCE_OUTPUT_PROMPT) {
        display_prompt_render(display_framebuffer, true);
    } else if (page == LOCAL_DISPLAY_PAGE_TUNING) {
        if (tuning_interaction.result_deadline_ms == 0 ||
            !display_tuning_operation_render(display_framebuffer, tuning_interaction.phase)) {
            (void)display_tuning_page_render(display_framebuffer, &tuning_menu,
                                             &base_settings.tuning_profiles);
        }
    } else if (page == LOCAL_DISPLAY_PAGE_SYSTEM_INFORMATION) {
        if (page != local_display_page) {
            display_system_information_page_render_title(display_framebuffer);
            local_display_system_information_title_deadline_ms =
                now_ms + LOCAL_DISPLAY_DIAGNOSTIC_TITLE_MS;
            local_display_system_information_content_active = false;
        } else {
            display_system_information_page_render(display_framebuffer,
                                                   &local_display_system_information);
            local_display_rendered_system_information = local_display_system_information;
            local_display_system_information_content_active = true;
        }
    } else if (page == LOCAL_DISPLAY_PAGE_FORCE_FEEDBACK_ANALYSIS) {
        if (page != local_display_page) {
            display_force_feedback_analysis_page_open(&local_display_force_feedback_analysis,
                                                      now_ms);
            display_force_feedback_analysis_page_render_title(display_framebuffer);
            local_display_force_feedback_analysis_title_deadline_ms =
                now_ms + LOCAL_DISPLAY_DIAGNOSTIC_TITLE_MS;
            local_display_force_feedback_analysis_content_active = false;
        } else {
            display_force_feedback_analysis_page_render(display_framebuffer,
                                                        &local_display_force_feedback_analysis);
            local_display_force_feedback_analysis_content_active = true;
        }
    } else if (page == LOCAL_DISPLAY_PAGE_MOTOR_DATA_ANALYSIS) {
        if (page != local_display_page) {
            display_motor_data_analysis_page_open(&local_display_motor_data_analysis, now_ms,
                                                  board_identity.variant);
            display_motor_data_analysis_page_render_title(display_framebuffer);
            local_display_motor_data_analysis_title_deadline_ms =
                now_ms + LOCAL_DISPLAY_DIAGNOSTIC_TITLE_MS;
            local_display_motor_data_analysis_content_active = false;
        } else {
            display_motor_data_analysis_page_render(display_framebuffer,
                                                    &local_display_motor_data_analysis);
            local_display_motor_data_analysis_content_active = true;
        }
    } else if (page == LOCAL_DISPLAY_PAGE_TEMPERATURE_ANALYSIS) {
        if (page != local_display_page) {
            display_temperature_analysis_page_open(&local_display_temperature_analysis, now_ms);
            display_temperature_analysis_page_render_title(display_framebuffer);
            local_display_temperature_analysis_title_deadline_ms =
                now_ms + LOCAL_DISPLAY_DIAGNOSTIC_TITLE_MS;
            local_display_temperature_analysis_content_active = false;
        } else {
            display_temperature_analysis_page_render(display_framebuffer,
                                                     &local_display_temperature_analysis);
            local_display_temperature_analysis_content_active = true;
        }
    } else if (page == LOCAL_DISPLAY_PAGE_AUXILIARY_CALIBRATION) {
        if (page != local_display_page) {
            display_auxiliary_calibration_page_render_title(display_framebuffer);
            local_display_auxiliary_calibration_title_deadline_ms =
                now_ms + LOCAL_DISPLAY_DIAGNOSTIC_TITLE_MS;
            local_display_auxiliary_calibration_content_active = false;
        } else {
            display_auxiliary_calibration_page_render(display_framebuffer,
                                                      &local_display_auxiliary_calibration);
            local_display_auxiliary_calibration_content_active = true;
        }
    } else if (page == LOCAL_DISPLAY_PAGE_IDENTITY) {
        display_identity_page_render(display_framebuffer, board_identity);
    } else {
        display_prompt_render_bite_point(display_framebuffer, page == LOCAL_DISPLAY_PAGE_BITE_POINT,
                                         wheel_bite_point_display_percent);
    }
    platform_display_write_frame(display_framebuffer);
    local_display_page = page;
    local_display_rendered_bite_point_percent = wheel_bite_point_display_percent;
    local_display_rendered_notice_kind = system_notice.kind;
    local_display_rendered_tuning_revision = local_display_tuning_revision;
}

/**
 * @brief Services force-output readiness and operator acknowledgement.
 *
 * Removes enabled force output when its wheel or USB prerequisite disappears. While interlocked,
 * accepts a released wheel input only when the force-output prompt owns the display and advances
 * prompt presentation through the shared event slot.
 */
static void service_force_output_enable(void) {
    bool wheel_protocol_ready = wheel_service_force_output_ready(&wheel_service);
    bool usb_connected = !usb_connection_monitor.disconnected;

    if (force_output_enabled && (!wheel_protocol_ready || !usb_connected)) {
        force_output_enabled = false;
    }
    if (force_output_enabled) {
        return;
    }
    force_feedback_state_deactivate_host_effects(&force_feedback_state);

    bool prompt_visible = force_output_prompt_visible && system_notice.kind == SYSTEM_NOTICE_NONE &&
                          !torque_disabled_notice_visible && !torque_key_prompt_visible;
    if (display_prompt_update(&force_output_display_prompt, prompt_visible,
                              wheel_service_acknowledgement_input_active(&wheel_service))) {
        force_output_enable_set_response(&force_output_enable, 1);
    }

    bool interlocked = force_output_enable_service(
        &force_output_enable, wheel_protocol_ready, usb_connected,
        system_event_queue.pending_code == 0, &force_output_enable_action);
    apply_force_output_prompt_action(force_output_enable_action);
    force_output_enabled = !interlocked;
}

/**
 * @brief Finishes direct-force interpretation after attached-wheel startup.
 *
 * Keeps the motor in direct-force mode through wheel discovery. Normal motor-side effect handling
 * resumes when wheel protocol selection completes or the 500-millisecond discovery window expires.
 *
 * @param[in] now_ms Current monotonic time in milliseconds.
 */
static void service_motor_startup_force_mode(uint32_t now_ms) {
    if (!motor_startup_direct_force || !motor_startup_wheel_discovery_started) {
        return;
    }
    if (wheel_service_force_output_ready(&wheel_service) ||
        platform_time_reached(now_ms, motor_startup_direct_force_deadline_ms)) {
        motor_startup_direct_force = false;
        motor_startup_wheel_discovery_started = false;
    }
}

/**
 * @brief Services the unsupported-wheel compatibility alert.
 *
 * Suppresses force output while the attached wheel remains in the unsupported protocol phase,
 * alternates the local warning presentation once per second, and blinks the three-character
 * firmware-update-required indication every 125 milliseconds.
 *
 * @param[in] now_ms Current monotonic time in milliseconds.
 */
static void service_wheel_compatibility_alert(uint32_t now_ms) {
    bool unsupported = wheel_service_protocol_phase(&wheel_service) == WHEEL_PROTOCOL_UNSUPPORTED;
    WheelCompatibilityAlertAction action = wheel_compatibility_alert_update(
        &wheel_compatibility_alert, unsupported, system_event_queue.pending_code == 0, now_ms);
    if (action == WHEEL_COMPATIBILITY_ALERT_ACTION_SHOW_INVERTED ||
        action == WHEEL_COMPATIBILITY_ALERT_ACTION_SHOW_OUTLINED) {
        uint8_t event_code = action == WHEEL_COMPATIBILITY_ALERT_ACTION_SHOW_INVERTED
                                 ? UNSUPPORTED_WHEEL_INVERTED_EVENT_CODE
                                 : UNSUPPORTED_WHEEL_OUTLINED_EVENT_CODE;
        if (system_event_queue_try_push(&system_event_queue, event_code)) {
            system_control_state_set_active_event(&system_control_state,
                                                  UNSUPPORTED_WHEEL_INVERTED_EVENT_CODE);
        }
    } else if (action == WHEEL_COMPATIBILITY_ALERT_ACTION_CLEAR) {
        if (system_notice.kind == SYSTEM_NOTICE_UNSUPPORTED_WHEEL_INVERTED ||
            system_notice.kind == SYSTEM_NOTICE_UNSUPPORTED_WHEEL_OUTLINED) {
            system_notice_init(&system_notice);
        }
        system_control_state_set_active_event(&system_control_state,
                                              SYSTEM_DISPLAY_DISMISS_EVENT_CODE);
    }

    if (!unsupported) {
        return;
    }
    force_output_enabled = false;
    wheel_compatibility_display_output = (WheelDisplayOutput){0};
    if (wheel_compatibility_alert_segment_visible(now_ms)) {
        wheel_compatibility_display_output.glyphs[0] = 0x3e;
        wheel_compatibility_display_output.glyphs[1] = 0x73;
        wheel_compatibility_display_output.glyphs[2] = 0xde;
    }
    wheel_service_set_display_output(&wheel_service, &wheel_compatibility_display_output);
}

/**
 * @brief Services Xbox host-capability recovery.
 *
 * Combines the current USB operating mode with attached-wheel and adapter capability state. When
 * the recovery policy requests it, the USB controller emits resume signaling without rebuilding
 * the active descriptors or endpoint state.
 *
 * @param[in] now_ms Current monotonic time in milliseconds.
 */
static void service_usb_host_capability_recovery(uint32_t now_ms) {
    const UsbHostCapabilityRecoveryInput input = {
        .wheel_mode = wheel_service_mode(&wheel_service),
        .wheel_capability_flags = wheel_service_capability_flags(&wheel_service),
        .xbox_mode = usb_device_operating_mode() == USB_OPERATING_MODE_XBOX_GIP,
        .host_capability_enabled = wheel_service_host_capability_enabled(&wheel_service),
        .adapter_requests_capability =
            wheel_service_adapter_requests_host_capability(&wheel_service),
    };
    if (usb_host_capability_recovery_update(&usb_host_capability_recovery, input, now_ms) ==
        USB_HOST_CAPABILITY_RECOVERY_SIGNAL_RESUME) {
        platform_usb_signal_resume();
    }
}

/**
 * @brief Runs the board LED startup brightness sweep.
 *
 * Writes the first pattern of buckets zero through 62 in ascending order and retains each pattern
 * through its strict 50-millisecond deadline. The first autonomous normal update selects the
 * remaining full-scale bucket.
 */
static void run_led_pattern_startup_sequence(void) {
    for (uint8_t step = 0; step < LED_PATTERN_STARTUP_STEP_COUNT; ++step) {
        platform_led_pattern_set_duty(led_pattern_pwm_duty(led_pattern_startup_pattern(step)));
        uint32_t deadline_ms = platform_time_ms() + 50;
        while (!platform_time_reached(platform_time_ms(), deadline_ms + 1)) {
        }
    }
}

/**
 * @brief Services autonomous board LED output.
 *
 * Selects the inhibited-output heartbeat from motor status, clears the LED after shutdown starts,
 * and requests the breathing transition from the torque-disable toggle or pedal recovery
 * handshake. A returned no-update marker leaves host-selected output unchanged.
 *
 * @param[in] now_ms Current monotonic time in milliseconds.
 */
static void service_led_pattern(uint32_t now_ms) {
    LedPatternControllerInput input = {
        .output_inhibited =
            motor_tuning_ready && motor_status_service_output_inhibited(&motor_status_service),
        .shutdown_complete = power_controller.phase == POWER_PHASE_SHUTDOWN_DELAY ||
                             power_controller.phase == POWER_PHASE_OFF,
        .pedal_handshake_active = pedal_service.recovery_handshake,
        .force_override_requested = power_controller.torque_disabled,
    };
    uint16_t pattern = led_pattern_controller_update(&led_pattern_controller, input, now_ms);
    if (pattern != LED_PATTERN_NO_UPDATE) {
        platform_led_pattern_set_duty(led_pattern_pwm_duty((uint8_t)pattern));
    }
}

/**
 * @brief Initializes and runs the wheel-base application.
 *
 * Brings up board safety, display, analog, pedal, serial, wheel, motor, settings, force-feedback,
 * and USB services, then continuously advances their cooperative runtime state machines.
 *
 * @return This function does not return during normal operation.
 */
int main(void) {
    platform_clock_init();
    board_identity = platform_board_identity_read();
    platform_pin_mux_init();
    platform_system_init();
    platform_power_init();
    power_controller_init(&power_controller);
    service_power(0);
    platform_system_enable_firmware_protection();
    platform_led_pattern_init();
    platform_torque_key_init();
    torque_key_init(&torque_key);
    torque_key_prompt_init(&torque_key_prompt);
    system_control_state_init(&system_control_state);
    system_torque_transition_init(&system_torque_transition);
    system_event_queue_init(&system_event_queue);
    system_event_dispatcher_init(&system_event_dispatcher);
    system_notice_init(&system_notice);
    platform_time_init();
    initialize_cooling();
    led_pattern_controller_init(&led_pattern_controller);
    run_led_pattern_startup_sequence();
    platform_display_init();
    platform_display_write_frame(display_framebuffer);
    platform_adc_init();
    platform_shifter_init();
    platform_shifter_read(&shifter_input);
    shifter_display_init(&shifter_display);
    wheel_startup_display_init(&wheel_startup_display);
    usb_disconnect_display_init(&usb_disconnect_display);
    platform_aux_bus_init();
    platform_pedal_link_init();
    pedal_service_init(&pedal_service);
    pedal_brake_indicator_init(&pedal_brake_indicator);
    platform_serial_link_init();
    serial_service_init(&serial_service);
    wheel_service_init(&wheel_service, &serial_service);
    wheel_compatibility_alert_init(&wheel_compatibility_alert);
    fanatec_encoder_init(&fanatec_encoder);
    usb_xbox_gip_input_builder_init(&xbox_input_builder);
    wheel_status_service_init(&wheel_status_service, &serial_service);
    runtime_bridge_init(&runtime_bridge);
    initialize_usb_command_bridge();
    wheel_velocity_reset(&wheel_velocity_estimator);
    wheel_center_capture_command_init(&wheel_center_capture_command);
    initialize_motor_link();
    initialize_base_settings();
    initialize_motor();
    initialize_force_feedback_script();
    initialize_startup_usb();
    service_usb_feature_reports();
    usb_connection_monitor_init(&usb_connection_monitor);
    usb_host_capability_recovery_init(&usb_host_capability_recovery);
    for (;;) {
        usb_device_service();
        service_usb_xbox_session_actions();
        service_usb_output();
        platform_aux_bus_service();
        uint32_t now_ms = platform_time_ms();
        service_power(now_ms);
        service_power_torque_request();
        if (service_runtime_bridge(now_ms)) {
            continue;
        }
        service_playstation_authentication(now_ms);
        service_usb_host_capability_recovery(now_ms);
        service_system_events(now_ms);
        UsbConnectionAction usb_connection_action = usb_connection_monitor_update(
            &usb_connection_monitor, platform_usb_connected(),
            wheel_startup_display_ready(&wheel_startup_display), now_ms);
        if ((usb_connection_action & USB_CONNECTION_ACTION_NOTIFY_DISCONNECTED) != 0) {
            system_control_state_set_status(&system_control_state,
                                            wheel_service_mode(&wheel_service),
                                            USB_DISCONNECT_STATUS_CODE);
        }
        if ((usb_connection_action & USB_CONNECTION_ACTION_SHOW_DISCONNECTED) != 0) {
            usb_disconnect_notice_visible = true;
        } else if ((usb_connection_action & USB_CONNECTION_ACTION_CLEAR_NOTIFICATION) != 0) {
            usb_disconnect_notice_visible = false;
        }
        platform_cooling_service(now_ms);
        update_fan_speed(PLATFORM_FAN_PRIMARY);
        update_fan_speed(PLATFORM_FAN_SECONDARY);
        service_analog_input(now_ms);
        service_motor_link();
        pedal_service_run(&pedal_service, now_ms);
        service_pedal_adjustment_display(now_ms);
        service_led_pattern(now_ms);
        service_alternate_brake_force(now_ms);
        uint8_t brake_indicator_selector = pedal_brake_indicator_update(
            &pedal_brake_indicator, tuning_profile->brake_indicator_level,
            pedal_service_input(&pedal_service)->axes[1],
            pedal_service_legacy_transport_active(&pedal_service));
        if (brake_indicator_selector != PEDAL_BRAKE_INDICATOR_NO_UPDATE) {
            pedal_service_set_brake_indicator_selector(&pedal_service, brake_indicator_selector);
        }
        wheel_vibration_from_brake(
            &wheel_vibration_output, pedal_service_input(&pedal_service)->axes[1],
            tuning_profile->vibration_strength, wheel_service_mode(&wheel_service),
            pedal_brake_indicator.selector != 0);
        wheel_service_set_vibration_output(&wheel_service, &wheel_vibration_output);
        serial_service_run(&serial_service, now_ms);
        service_usb_command_bridge(now_ms);
        wheel_status_service_run(&wheel_status_service, now_ms, !serial_command_waiting());
        wheel_service_configure_axis_processing(
            &wheel_service, (uint8_t)usb_device_operating_mode(),
            (uint8_t)tuning_profile->paddle_mode,
            wheel_steering_limits_active(&base_settings.steering_limits,
                                         base_settings.tuning_profiles.active_slot),
            now_ms);
        if (usb_playstation_wheel_value_expire(&usb_playstation_wheel_value, now_ms)) {
            wheel_service_set_legacy_axes(
                &wheel_service, usb_playstation_wheel_value_axes(&usb_playstation_wheel_value));
        }
        if (usb_playstation_wheel_value_copy_axes(&usb_playstation_wheel_value,
                                                  wheel_service_clutch_paddles(&wheel_service))) {
            wheel_service_set_legacy_axes(
                &wheel_service, usb_playstation_wheel_value_axes(&usb_playstation_wheel_value));
        }
        wheel_position_calibration = wheel_position_calibration_build(
            &base_settings.wheel_position, tuning_profile->rotation_degrees,
            tuning_profile->steering_deadzone);
        service_motor_rotation_guard(now_ms);
        wheel_service_set_display_rotation(
            &wheel_service, tuning_profile->display_rotation_enabled != 0,
            wheel_position_display_rotation(motor_position_report.wheel_position,
                                            &wheel_position_calibration));
        if (system_control_state_take_status(&system_control_state, &pending_system_status_code)) {
            wheel_service_queue_system_status(&wheel_service, pending_system_status_code);
        }
        if (system_control_state_take_wheel_response(&system_control_state,
                                                     &system_wheel_response)) {
            (void)wheel_service_queue_system_control_response(&wheel_service,
                                                              &system_wheel_response);
        }
        wheel_service_run(&wheel_service, now_ms, !serial_command_waiting());
        wheel_service_update_interface_mode_gate(&wheel_service, now_ms);
        service_motor_startup_force_mode(now_ms);
        service_tuning_interaction(now_ms);
        service_alternative_shifter(now_ms);
        if (wheel_service_take_bite_point(&wheel_service, &wheel_adjusted_bite_point_percent)) {
            wheel_steering_limit_command = (WheelSteeringLimitCommand){
                .percent = wheel_adjusted_bite_point_percent,
                .reset_all = false,
            };
            apply_wheel_steering_limit_command(&wheel_steering_limit_command);
        }
        if (serial_service.status == SERIAL_SERVICE_IDLE) {
            (void)motor_command_serial_submit(&command_transport, &serial_service, now_ms);
        }
        service_torque_key(now_ms);
        service_force_feedback_script(now_ms);
        service_force_output_enable();
        service_local_display();
        service_wheel_startup_display(now_ms);
        service_shifter_display(now_ms);
        service_wheel_compatibility_alert(now_ms);
        (void)wheel_service_update_display_overlay(&wheel_service, now_ms);
        service_usb_input(now_ms);
        service_usb_feature_reports();
        service_motor();
        service_cooling(now_ms);
    }
}
