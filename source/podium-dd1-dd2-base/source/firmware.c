#include <stdbool.h>
#include <xc.h>

#include "board/identity.h"
#include "force_feedback/output.h"
#include "motor/live_frame.h"
#include "motor/probe.h"
#include "motor/telemetry_service.h"
#include "motor/tuning_service.h"
#include "platform/adc.h"
#include "platform/aux_bus.h"
#include "platform/board_identity.h"
#include "platform/clock.h"
#include "platform/motor_link.h"
#include "platform/pin_mux.h"
#include "platform/time.h"
#include "profile/tuning.h"

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
static TuningProfile tuning_profile;
static MotorTuningContext motor_tuning_context;
static bool motor_tuning_ready;
static MotorPositionReport motor_position_report;
static bool motor_position_ready;
static ForceOutputCommand motor_output_command;
static MotorLiveFrame motor_live_frame;
static uint8_t motor_received_frame[MOTOR_LIVE_FRAME_SIZE];
static uint8_t motor_transmitted_frame[MOTOR_LIVE_FRAME_SIZE];

static void initialize_motor_link(void) {
    motor_output_command = (ForceOutputCommand){0};
    motor_force_frame_init(0, &motor_output_command, 0, &motor_live_frame);
    motor_live_frame_encode(&motor_live_frame, motor_transmitted_frame);
    platform_motor_link_init(motor_transmitted_frame);
    motor_position_ready = false;
}

static void service_motor_link(void) {
    if (platform_motor_link_take_received(motor_received_frame) &&
        motor_live_frame_decode(motor_received_frame, &motor_live_frame) ==
            MOTOR_LIVE_FRAME_VALID &&
        motor_position_report_decode(&motor_live_frame, &motor_position_report)) {
        motor_position_ready = true;
    }
}

static void initialize_motor(void) {
    tuning_profile_defaults(&tuning_profile);
    motor_tuning_context = (MotorTuningContext){
        .automatic_rotation_degrees = tuning_profile.rotation_degrees,
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
    motor_probe_run(&motor_probe);
    const MotorIdentity *identity = motor_probe_identity(&motor_probe);
    if (!motor_tuning_ready && identity != 0) {
        motor_telemetry_service_init(&motor_telemetry_service, identity);
        motor_tuning_service_init(&motor_tuning_service, &tuning_profile, &motor_tuning_context);
        motor_tuning_ready = true;
    }
    if (motor_tuning_ready) {
        motor_telemetry_service_run(&motor_telemetry_service, platform_time_ms());
        motor_tuning_service_run(&motor_tuning_service);
    }
}

int main(void) {
    platform_clock_init();
    board_identity = platform_board_identity_read();
    platform_pin_mux_init();
    platform_time_init();
    platform_adc_init();
    platform_aux_bus_init();
    initialize_motor_link();
    initialize_motor();
    for (;;) {
        platform_aux_bus_service();
        service_motor_link();
        service_motor();
    }
}
