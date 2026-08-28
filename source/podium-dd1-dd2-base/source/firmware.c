#include <stdbool.h>
#include <xc.h>

#include "board/identity.h"
#include "board/status_led.h"
#include "cooling/fan.h"
#include "force_feedback/output.h"
#include "motor/live_frame.h"
#include "motor/probe.h"
#include "motor/telemetry_service.h"
#include "motor/tuning_service.h"
#include "pedal/input.h"
#include "pedal/service.h"
#include "platform/adc.h"
#include "platform/aux_bus.h"
#include "platform/board_identity.h"
#include "platform/clock.h"
#include "platform/cooling.h"
#include "platform/motor_link.h"
#include "platform/pedal_link.h"
#include "platform/pin_mux.h"
#include "platform/shifter.h"
#include "platform/status_led.h"
#include "platform/time.h"
#include "platform/wheel_link.h"
#include "profile/bank.h"
#include "profile/tuning.h"
#include "settings/persistence.h"
#include "settings/state.h"
#include "shifter/display.h"
#include "shifter/h_pattern.h"
#include "shifter/input.h"
#include "usb/device.h"
#include "usb/fanatec_input.h"
#include "wheel/position.h"
#include "wheel/service.h"

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
static MotorTelemetryService motor_telemetry_service;
static MotorTuningService motor_tuning_service;
static BaseSettings base_settings;
static BaseSettingsPersistence settings_persistence;
static const TuningProfile *tuning_profile;
static MotorTuningContext motor_tuning_context;
static bool motor_tuning_ready;
static MotorPositionReport motor_position_report;
static bool motor_position_ready;
static WheelPositionCalibration wheel_position_calibration;
static PedalService pedal_service;
static WheelService wheel_service;
static AnalogSamples analog_samples;
static HPatternShifter h_pattern_shifter;
static ShifterInputState shifter_input;
static ShifterDisplay shifter_display;
static fanatec_input_state usb_input_state;
static uint8_t usb_input_report[FANATEC_INPUT_REPORT_SIZE];
static ForceOutputReport motor_output_report;
static MotorLiveFrame motor_live_frame;
static uint8_t motor_received_frame[MOTOR_LIVE_FRAME_SIZE];
static uint8_t motor_transmitted_frame[MOTOR_LIVE_FRAME_SIZE];
static FanController fan_controller;
static StatusLed status_led;
static PlatformFanTachometer fan_tachometer;
static uint16_t fan_speed_rpm[2];
static uint32_t next_cooling_update_ms;

enum {
    COOLING_UPDATE_INTERVAL_MS = 1000,
    FAN_TACHOMETER_TIMER_HZ = 60000000,
    FAN_TACHOMETER_PULSES_PER_REVOLUTION = 2,
};

static void set_cooling_duty(uint8_t duty_percent) {
    if (board_identity.mode_bits == 7) {
        platform_cooling_set_duty(duty_percent, duty_percent);
    } else {
        platform_cooling_set_duty(duty_percent, 0);
    }
}

static void initialize_cooling(void) {
    fan_controller_init(&fan_controller);
    fan_speed_rpm[PLATFORM_FAN_PRIMARY] = 0;
    fan_speed_rpm[PLATFORM_FAN_SECONDARY] = 0;
    next_cooling_update_ms = 0;
    platform_cooling_init(board_identity.mode_bits != 7);
    set_cooling_duty(fan_controller.duty_percent);
}

static void update_fan_speed(PlatformFan fan) {
    if (!platform_cooling_take_tachometer(fan, &fan_tachometer)) {
        return;
    }

    fan_speed_rpm[fan] =
        fan_tachometer.present
            ? fan_tachometer_rpm(fan_tachometer.previous_capture, fan_tachometer.current_capture,
                                 FAN_TACHOMETER_TIMER_HZ, FAN_TACHOMETER_PULSES_PER_REVOLUTION)
            : 0;
}

static void service_cooling(uint32_t now_ms) {
    update_fan_speed(PLATFORM_FAN_PRIMARY);
    update_fan_speed(PLATFORM_FAN_SECONDARY);
    if (!platform_time_reached(now_ms, next_cooling_update_ms)) {
        return;
    }

    const MotorTelemetry *telemetry =
        motor_tuning_ready ? motor_telemetry_service_value(&motor_telemetry_service) : 0;
    bool temperature_valid = telemetry != 0 && telemetry->motor_temperature_valid;
    int16_t temperature = temperature_valid ? (int16_t)telemetry->motor_temperature : 0;
    uint8_t duty = fan_controller_update(&fan_controller, temperature, temperature_valid, true);
    set_cooling_duty(duty);
    next_cooling_update_ms = now_ms + COOLING_UPDATE_INTERVAL_MS;
}

