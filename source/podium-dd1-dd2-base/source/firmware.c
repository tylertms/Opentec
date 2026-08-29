#include <stdbool.h>
#include <xc.h>

#include "analog/auxiliary_axis.h"
#include "board/identity.h"
#include "board/status_led.h"
#include "cooling/controller.h"
#include "cooling/effect_limit.h"
#include "cooling/tachometer.h"
#include "cooling/temperature.h"
#include "display/prompt.h"
#include "force_feedback/command.h"
#include "force_feedback/output.h"
#include "force_feedback/output_enable.h"
#include "force_feedback/script_runtime.h"
#include "force_feedback/state.h"
#include "motor/command_mailbox.h"
#include "motor/command_serial.h"
#include "motor/live_frame.h"
#include "motor/output_transport.h"
#include "motor/probe.h"
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
#include "platform/motor_link.h"
#include "platform/pedal_link.h"
#include "platform/pin_mux.h"
#include "platform/serial_link.h"
#include "platform/shifter.h"
#include "platform/status_led.h"
#include "platform/time.h"
#include "platform/usb.h"
#include "profile/bank.h"
#include "profile/tuning.h"
#include "serial/service.h"
#include "settings/persistence.h"
#include "settings/state.h"
#include "shifter/display.h"
#include "shifter/h_pattern.h"
#include "shifter/input.h"
#include "usb/connection.h"
#include "usb/device.h"
#include "usb/diagnostic_report.h"
#include "usb/fanatec_encoder.h"
#include "usb/fanatec_input.h"
#include "usb/input_report.h"
#include "usb/motor_vendor_service.h"
#include "usb/operating_mode_command.h"
#include "usb/output_command.h"
#include "usb/remote_tuning_service.h"
#include "usb/tuning_menu_service.h"
#include "usb/tuning_profile_report.h"
#include "usb/tuning_profile_service.h"
#include "usb/vendor_command.h"
#include "wheel/command_forwarder.h"
#include "wheel/position.h"
#include "wheel/service.h"
#include "wheel/status_service.h"
#include "wheel/steering_limit.h"
#include "wheel/transfer_service.h"
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

