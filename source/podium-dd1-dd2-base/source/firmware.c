#include <stdbool.h>
#include <xc.h>

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
#include "pedal/input.h"
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
#include "usb/fanatec_input.h"
#include "usb/input_report.h"
#include "usb/motor_vendor_service.h"
#include "usb/operating_mode_command.h"
#include "usb/output_command.h"
#include "usb/vendor_command.h"
#include "wheel/position.h"
#include "wheel/service.h"
#include "wheel/transfer_service.h"

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
static PedalService pedal_service;
static SerialService serial_service;
static WheelService wheel_service;
static AnalogSamples analog_samples;
static HPatternShifter h_pattern_shifter;
static ShifterInputState shifter_input;
static ShifterDisplay shifter_display;
static UsbInputReportState usb_input_state;
static uint8_t usb_input_report[USB_INPUT_REPORT_MAX_SIZE];
static uint8_t usb_motor_acknowledgement[USB_DEVICE_REPORT_SIZE];
static uint8_t usb_motor_response[USB_DEVICE_REPORT_SIZE];
static uint8_t usb_wheel_transfer_response[USB_DEVICE_REPORT_SIZE];
static UsbDeviceOutputReport usb_device_output_report;
static UsbConnectionMonitor usb_connection_monitor;
static UsbOutputCommand usb_output_command;
static UsbOperatingModeCommand usb_operating_mode_command;
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
static bool usb_wheel_transfer_response_ready;
static bool usb_wheel_transfer_response_pending[WHEEL_TRANSFER_REQUEST_COUNT];