static void initialize_motor_link(void) {
    motor_output_report = (ForceOutputReport){0};
    motor_live_force_frame_init(0, &motor_output_report, &motor_live_frame);
    motor_live_frame_encode(&motor_live_frame, motor_transmitted_frame);
    platform_motor_link_init(motor_transmitted_frame);
    motor_position_ready = false;
}

static void service_motor_link(void) {
    if (platform_motor_link_take_received(motor_received_frame) &&
        motor_live_frame_decode(motor_received_frame, &motor_live_frame) ==
            MOTOR_LIVE_FRAME_VALID &&
        motor_position_report_decode(&motor_live_frame, &motor_position_report)) {
        if (!base_settings.wheel_position.calibrated &&
            wheel_position_reference_capture(&base_settings.wheel_position,
                                             motor_position_report.wheel_position)) {
            base_settings_persistence_mark_dirty(&settings_persistence, platform_time_ms());
        }
        motor_position_ready = true;
    }
}

static void initialize_motor(void) {
    base_settings_persistence_load(&settings_persistence, &base_settings, platform_time_ms());
    tuning_profile = tuning_profile_bank_active(&base_settings.tuning_profiles);
    pedal_service_set_brake_force(&pedal_service, tuning_profile->brake_force);
    motor_tuning_context = (MotorTuningContext){
        .automatic_rotation_degrees = tuning_profile->rotation_degrees,
        .ramp_percent = 0,
        .strength_percent = board_identity.variant == BOARD_VARIANT_DD1 ? 40 : 32,
        .xbox_mode = 0,
        .calibration_active = 0,
    };
    motor_probe_init(&motor_probe);
    motor_probe_start(&motor_probe);
    motor_tuning_ready = false;
}

static void service_motor(void) {
    base_settings_persistence_service(&settings_persistence, &base_settings, platform_time_ms());
    motor_probe_run(&motor_probe);
    const MotorIdentity *identity = motor_probe_identity(&motor_probe);
    if (!motor_tuning_ready && identity != 0) {
        motor_telemetry_service_init(&motor_telemetry_service, identity);
        motor_tuning_service_init(&motor_tuning_service, tuning_profile, &motor_tuning_context);
        motor_tuning_ready = true;
    }
    if (motor_tuning_ready) {
        motor_telemetry_service_run(&motor_telemetry_service, platform_time_ms());
        motor_tuning_service_run(&motor_tuning_service);
    }
}

static void service_usb_input(void) {
    if (!motor_position_ready) {
        return;
    }

    wheel_position_calibration = wheel_position_calibration_build(
        &base_settings.wheel_position, tuning_profile->rotation_degrees,
        tuning_profile->steering_deadzone);
    usb_input_state = (fanatec_input_state){
        .steering = wheel_position_hid_axis(motor_position_report.wheel_position,
                                            &wheel_position_calibration),
    };
    const uint8_t *wheel_buttons = wheel_service_buttons(&wheel_service);
    for (uint8_t bank = 0; bank < WHEEL_BUTTON_BANK_COUNT; bank++) {
        usb_input_state.button_banks[bank] = wheel_buttons[bank];
    }
    fanatec_input_apply_shifter(&usb_input_state, &shifter_input, h_pattern_shifter.gear);
    const PedalInput *pedal_input = pedal_service_input(&pedal_service);
    for (uint8_t axis = 0; axis < FANATEC_INPUT_PEDAL_AXES; axis++) {
        usb_input_state.pedals[axis] = pedal_input_hid_axis(pedal_input->axes[axis]);
    }
    usb_input_state.auxiliary_pedal = pedal_input_hid_auxiliary(pedal_input->auxiliary);
    if (fanatec_input_encode(usb_input_report, &usb_input_state)) {
        usb_device_send_input(usb_input_report, sizeof(usb_input_report));
    }
}

static void service_analog_input(void) {
    platform_shifter_read(&shifter_input);
    if (platform_adc_read(&analog_samples)) {
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

int main(void) {
    platform_clock_init();
    board_identity = platform_board_identity_read();
    platform_pin_mux_init();
    platform_time_init();
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
    platform_wheel_link_init();
    wheel_service_init(&wheel_service);
    initialize_motor_link();
    initialize_motor();
    usb_device_init(board_identity.variant);
    for (;;) {
        usb_device_service();
        platform_aux_bus_service();
        uint32_t now_ms = platform_time_ms();
        platform_status_led_set(status_led_update(&status_led, now_ms));
        platform_cooling_service(now_ms);
        service_analog_input();
        service_motor_link();
        pedal_service_run(&pedal_service, now_ms);
        wheel_service_run(&wheel_service, now_ms);
        service_shifter_display(now_ms);
        service_usb_input();
        service_motor();
        service_cooling(now_ms);
    }
}