static BoardIdentity board_identity;
static MotorProbe motor_probe;
static CommandTransport command_transport;
static MotorCommandMailboxExchange motor_command_mailbox;
static UsbMotorVendorService usb_motor_vendor_service;
static WheelTransferService wheel_transfer_service;
static MotorStatusService motor_status_service;
static MotorTelemetryService motor_telemetry_service;
static MotorTuningService motor_tuning_service;
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
static PedalService pedal_service;
static PedalBrakeIndicator pedal_brake_indicator;
static WheelVibrationOutput wheel_vibration_output;
static SerialService serial_service;
static WheelService wheel_service;
static WheelStatusService wheel_status_service;
static AnalogSamples analog_samples;
static AuxiliaryAxis auxiliary_axis;
static HPatternShifter h_pattern_shifter;
static ShifterInputState shifter_input;
static ShifterDisplay shifter_display;
static UsbInputReportState usb_input_state;
static FanatecEncoder fanatec_encoder;
static WheelMultiPositionInput wheel_multi_position_input;
static fanatec_multi_position_input fanatec_multi_position_input_state;
static uint8_t usb_input_report[USB_INPUT_REPORT_MAX_SIZE];
static uint8_t usb_motor_acknowledgement[USB_DEVICE_REPORT_SIZE];
static uint8_t usb_motor_response[USB_DEVICE_REPORT_SIZE];
static uint8_t usb_wheel_transfer_response[USB_DEVICE_REPORT_SIZE];
static UsbDiagnosticReportService usb_diagnostic_report_service;
static UsbRemoteTuningService usb_remote_tuning_service;
static WheelCommandForwarder wheel_command_forwarder;
static uint8_t wheel_command_batch[USB_REMOTE_TUNING_FORWARD_BATCH_SIZE];
static uint8_t wheel_command_batch_length;
static RemoteTuningResponse usb_remote_tuning_response;
static uint8_t usb_remote_tuning_host_report[USB_REMOTE_TUNING_HOST_REPORT_SIZE];
static uint8_t wheel_remote_telemetry_report[REMOTE_TELEMETRY_REPORT_SIZE];
static UsbTuningMenuService usb_tuning_menu_service;
static UsbTuningProfileService usb_tuning_profile_service;
static UsbDiagnosticSnapshot usb_diagnostic_snapshot;
static uint8_t usb_diagnostic_report[USB_DEVICE_REPORT_SIZE];
static uint8_t usb_tuning_menu_response[USB_DEVICE_REPORT_SIZE];
static uint8_t usb_tuning_profile_response[USB_DEVICE_REPORT_SIZE];
static UsbDeviceOutputReport usb_device_output_report;
static UsbConnectionMonitor usb_connection_monitor;
static UsbOutputCommand usb_output_command;
static UsbOperatingModeCommand usb_operating_mode_command;
static PedalCalibrationCommand pedal_calibration_command;
static PedalCalibrationActions pedal_calibration_actions;
static PedalProtocolCommand pedal_protocol_command;
static WheelSteeringLimitCommand wheel_steering_limit_command;
static uint8_t wheel_adjusted_bite_point_percent;
static uint8_t wheel_bite_point_report_percent;
static UsbVendorCommand usb_vendor_command;
static UsbWheelTransferCommand usb_wheel_transfer_command;
static ForceFeedbackCommand force_feedback_command;
static ForceFeedbackState force_feedback_state;
static ForceFeedbackScriptSystem force_feedback_script_system;
static ForceOutputReport motor_output_report;
static MotorLiveFrame motor_live_frame;
static MotorOutputTransport motor_output_transport;
static uint8_t motor_received_frame[MOTOR_LIVE_FRAME_SIZE];
static uint8_t motor_transmitted_frame[MOTOR_LIVE_FRAME_SIZE];
static StatusLed status_led;
static CoolingController cooling_controller;
static CoolingEffectLimit cooling_effect_limit;
static CoolingEffectStrengths cooling_effect_strengths;
static CoolingTemperatureMonitor cooling_temperature_monitor;
static PlatformFanTachometer fan_tachometer;
static uint16_t fan_speed_rpm[2];
static uint8_t display_framebuffer[DISPLAY_FRAMEBUFFER_SIZE];
static DisplayPrompt display_prompt;
static ForceOutputEnable force_output_enable;
static ForceOutputEnableAction force_output_enable_action;
static bool force_output_enabled;
static bool force_output_prompt_visible;
static bool usb_motor_acknowledgement_ready;
static bool usb_motor_response_ready;
static bool usb_remote_tuning_host_report_ready;
static bool usb_wheel_transfer_response_ready;
static bool usb_tuning_menu_response_ready;
static bool usb_tuning_profile_response_ready;
static bool usb_wheel_transfer_response_pending[WHEEL_TRANSFER_REQUEST_COUNT];
static uint8_t local_display_page;
static uint8_t local_display_rendered_bite_point_percent;
static uint8_t wheel_bite_point_display_percent;

enum {
    FAN_STARTUP_DUTY_PERCENT = 25,
    USB_MOTOR_BUFFER_SIZE = MEMORY_TRANSFER_MAX_READ_SIZE,
    LOCAL_DISPLAY_PAGE_CLEAR = 0,
    LOCAL_DISPLAY_PAGE_TORQUE_PROMPT = 1,
    LOCAL_DISPLAY_PAGE_BITE_POINT = 2,
};

static uint8_t usb_motor_upload_assembly[USB_MOTOR_BUFFER_SIZE];
static uint8_t usb_motor_receive_assembly[USB_MOTOR_BUFFER_SIZE];
static uint8_t usb_motor_mailbox_receive[USB_MOTOR_BUFFER_SIZE];
static uint8_t usb_motor_transmit[USB_MOTOR_BUFFER_SIZE];
static uint8_t usb_motor_application_data[USB_MOTOR_BUFFER_SIZE];
static const UsbMotorVendorServiceBuffers usb_motor_buffers = {
    .upload_assembly = usb_motor_upload_assembly,
    .upload_assembly_capacity = sizeof(usb_motor_upload_assembly),
    .receive_assembly = usb_motor_receive_assembly,
    .receive_assembly_capacity = sizeof(usb_motor_receive_assembly),
    .motor_transmit = usb_motor_transmit,
    .motor_transmit_capacity = sizeof(usb_motor_transmit),
    .application_data = usb_motor_application_data,
    .application_data_capacity = sizeof(usb_motor_application_data),
};

static void initialize_cooling(void) {
    cooling_controller_init(&cooling_controller, board_identity.mode_bits == 7);
    cooling_effect_limit_init(&cooling_effect_limit);
    cooling_temperature_monitor_init(&cooling_temperature_monitor);
    fan_speed_rpm[PLATFORM_FAN_PRIMARY] = 0;
    fan_speed_rpm[PLATFORM_FAN_SECONDARY] = 0;
    platform_cooling_init(board_identity.mode_bits != 7);
    platform_cooling_set_duty(FAN_STARTUP_DUTY_PERCENT, FAN_STARTUP_DUTY_PERCENT, false);
}