enum {
    FAN_STARTUP_DUTY_PERCENT = 25,
    USB_MOTOR_BUFFER_SIZE = MEMORY_TRANSFER_MAX_READ_SIZE,
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
            wheel_position_reference_capture(&base_settings.wheel_position,
                                             motor_position_report.wheel_position)) {
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

static void initialize_motor(void) {
    base_settings_persistence_load(&settings_persistence, &base_settings, platform_time_ms());
    force_feedback_state_init(&force_feedback_state);
    runtime_tuning_profile = *tuning_profile_bank_active(&base_settings.tuning_profiles);
    tuning_profile = &runtime_tuning_profile;
    cooling_effect_strengths = (CoolingEffectStrengths){
        .force = tuning_profile->force_effect_strength,
        .spring = tuning_profile->spring_effect_strength,
        .damper = tuning_profile->damper_effect_strength,
    };
    pedal_service_set_brake_force(&pedal_service, tuning_profile->brake_force);
    PedalV4Tuning pedal_tuning = {
        .brake_force = tuning_profile->brake_force,
        .clutch_curve = (uint8_t)tuning_profile->clutch_pedal_curve,
        .brake_curve = (uint8_t)tuning_profile->brake_pedal_curve,
        .throttle_curve = (uint8_t)tuning_profile->throttle_pedal_curve,
    };
    pedal_service_set_v4_tuning(&pedal_service, pedal_tuning);
    motor_tuning_context = (MotorTuningContext){
        .automatic_rotation_degrees = tuning_profile->rotation_degrees,
        .ramp_percent = 0,
        .strength_percent = board_identity.variant == BOARD_VARIANT_DD1 ? 40 : 32,
        .xbox_mode = 0,
        .calibration_active = 0,
    };
    motor_probe_init(&motor_probe);
    motor_probe_start(&motor_probe, platform_time_ms());
    motor_tuning_ready = false;
}

/**
 * @brief Initializes the host command bridge.
 *
 * Attaches report-6 mailbox storage and wheel-transfer requests to the shared type-four command
 * transport.
 */
static void initialize_usb_command_bridge(void) {
    command_transport_init(&command_transport);
    (void)motor_command_mailbox_exchange_init(&motor_command_mailbox, usb_motor_mailbox_receive,
                                              sizeof(usb_motor_mailbox_receive));
    (void)usb_motor_vendor_service_init(&usb_motor_vendor_service, &usb_motor_buffers);
    wheel_transfer_service_init(&wheel_transfer_service);
    usb_motor_acknowledgement_ready = false;
    usb_motor_response_ready = false;
    usb_wheel_transfer_response_ready = false;
    for (uint8_t request = 0; request < WHEEL_TRANSFER_REQUEST_COUNT; request++) {
        usb_wheel_transfer_response_pending[request] = false;
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
 * Applies completed type-four responses, advances wheel-transfer and mailbox requests, submits the
 * next queued command, and schedules their vendor reports on the shared USB endpoint.
 *
 * @param[in] now_ms Current monotonic time in milliseconds.
 */
static void service_usb_command_bridge(uint32_t now_ms) {
    (void)motor_command_serial_receive(&command_transport, &serial_service);
    wheel_transfer_service_run(&wheel_transfer_service, &command_transport);
    (void)usb_motor_vendor_service_run_mailbox(&usb_motor_vendor_service, &motor_command_mailbox,
                                               &command_transport);
    if (serial_service.status == SERIAL_SERVICE_IDLE) {
        (void)motor_command_serial_submit(&command_transport, &serial_service, now_ms);
    }

    if (usb_motor_acknowledgement_ready &&
        usb_device_send_vendor_report(usb_motor_acknowledgement)) {
        usb_motor_acknowledgement_ready = false;
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
    if (!usb_motor_acknowledgement_ready && usb_wheel_transfer_response_ready &&
        usb_device_send_vendor_report(usb_wheel_transfer_response)) {
        usb_wheel_transfer_response_ready = false;
    }
    if (!usb_motor_acknowledgement_ready && !usb_wheel_transfer_response_ready &&
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
        }
        return;
    }

    if (usb_vendor_command_decode(&usb_output_command, &usb_vendor_command)) {
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

static void service_usb_input(void) {
    if (!motor_position_ready) {
        return;
    }

    wheel_position_calibration = wheel_position_calibration_build(
        &base_settings.wheel_position, tuning_profile->rotation_degrees,
        tuning_profile->steering_deadzone);
    usb_input_state = (UsbInputReportState){
        .fanatec =
            {
                .steering = wheel_position_hid_axis(motor_position_report.wheel_position,
                                                    &wheel_position_calibration),
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
        fanatec_input_apply_wheel_controls(&usb_input_state.fanatec, wheel_controls,
                                           wheel_service_mode(&wheel_service) !=
                                               WHEEL_MODE_CRC_AUTHENTICATED);
    }
    const uint8_t *wheel_buttons = wheel_service_buttons(&wheel_service);
    for (uint8_t bank = 0; bank < WHEEL_BUTTON_BANK_COUNT; bank++) {
        usb_input_state.fanatec.button_banks[bank] = wheel_buttons[bank];
    }
    usb_input_state.fanatec.encoder_delta = wheel_service_take_encoder_delta(&wheel_service);
    fanatec_input_apply_shifter(&usb_input_state.fanatec, &shifter_input, h_pattern_shifter.gear);
    const PedalInput *pedal_input = pedal_service_input(&pedal_service);
    for (uint8_t axis = 0; axis < FANATEC_INPUT_PEDAL_AXES; axis++) {
        usb_input_state.fanatec.pedals[axis] = pedal_input_hid_axis(pedal_input->axes[axis]);
    }
    usb_input_state.fanatec.auxiliary_pedal = pedal_input_hid_auxiliary(pedal_input->auxiliary);
    uint8_t report_size =
        usb_input_report_encode(usb_device_input_mode(), usb_input_report, &usb_input_state);
    if (report_size != 0) {
        usb_device_send_input(usb_input_report, report_size);
    }
}

static void service_analog_input(void) {
    platform_shifter_read(&shifter_input);
    if (platform_adc_read(&analog_samples)) {
        cooling_temperature_monitor_add(&cooling_temperature_monitor,
                                        analog_samples.primary_thermistor,
                                        analog_samples.secondary_thermistor);
        pedal_service_set_analog_samples(&pedal_service, analog_samples.pedal_axes);
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

static void apply_force_output_prompt_action(ForceOutputEnableAction action) {
    if (action == FORCE_OUTPUT_ENABLE_ACTION_NONE) {
        return;
    }
    force_output_prompt_visible = action == FORCE_OUTPUT_ENABLE_ACTION_SHOW_PROMPT;
    display_prompt_render(display_framebuffer, force_output_prompt_visible);
    platform_display_write_frame(display_framebuffer);
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
    platform_serial_link_init();
    serial_service_init(&serial_service);
    wheel_service_init(&wheel_service, &serial_service);
    initialize_usb_command_bridge();
    initialize_motor_link();
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
        service_analog_input();
        service_motor_link();
        pedal_service_run(&pedal_service, now_ms);
        serial_service_run(&serial_service, now_ms);
        service_usb_command_bridge(now_ms);
        wheel_service_run(&wheel_service, now_ms, !serial_command_waiting());
        if (serial_service.status == SERIAL_SERVICE_IDLE) {
            (void)motor_command_serial_submit(&command_transport, &serial_service, now_ms);
        }
        service_force_output_enable();
        service_shifter_display(now_ms);
        service_usb_input();
        service_motor();
        service_cooling(now_ms);
    }
}