static void service_cooling(uint32_t now_ms) {
    bool managed_motor_present = motor_probe_identity(&motor_probe) != 0;
    const MotorTelemetry *telemetry =
        motor_tuning_ready ? motor_telemetry_service_value(&motor_telemetry_service) : 0;
    float motor_temperature = telemetry != 0 && telemetry->motor_temperature_valid
                                  ? (float)(int16_t)telemetry->motor_temperature
                                  : 0.0f;
    bool output_inhibited =
        motor_tuning_ready && motor_status_service_output_inhibited(&motor_status_service);
    cooling_controller_update(&cooling_controller, motor_temperature, managed_motor_present,
                              output_inhibited, now_ms);
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

static void update_fan_speed(PlatformFan fan) {
    if (!platform_cooling_take_tachometer(fan, &fan_tachometer)) {
        return;
    }

    fan_speed_rpm[fan] =
        fan_tachometer.present
            ? fan_tachometer_rpm(fan_tachometer.previous_capture, fan_tachometer.current_capture)
            : 0;
}

static void initialize_motor_link(void) {
    motor_output_report = (ForceOutputReport){0};
    motor_output_transport_init(&motor_output_transport);
    motor_output_transport_build_frame(&motor_output_transport, MOTOR_OUTPUT_STATUS_REMOTE_EFFECTS,
                                       0, &motor_output_report, &motor_live_frame);
    motor_live_frame_encode(&motor_live_frame, motor_transmitted_frame);
    platform_motor_link_init(motor_transmitted_frame);
    motor_position_ready = false;
}

/**
 * @brief Builds the motor controller's force-feedback status byte.
 *
 * Selects remote motor-side effect processing, reports whether motor safety, USB connection, and
 * operator confirmation permit force, and mirrors the primary and secondary output gates.
 *
 * @return Current force-feedback status bits for the next motor-link packet.
 */
static uint8_t motor_force_feedback_status(void) {
    uint8_t status = MOTOR_OUTPUT_STATUS_REMOTE_EFFECTS;
    if (motor_tuning_ready && !motor_status_service_output_inhibited(&motor_status_service) &&
        !usb_connection_monitor.disconnected && force_output_enabled) {
        status |= MOTOR_OUTPUT_STATUS_ENABLED;
    }
    if (force_feedback_state.primary_output_disabled) {
        status |= MOTOR_OUTPUT_STATUS_PRIMARY_DISABLED;
    }
    if (force_feedback_state.secondary_output_disabled) {
        status |= MOTOR_OUTPUT_STATUS_SECONDARY_DISABLED;
    }
    if (usb_connection_monitor.disconnected) {
        status |= MOTOR_OUTPUT_STATUS_USB_DISCONNECTED;
    }
    return status;
}

static void service_motor_link(void) {
    if (!platform_motor_link_take_received(motor_received_frame)) {
        return;
    }

    if (motor_live_frame_decode(motor_received_frame, &motor_live_frame) ==
            MOTOR_LIVE_FRAME_VALID &&
        motor_position_report_decode(&motor_live_frame, &motor_position_report)) {
        if (!base_settings.wheel_position.calibrated &&
            wheel_position_reference_capture(
                &base_settings.wheel_position, motor_position_report.wheel_position,
                motor_identity_position_modulus(motor_probe_identity(&motor_probe)))) {
            base_settings_persistence_mark_dirty(&settings_persistence, platform_time_ms());
        }
        motor_position_ready = true;
    }

    motor_output_transport_build_frame(&motor_output_transport, motor_force_feedback_status(),
                                       (int16_t)base_settings.wheel_position.center,
                                       &motor_output_report, &motor_live_frame);
    motor_live_frame_encode(&motor_live_frame, motor_transmitted_frame);
    platform_motor_link_set_transmit(motor_transmitted_frame);
}

static void apply_active_tuning_profile(void) {
    runtime_tuning_profile = *tuning_profile_bank_active(&base_settings.tuning_profiles);
    tuning_profile = &runtime_tuning_profile;
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
 * @brief Applies a brake-force value reported by the attached V3 pedal controller.
 *
 * Updates the active runtime and retained tuning profiles when the controller reports a changed
 * alternate brake-force percentage, then schedules the shared settings record for persistence.
 *
 * @param[in] now_ms Current monotonic time in milliseconds.
 */
static void service_alternate_brake_force(uint32_t now_ms) {
    uint8_t brake_force = pedal_service_take_alternate_brake_force(&pedal_service);
    if (brake_force == PEDAL_ALTERNATE_BRAKE_FORCE_NO_UPDATE ||
        brake_force == tuning_profile->alternate_brake_force) {
        return;
    }

    runtime_tuning_profile.alternate_brake_force = brake_force;
    base_settings.tuning_profiles.slots[base_settings.tuning_profiles.active_slot]
        .alternate_brake_force = brake_force;
    base_settings_persistence_mark_dirty(&settings_persistence, now_ms);
}

/**
 * @brief Loads retained base settings and initializes their runtime consumers.
 *
 * Selects the newest valid settings record and initializes the local auxiliary input from its
 * retained endpoint calibration.
 */
static void initialize_base_settings(void) {
    base_settings_persistence_load(&settings_persistence, &base_settings, platform_time_ms());
    auxiliary_axis_init(&auxiliary_axis, &base_settings.auxiliary_axis);
}

static void initialize_motor(void) {
    force_feedback_state_init(&force_feedback_state);
    motor_tuning_context = (MotorTuningContext){
        .ramp_percent = 0,
        .strength_percent = board_identity.variant == BOARD_VARIANT_DD1 ? 40 : 32,
        .xbox_mode = 0,
        .calibration_active = 0,
    };
    motor_tuning_ready = false;
    apply_active_tuning_profile();
    motor_probe_init(&motor_probe);
    motor_probe_start(&motor_probe, platform_time_ms());
}

/**
 * @brief Initializes the host command bridge.
 *
 * Attaches report-6 mailbox storage and wheel-transfer requests to the shared type-four command
 * transport, then initializes diagnostic and tuning vendor responses.
 */
static void initialize_usb_command_bridge(void) {
    command_transport_init(&command_transport);
    (void)motor_command_mailbox_exchange_init(&motor_command_mailbox, usb_motor_mailbox_receive,
                                              sizeof(usb_motor_mailbox_receive));
    (void)usb_motor_vendor_service_init(&usb_motor_vendor_service, &usb_motor_buffers);
    wheel_transfer_service_init(&wheel_transfer_service);
    usb_diagnostic_report_service_init(&usb_diagnostic_report_service);
    usb_remote_tuning_service_init(&usb_remote_tuning_service);
    wheel_command_forwarder_init(&wheel_command_forwarder);
    usb_tuning_menu_service_init(&usb_tuning_menu_service);
    usb_tuning_profile_service_init(&usb_tuning_profile_service);
    usb_motor_acknowledgement_ready = false;
    usb_motor_response_ready = false;
    usb_remote_tuning_host_report_ready = false;
    usb_wheel_transfer_response_ready = false;
    usb_tuning_menu_response_ready = false;
    usb_tuning_profile_response_ready = false;
    for (uint8_t request = 0; request < WHEEL_TRANSFER_REQUEST_COUNT; request++) {
        usb_wheel_transfer_response_pending[request] = false;
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
    UsbMotorVendorServiceResult result = usb_motor_vendor_service_accept_usb_mailbox(
        &usb_motor_vendor_service, &motor_command_mailbox, &command_transport, report->data,
        report->length, usb_motor_acknowledgement);
    if ((result.actions & USB_MOTOR_VENDOR_ACTION_WRITE_USB) != 0) {
        usb_motor_acknowledgement_ready = true;
    }
    return (result.actions & USB_MOTOR_VENDOR_ACTION_CLAIM) != 0;
}

/**
 * @brief Advances host command services over serial message type four.
 *
 * Queues remote-tuning responses and telemetry for the attached wheel, batches generic tuning
 * records, advances wheel-transfer and mailbox requests, submits the next queued command, and
 * schedules control and vendor reports on the shared USB endpoint.
 *
 * @param[in] now_ms Current monotonic time in milliseconds.
 */
static void service_usb_command_bridge(uint32_t now_ms) {
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
    if (wheel_command_forwarder_accepting(&wheel_command_forwarder) &&
        usb_remote_tuning_service_take_forward_batch(
            &usb_remote_tuning_service, wheel_service_mode(&wheel_service), wheel_command_batch,
            &wheel_command_batch_length)) {
        (void)wheel_command_forwarder_queue(&wheel_command_forwarder, wheel_command_batch,
                                            wheel_command_batch_length);
    }
    wheel_command_forwarder_run(&wheel_command_forwarder, &command_transport);
    (void)usb_motor_vendor_service_run_mailbox(&usb_motor_vendor_service, &motor_command_mailbox,
                                               &command_transport);
    if (serial_service.status == SERIAL_SERVICE_IDLE) {
        (void)motor_command_serial_submit(&command_transport, &serial_service, now_ms);
    }

    if (usb_motor_acknowledgement_ready &&
        usb_device_send_vendor_report(usb_motor_acknowledgement)) {
        usb_motor_acknowledgement_ready = false;
    }
    if (!usb_remote_tuning_host_report_ready &&
        usb_device_operating_mode() == USB_OPERATING_MODE_FANATEC) {
        usb_remote_tuning_host_report_ready = usb_remote_tuning_service_take_host_report(
            &usb_remote_tuning_service, wheel_service_mode(&wheel_service),
            USB_REMOTE_TUNING_HOST_NATIVE, usb_remote_tuning_host_report);
    }
    if (!usb_motor_acknowledgement_ready && usb_remote_tuning_host_report_ready &&
        usb_device_send_vendor_report(usb_remote_tuning_host_report)) {
        usb_remote_tuning_host_report_ready = false;
    }
    if (!usb_motor_response_ready) {
        usb_motor_response_ready = usb_motor_vendor_service_next_response(&usb_motor_vendor_service,
                                                                          usb_motor_response) != 0;
    }
    if (!usb_wheel_transfer_response_ready) {
        WheelTransferRequest request = WHEEL_TRANSFER_READ;
        if (!usb_wheel_transfer_response_pending[request]) {
            request = WHEEL_TRANSFER_WRITE;
        }
        if (usb_wheel_transfer_response_pending[request]) {
            usb_vendor_command_encode_wheel_transfer_response(
                request, wheel_transfer_service_status(&wheel_transfer_service, request),
                usb_wheel_transfer_response);
            usb_wheel_transfer_response_pending[request] = false;
            usb_wheel_transfer_response_ready = true;
        }
    }
    if (!usb_tuning_profile_response_ready &&
        usb_tuning_profile_service_response_pending(&usb_tuning_profile_service)) {
        usb_tuning_profile_report_encode_response(&base_settings.tuning_profiles,
                                                  usb_tuning_profile_response);
        usb_tuning_profile_response_ready = true;
    }
    if (!usb_tuning_menu_response_ready &&
        usb_tuning_menu_service_response_pending(&usb_tuning_menu_service)) {
        usb_tuning_menu_service_encode_response(&usb_tuning_menu_service, usb_tuning_menu_response);
        usb_tuning_menu_response_ready = true;
    }
    update_usb_diagnostic_snapshot(now_ms);
    bool usb_diagnostic_report_ready = usb_diagnostic_report_prepare(
        &usb_diagnostic_report_service, &usb_diagnostic_snapshot, usb_diagnostic_report);
    if (!usb_motor_acknowledgement_ready && !usb_remote_tuning_host_report_ready &&
        usb_wheel_transfer_response_ready &&
        usb_device_send_vendor_report(usb_wheel_transfer_response)) {
        usb_wheel_transfer_response_ready = false;
    }
    if (!usb_motor_acknowledgement_ready && !usb_remote_tuning_host_report_ready &&
        !usb_wheel_transfer_response_ready && usb_tuning_profile_response_ready &&
        usb_device_send_vendor_report(usb_tuning_profile_response)) {
        usb_tuning_profile_response_ready = false;
        usb_tuning_profile_service_response_sent(&usb_tuning_profile_service);
    }
    if (!usb_motor_acknowledgement_ready && !usb_remote_tuning_host_report_ready &&
        !usb_wheel_transfer_response_ready && !usb_tuning_profile_response_ready &&
        usb_tuning_menu_response_ready && usb_device_send_vendor_report(usb_tuning_menu_response)) {
        usb_tuning_menu_response_ready = false;
        usb_tuning_menu_service_response_sent(&usb_tuning_menu_service);
    }
    if (!usb_motor_acknowledgement_ready && !usb_remote_tuning_host_report_ready &&
        !usb_wheel_transfer_response_ready && !usb_tuning_profile_response_ready &&
        !usb_tuning_menu_response_ready && usb_diagnostic_report_ready &&
        usb_device_send_vendor_report(usb_diagnostic_report)) {
        usb_diagnostic_report_commit(&usb_diagnostic_report_service, usb_diagnostic_report);
        usb_diagnostic_report_ready = false;
    }
    if (!usb_motor_acknowledgement_ready && !usb_remote_tuning_host_report_ready &&
        !usb_wheel_transfer_response_ready && !usb_tuning_profile_response_ready &&
        !usb_tuning_menu_response_ready && !usb_diagnostic_report_ready &&
        usb_motor_response_ready && usb_device_send_vendor_report(usb_motor_response)) {
        usb_motor_response_ready = false;
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
 * @brief Initialize force-feedback script state and its runtime clock.
 *
 * Resets the complete script runtime in position-only mode before starting the dedicated Timer 2
 * clock used for engine, slot, and wheel-motion timing.
 */
static void initialize_force_feedback_script(void) {
    force_feedback_script_runtime_init(&force_feedback_script_system);
    platform_force_feedback_timer_init(handle_force_feedback_timer_tick,
                                       &force_feedback_script_system);
}

static void service_motor(void) {
    base_settings_persistence_service(&settings_persistence, &base_settings, platform_time_ms());
    motor_probe_run(&motor_probe, platform_time_ms());
    const MotorIdentity *identity = motor_probe_identity(&motor_probe);
    if (!motor_tuning_ready && identity != 0) {
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
        motor_telemetry_service_run(&motor_telemetry_service, platform_time_ms());
        motor_status_service_run(&motor_status_service, platform_time_ms());
        motor_tuning_service_run(&motor_tuning_service);
    }
}

/**
 * @brief Forwards a host force-feedback command to the motor controller.
 *
 * Queues full seven-byte records for configuration and position-effect activation. Clear commands
 * carry only their opcode, while primary and secondary output commands are represented by status
 * bits in the next motor-link packet.
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

    case FORCE_FEEDBACK_COMMAND_SET_PRIMARY_OUTPUT:
    case FORCE_FEEDBACK_COMMAND_SET_SECONDARY_OUTPUT:
        break;
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
        base_settings_persistence_mark_dirty(&settings_persistence, platform_time_ms());
    }
}

static void service_usb_output(void) {
    if (!usb_device_take_output(&usb_device_output_report)) {
        return;
    }
    if (accept_usb_motor_report(&usb_device_output_report)) {
        return;
    }
    if (!usb_output_command_decode(&usb_device_output_report, &usb_output_command)) {
        return;
    }

    if (force_feedback_command_decode(&usb_output_command, &force_feedback_command)) {
        if (force_feedback_state_apply(
                &force_feedback_state, &force_feedback_command,
                (int32_t)wheel_position_travel_from_degrees(tuning_profile->rotation_degrees))) {
            forward_force_feedback_command(&force_feedback_command, usb_output_command.payload);
        }
        return;
    }

    if (usb_operating_mode_command_decode(&usb_output_command, &usb_operating_mode_command)) {
        if (usb_operating_mode_command_requests_native_reset(&usb_operating_mode_command)) {
            (void)usb_device_set_input_mode(USB_INPUT_REPORT_MODE_FANATEC);
        } else if (pedal_calibration_command_decode(&usb_operating_mode_command,
                                                    &pedal_calibration_command)) {
            pedal_calibration_actions = pedal_calibration_command_route(
                &pedal_calibration_command, pedal_service_calibration_active(&pedal_service),
                auxiliary_axis.active);
            apply_pedal_calibration_actions(&pedal_calibration_actions);
        } else if (pedal_protocol_command_decode(&usb_operating_mode_command,
                                                 &pedal_protocol_command)) {
            pedal_service_apply_protocol_command(&pedal_service, &pedal_protocol_command);
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
        if (usb_vendor_command.kind == USB_VENDOR_COMMAND_WHEEL_OUTPUT_REPORT) {
            wheel_service_apply_output_report(&wheel_service, usb_vendor_command.arguments, false);
            return;
        }
        const uint8_t *wheel_report_seventeen =
            usb_vendor_command_decode_wheel_report_seventeen(&usb_vendor_command);
        if (wheel_report_seventeen != 0) {
            wheel_service_queue_report_seventeen(&wheel_service, wheel_report_seventeen);
            return;
        }
        if (usb_tuning_menu_service_apply(&usb_tuning_menu_service, &usb_vendor_command)) {
            if (usb_tuning_menu_service_response_pending(&usb_tuning_menu_service)) {
                usb_tuning_menu_response_ready = false;
            }
            return;
        }
        if (usb_diagnostic_report_apply_command(&usb_diagnostic_report_service,
                                                &usb_vendor_command)) {
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
                base_settings_persistence_mark_dirty(&settings_persistence, now_ms);
            }
            if ((tuning_action & USB_TUNING_PROFILE_ACTION_SAVE) != 0) {
                base_settings_persistence_request_save(&settings_persistence, now_ms);
            }
            if (usb_tuning_profile_service_response_pending(&usb_tuning_profile_service)) {
                usb_tuning_profile_response_ready = false;
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
 * @brief Builds and submits the current USB input report.
 *
 * Combines calibrated motor position, attached-wheel controls and rotary selectors, shifter
 * state, thermal limit state, pedal axes, and pending bite-point updates into the active USB input
 * format.
 *
 * @param[in] now_ms Current monotonic time in milliseconds.
 */
static void service_usb_input(uint32_t now_ms) {
    if (!motor_position_ready) {
        return;
    }

    const MotorIdentity *motor_identity = motor_probe_identity(&motor_probe);
    usb_input_state = (UsbInputReportState){
        .fanatec =
            {
                .steering = wheel_position_hid_axis(motor_position_report.wheel_position,
                                                    &wheel_position_calibration),
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
    uint8_t wheel_controls[8];
    if (wheel_service_controls(&wheel_service, wheel_controls)) {
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
    if (wheel_service_multi_position_input(&wheel_service, now_ms, &wheel_multi_position_input)) {
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
    if (fanatec_encoder_update(&fanatec_encoder, wheel_service_encoder_direction(&wheel_service),
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
    fanatec_input_apply_wheel_axis_overrides(&usb_input_state.fanatec,
                                             wheel_service_axis_overrides(&wheel_service));
    uint8_t report_size =
        usb_input_report_encode(usb_device_input_mode(), usb_input_report, &usb_input_state);
    if (report_size != 0) {
        usb_device_send_input(usb_input_report, report_size);
    }
}

/**
 * @brief Samples and publishes all base-side analog inputs.
 *
 * Updates cooling temperatures, pedal fallback samples, the local auxiliary override, and the
 * active H-pattern shifter. Changed auxiliary endpoint settings enter the shared delayed
 * persistence path.
 *
 * @param[in] now_ms Current monotonic time in milliseconds.
 */
static void service_analog_input(uint32_t now_ms) {
    platform_shifter_read(&shifter_input);
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
            base_settings_persistence_mark_dirty(&settings_persistence, now_ms);
        }
        if (!base_settings.h_pattern_shifter.calibrated) {
            h_pattern_shifter = (HPatternShifter){0};
        } else if (shifter_input.primary_mode == SHIFTER_INPUT_H_PATTERN) {
            h_pattern_shifter_update(
                &h_pattern_shifter, &base_settings.h_pattern_shifter.calibration,
                analog_samples.primary_shifter_x, analog_samples.primary_shifter_y);
        } else if (shifter_input.secondary_mode == SHIFTER_INPUT_H_PATTERN) {
            h_pattern_shifter_update(
                &h_pattern_shifter, &base_settings.h_pattern_shifter.calibration,
                analog_samples.secondary_shifter_x, analog_samples.secondary_shifter_y);
        } else {
            h_pattern_shifter = (HPatternShifter){0};
        }
    }
}

static void service_shifter_display(uint32_t now_ms) {
    WheelDisplayOutput *output = &wheel_service.display_output;
    bool wheel_active = wheel_service_protocol_phase(&wheel_service) == WHEEL_PROTOCOL_ACTIVE;
    if (shifter_display_update(&shifter_display, h_pattern_shifter.gear, wheel_active, now_ms,
                               output)) {
        wheel_service_set_display_output(&wheel_service, output);
    }
}

/**
 * @brief Applies a force-output prompt visibility action.
 *
 * Retains the current display state when no action is requested and otherwise shows or hides the
 * torque-confirmation prompt.
 *
 * @param[in] action Requested prompt visibility transition.
 */
static void apply_force_output_prompt_action(ForceOutputEnableAction action) {
    if (action == FORCE_OUTPUT_ENABLE_ACTION_NONE) {
        return;
    }
    force_output_prompt_visible = action == FORCE_OUTPUT_ENABLE_ACTION_SHOW_PROMPT;
}

/**
 * @brief Updates the local display when its active page changes.
 *
 * Gives the torque-confirmation prompt priority over paddle bite-point adjustment. Changes to the
 * active percentage redraw the bite-point page, and leaving both states clears the display.
 */
static void service_local_display(void) {
    bool bite_point_visible =
        wheel_service_bite_point_adjustment(&wheel_service, &wheel_bite_point_display_percent);
    uint8_t page = force_output_prompt_visible ? LOCAL_DISPLAY_PAGE_TORQUE_PROMPT
                   : bite_point_visible        ? LOCAL_DISPLAY_PAGE_BITE_POINT
                                               : LOCAL_DISPLAY_PAGE_CLEAR;
    if (page == local_display_page &&
        (page != LOCAL_DISPLAY_PAGE_BITE_POINT ||
         wheel_bite_point_display_percent == local_display_rendered_bite_point_percent)) {
        return;
    }

    if (page == LOCAL_DISPLAY_PAGE_TORQUE_PROMPT) {
        display_prompt_render(display_framebuffer, true);
    } else {
        display_prompt_render_bite_point(display_framebuffer, page == LOCAL_DISPLAY_PAGE_BITE_POINT,
                                         wheel_bite_point_display_percent);
    }
    platform_display_write_frame(display_framebuffer);
    local_display_page = page;
    local_display_rendered_bite_point_percent = wheel_bite_point_display_percent;
}

static void service_force_output_enable(void) {
    WheelProtocolPhase wheel_phase = wheel_service_protocol_phase(&wheel_service);
    bool wheel_protocol_ready = wheel_phase >= WHEEL_PROTOCOL_AUTHENTICATING;
    bool usb_connected = !usb_connection_monitor.disconnected;

    if (force_output_enabled && (!wheel_protocol_ready || !usb_connected)) {
        force_output_enabled = false;
    }
    if (force_output_enabled) {
        return;
    }

    if (display_prompt_update(&display_prompt, force_output_prompt_visible,
                              wheel_service_acknowledgement_input_active(&wheel_service))) {
        force_output_enable_set_response(&force_output_enable, 1);
    }

    bool interlocked =
        force_output_enable_service(&force_output_enable, wheel_protocol_ready, usb_connected, true,
                                    &force_output_enable_action);
    apply_force_output_prompt_action(force_output_enable_action);
    force_output_enabled = !interlocked;
}

int main(void) {
    platform_clock_init();
    board_identity = platform_board_identity_read();
    platform_pin_mux_init();
    platform_time_init();
    platform_display_init();
    platform_display_write_frame(display_framebuffer);
    platform_status_led_init();
    status_led_init(&status_led);
    initialize_cooling();
    platform_adc_init();
    platform_shifter_init();
    platform_shifter_read(&shifter_input);
    shifter_display_init(&shifter_display);
    platform_aux_bus_init();
    platform_pedal_link_init();
    pedal_service_init(&pedal_service);
    pedal_brake_indicator_init(&pedal_brake_indicator);
    platform_serial_link_init();
    serial_service_init(&serial_service);
    wheel_service_init(&wheel_service, &serial_service);
    fanatec_encoder_init(&fanatec_encoder);
    wheel_status_service_init(&wheel_status_service, &serial_service);
    initialize_usb_command_bridge();
    wheel_velocity_reset(&wheel_velocity_estimator);
    initialize_motor_link();
    initialize_base_settings();
    initialize_motor();
    initialize_force_feedback_script();
    usb_device_init(board_identity.variant);
    usb_connection_monitor_init(&usb_connection_monitor);
    for (;;) {
        usb_device_service();
        service_usb_output();
        platform_aux_bus_service();
        uint32_t now_ms = platform_time_ms();
        (void)usb_connection_monitor_update(&usb_connection_monitor, platform_usb_connected(), true,
                                            now_ms);
        platform_status_led_set(status_led_update(&status_led, now_ms));
        platform_cooling_service(now_ms);
        update_fan_speed(PLATFORM_FAN_PRIMARY);
        update_fan_speed(PLATFORM_FAN_SECONDARY);
        service_analog_input(now_ms);
        service_motor_link();
        pedal_service_run(&pedal_service, now_ms);
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
        wheel_position_calibration = wheel_position_calibration_build(
            &base_settings.wheel_position, tuning_profile->rotation_degrees,
            tuning_profile->steering_deadzone);
        wheel_service_set_display_rotation(
            &wheel_service, tuning_profile->display_rotation_enabled != 0,
            wheel_position_display_rotation(motor_position_report.wheel_position,
                                            &wheel_position_calibration));
        wheel_service_run(&wheel_service, now_ms, !serial_command_waiting());
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
        service_force_output_enable();
        service_local_display();
        service_shifter_display(now_ms);
        service_usb_input(now_ms);
        service_motor();
        service_cooling(now_ms);
    }
}
