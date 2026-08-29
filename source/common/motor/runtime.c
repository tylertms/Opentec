#include "common/motor/runtime.h"

#include <fsl_common.h>
#include <fsl_ftm.h>
#include <fsl_gpio.h>
#include <fsl_wdog.h>
#include <gflib.h>
#include <limits.h>
#include <mlib.h>
#include <string.h>

#include "common/motor/board.h"
#include "common/motor/calibration.h"
#include "common/motor/center.h"
#include "common/motor/communication.h"
#include "common/motor/control.h"
#include "common/motor/encoder.h"
#include "common/motor/encoder_calibration.h"
#include "common/motor/foc.h"
#include "common/motor/io.h"
#include "common/motor/link.h"
#include "common/motor/motion.h"
#include "common/motor/parameter.h"
#include "common/motor/peripherals.h"
#include "common/motor/product.h"
#include "common/motor/profile.h"
#include "common/motor/protocol.h"
#include "common/motor/service.h"
#include "common/motor/spi.h"
#include "common/motor/storage.h"
#include "common/motor/telemetry.h"
#include "common/motor/velocity_control.h"

enum {
    MOTOR_PARAMETER_DIRECTION_COMMAND = 5,
    MOTOR_PARAMETER_CALIBRATION_COMMAND = 6,
    MOTOR_PARAMETER_CALIBRATION_VERSION = 7,
    MOTOR_PARAMETER_TORQUE = 16,
    MOTOR_PARAMETER_SERVICE_TICK = 17,
    MOTOR_PARAMETER_MOTOR_TEMPERATURE = 18,
    MOTOR_PARAMETER_DRIVER_TEMPERATURE = 19,
    MOTOR_PARAMETER_DRIVE_CURRENT = 20,
    MOTOR_PARAMETER_STEERING_RANGE = 32,
    MOTOR_PARAMETER_OVERALL_GAIN = 33,
    MOTOR_PARAMETER_MINIMUM_CURRENT_MODE = 34,
    MOTOR_PARAMETER_NATURAL_DAMPING = 35,
    MOTOR_PARAMETER_NATURAL_FRICTION = 36,
    MOTOR_PARAMETER_NATURAL_INERTIA = 37,
    MOTOR_PARAMETER_INTERPOLATION = 38,
    MOTOR_PARAMETER_FILTER = 39,
    MOTOR_PARAMETER_CONSTANT_GAIN = 40,
    MOTOR_PARAMETER_WINDOW_GAIN = 41,
    MOTOR_PARAMETER_DIRECTIONAL_GAIN = 42,
};

typedef struct {
    MotorHardwareProfile hardware;
    MotorParameterBank parameters;
    MotorProtocolState protocol;
    MotorFocState foc;
    MotorFocOutput foc_output;
    MotorVelocityControlState velocity_control;
    MotorAdcSample adc_sample;
    MotorCurrentCalibrationState current_calibration;
    MotorEncoderState encoder;
    MotorEncoderCalibrationState encoder_calibration;
    MotorEncoderDirectionState encoder_direction;
    MotorMotionState motion;
    MotorMotionFilter position_filter;
    MotorMotionFilter velocity_filter;
    MotorMotionSample motion_sample;
    MotorServiceTiming timing;
    MotorAuxiliaryAccumulator auxiliary_accumulator;
    MotorAuxiliaryTelemetry auxiliary_telemetry;
    MotorSpiTransferBuffers spi;
    MotorDriveCommand live_drive;
    MotorDriveInterpolationState drive_interpolation;
    MotorDriveFrictionState drive_friction;
    MotorDriveDeratingState drive_derating;
    MotorDriveOverspeedState drive_overspeed;
    GFLIB_CTRL_PI_P_AW_T_A32 derating_controller;
    MotorControlMode mode;
    uint32_t service_tick;
    int16_t electrical_angle;
    int16_t control_current;
    int16_t friction_current;
    uint8_t identity;
    uint8_t derating_update_count;
    bool_t derating_stop_integrator;
    bool control_update_pending;
    bool current_calibration_started;
    bool calibration_valid;
    bool correction_reverse;
    bool encoder_zero_captured;
    volatile bool encoder_index_detected;
} MotorRuntime;

static MotorRuntime motor_runtime;

/**
 * @brief Stops motor output after an unrecoverable runtime failure.
 *
 * Interrupts remain disabled, the first startup interlock is asserted, all PWM outputs are masked,
 * and the watchdog is refreshed while the controller remains in its safe state.
 */
_Noreturn static void motor_runtime_fault(void) {
    (void)DisableGlobalIRQ();
    for (;;) {
        GPIO_PortClear(GPIOC, 1UL << 1U);
        FTM0->OUTMASK = 0x3fU;
        WDOG_Refresh(WDOG);
    }
}

/**
 * @brief Applies the live parameter bank to force-feedback processing.
 *
 * Only the six settings consumed by the official live refresh path are transferred. The filter
 * reconfiguration is idempotent when its parameter did not change.
 *
 * @param context Active motor runtime supplied by the parameter bus callback.
 */
static void motor_runtime_settings_apply(void *context) {
    MotorRuntime *runtime = context;
    MotorForceFeedbackSettings *settings = &runtime->protocol.force_feedback.settings;
    motor_force_feedback_settings_apply(
        settings, (int8_t)runtime->parameters.entries[MOTOR_PARAMETER_STEERING_RANGE].value,
        (uint8_t)runtime->parameters.entries[MOTOR_PARAMETER_OVERALL_GAIN].value,
        (uint8_t)runtime->parameters.entries[MOTOR_PARAMETER_FILTER].value,
        (uint8_t)runtime->parameters.entries[MOTOR_PARAMETER_CONSTANT_GAIN].value,
        (uint8_t)runtime->parameters.entries[MOTOR_PARAMETER_WINDOW_GAIN].value,
        (uint8_t)runtime->parameters.entries[MOTOR_PARAMETER_DIRECTIONAL_GAIN].value);
    motor_force_feedback_filter_configure(&runtime->protocol.force_feedback.filter,
                                          settings->filter_setting);
}

/**
 * @brief Applies the latest resolved motor-link drive command.
 *
 * The signed primary and secondary current fields become the live run-mode command. Both current
 * controllers receive the coefficient pair selected by the official output path.
 *
 * @param runtime Active motor runtime and protocol state.
 */
static void motor_runtime_drive_apply(MotorRuntime *runtime) {
    runtime->live_drive = runtime->protocol.live_drive;
    runtime->protocol.live_drive_updated = false;
    runtime->foc.d_controller.a32PGain = runtime->live_drive.controller_coefficient;
    runtime->foc.d_controller.a32IGain = runtime->live_drive.controller_scale;
    runtime->foc.q_controller.a32PGain = runtime->live_drive.controller_coefficient;
    runtime->foc.q_controller.a32IGain = runtime->live_drive.controller_scale;
}

/**
 * @brief Refreshes the extended encoder position from FTM2.
 *
 * A pending quadrature overflow defers publication until the overflow handler updates the
 * revolution offset. Positions outside the board-selected safety range enter the safe state.
 *
 * @param runtime Active motor runtime and encoder state.
 */
static void motor_runtime_encoder_position_refresh(MotorRuntime *runtime) {
    bool overflow_pending = (FTM2->SC & FTM_SC_TOF_MASK) != 0U;
    MotorEncoderPositionResult result =
        motor_encoder_position_update(&runtime->encoder, overflow_pending, (uint16_t)FTM2->CNT,
                                      (int32_t)runtime->hardware.position_limit);
    if (result == kMotorEncoderPositionOutOfRange) {
        motor_runtime_fault();
    }
}

/**
 * @brief Resolves the run-mode current command.
 *
 * Primary and secondary link currents are combined with natural effects and product-specific
 * derating. A valid indexed encoder adds the directional calibration correction selected by the
 * motion hysteresis.
 *
 * @param runtime Active motor runtime and latest link, motion, and calibration state.
 * @return Saturated current command for the normal FOC cycle.
 */
static int16_t motor_runtime_current_resolve(MotorRuntime *runtime) {
    uint8_t interpolation_setting =
        (uint8_t)runtime->parameters.entries[MOTOR_PARAMETER_INTERPOLATION].value;
    int16_t primary_current = interpolation_setting < MOTOR_DRIVE_INTERPOLATION_SETTING_COUNT
                                  ? runtime->drive_interpolation.output
                                  : runtime->live_drive.primary_current;
    int16_t damping = motor_drive_motion_resistance_resolve(
        runtime->motion_sample.filtered_position_delta,
        (uint8_t)runtime->parameters.entries[MOTOR_PARAMETER_NATURAL_DAMPING].value);
    int16_t current = MLIB_SubSat_F16(primary_current, damping);
    current = MLIB_SubSat_F16(current, runtime->friction_current);
    current = MLIB_AddSat_F16(current, runtime->live_drive.secondary_current);
    int16_t inertia = motor_drive_motion_resistance_resolve(
        runtime->motion_sample.filtered_velocity_delta,
        (uint8_t)runtime->parameters.entries[MOTOR_PARAMETER_NATURAL_INERTIA].value);
    current = MLIB_SubSat_F16(current, inertia);
    current = motor_drive_product_scale(
        &runtime->drive_derating, current, runtime->auxiliary_telemetry.motor_average,
        motor_product_configuration.normal_current_scale,
        motor_product_configuration.minimum_current_scale,
        (uint8_t)runtime->parameters.entries[MOTOR_PARAMETER_MINIMUM_CURRENT_MODE].value == 0xaaU);
    current = motor_drive_overspeed_apply(&runtime->drive_overspeed, current,
                                          runtime->motion_sample.filtered_position_delta);
    if (!runtime->calibration_valid || !runtime->encoder_index_detected) {
        return current;
    }

    runtime->correction_reverse = motor_encoder_correction_direction_update(
        runtime->correction_reverse, runtime->motion_sample.filtered_position_delta);
    uint16_t relative_position = motor_encoder_relative_position(
        (uint16_t)FTM2->CNT, (uint16_t)runtime->encoder.zero_counter,
        (uint16_t)runtime->hardware.encoder_modulus);
    int16_t correction = motor_encoder_correction_read(
        &runtime->encoder_calibration.record, runtime->correction_reverse, relative_position,
        runtime->hardware.correction_table_length);
    return MLIB_AddSat_F16(current, correction);
}

/**
 * @brief Runs one synchronized field-oriented current-control cycle.
 *
 * Startup alignment fixes the rotor vector and drives only the D axis. Normal operation uses the
 * measured electrical angle and the resolved D/Q current reference. The cycle also advances the
 * alternating temperature sample pair.
 *
 * @param runtime Active motor runtime and persistent control state.
 * @param torque_current Signed current command for this cycle.
 * @param rotor_aligned True during the startup rotor-alignment ramp.
 */
static void motor_runtime_control_cycle(MotorRuntime *runtime, int16_t torque_current,
                                        bool rotor_aligned) {
    (void)motor_adc_read(ADC0, ADC1, &runtime->current_calibration.offsets, &runtime->adc_sample);

    MotorFocInput input = {
        .phase_current = runtime->adc_sample.phase_current,
        .dc_bus_voltage = runtime->adc_sample.dc_bus_voltage,
    };
    if (rotor_aligned) {
        input.current_reference.f16D = torque_current;
        input.current_reference.f16Q = 0;
        input.rotor_sin_cos.f16Sin = 0;
        input.rotor_sin_cos.f16Cos = INT16_MAX;
    } else {
        MotorControlCurrentReference reference = motor_control_current_reference(torque_current);
        input.current_reference.f16D = reference.d;
        input.current_reference.f16Q = reference.q;
        GFLIB_SinCos_F16(runtime->electrical_angle, &input.rotor_sin_cos);
    }

    motor_foc_step(&runtime->foc, &input, &runtime->foc_output);
    motor_pwm_write(FTM0, &runtime->foc_output.duty);

    MotorAdcAuxiliarySamples auxiliary;
    if (motor_adc_auxiliary_cycle(&auxiliary)) {
        (void)motor_auxiliary_samples_accumulate(&runtime->auxiliary_accumulator, auxiliary.motor,
                                                 auxiliary.driver);
    }
}

/**
 * @brief Starts a fresh encoder-index search.
 *
 * The one-shot PORTE interrupt is re-enabled and its five-thousand-tick timeout is restored.
 *
 * @param runtime Active motor runtime and service countdown state.
 */
static void motor_runtime_index_seek_restart(MotorRuntime *runtime) {
    runtime->encoder_index_detected = false;
    runtime->timing.countdowns[kMotorCountdownEncoderIndex].ticks = 5000U;
    runtime->timing.countdowns[kMotorCountdownEncoderIndex].active = 0U;
    motor_encoder_index_interrupt_enable();
}

/**
 * @brief Advances the two active-low startup interlock delays.
 *
 * Interlock A remains asserted for two hundred service ticks. Interlock B is asserted after the
 * following ten-tick release delay, then current calibration begins.
 *
 * @param runtime Active motor runtime and startup countdowns.
 */
static void motor_runtime_interlock_step(MotorRuntime *runtime) {
    if (runtime->mode == kMotorControlStartupInterlockA) {
        MotorCountdown *countdown = &runtime->timing.countdowns[kMotorCountdownStartupInterlockA];
        motor_startup_interlock_outputs_apply(true, false);
        countdown->active = 1U;
        if (countdown->ticks == 0U) {
            countdown->active = 0U;
            motor_startup_interlock_outputs_apply(false, false);
            runtime->mode = motor_control_mode_complete(runtime->mode);
        }
        return;
    }

    MotorCountdown *countdown = &runtime->timing.countdowns[kMotorCountdownStartupInterlockB];
    motor_startup_interlock_outputs_apply(false, false);
    countdown->active = 1U;
    if (countdown->ticks == 0U) {
        countdown->active = 0U;
        motor_startup_interlock_outputs_apply(false, true);
        runtime->mode = motor_control_mode_complete(runtime->mode);
    }
}

/**
 * @brief Advances the two-phase current-offset calibration.
 *
 * The first visit installs zero PWM, routes both ADC triggers, and unmasks the PWM outputs. After
 * 1024 samples per phase, the normal four ADC channels and startup ramp are enabled.
 *
 * @param runtime Active motor runtime and current calibration state.
 */
static void motor_runtime_current_calibration_step(MotorRuntime *runtime) {
    if (!runtime->current_calibration_started) {
        GMCLIB_3COOR_T_F16 duty = {0};
        motor_pwm_write(FTM0, &duty);
        motor_current_calibration_start(&runtime->current_calibration);
        motor_current_calibration_hardware_start();
        runtime->current_calibration_started = true;
    }

    if (motor_current_calibration_poll(&runtime->current_calibration) !=
        kMotorCurrentCalibrationFinished) {
        return;
    }

    motor_adc_runtime_initialize(runtime->hardware.adc_auxiliary_channel);
    runtime->timing.countdowns[kMotorCountdownStartupRamp].active = 1U;
    runtime->mode = motor_control_mode_complete(runtime->mode);
}

/**
 * @brief Advances startup rotor alignment and index-seek setup.
 *
 * D-axis current rises by ten per service tick to ten thousand. Completion enables quadrature
 * overflow extension, the motor link, motion estimation, and the first encoder-index search.
 *
 * @param runtime Active motor runtime and startup ramp countdown.
 */
static void motor_runtime_startup_ramp_step(MotorRuntime *runtime) {
    MotorCountdown *countdown = &runtime->timing.countdowns[kMotorCountdownStartupRamp];
    uint16_t current = motor_control_startup_ramp_current(countdown->ticks);
    motor_runtime_control_cycle(runtime, (int16_t)current, true);
    if (countdown->ticks != 0U) {
        return;
    }

    countdown->active = 0U;
    runtime->control_current = 0;
    motor_encoder_overflow_interrupt_enable();
    motor_encoder_position_reset(&runtime->encoder);
    runtime->motion = (MotorMotionState){0};
    runtime->position_filter.accumulator = 0;
    runtime->velocity_filter.accumulator = 0;
    motor_runtime_index_seek_restart(runtime);
    motor_spi_link_active_set(true);
    runtime->mode = motor_control_mode_complete(runtime->mode);
}

/**
 * @brief Advances the startup encoder-index search.
 *
 * The motor applies the fixed index current until the index arrives or the five-thousand-tick
 * timeout expires. Either result releases the search and enters normal run mode.
 *
 * @param runtime Active motor runtime and encoder-index search state.
 */
static void motor_runtime_startup_gate_step(MotorRuntime *runtime) {
    motor_runtime_encoder_position_refresh(runtime);
    MotorCountdown *countdown = &runtime->timing.countdowns[kMotorCountdownEncoderIndex];
    MotorEncoderIndexSeekStep step =
        motor_encoder_index_seek_step(runtime->encoder_index_detected, countdown->ticks);
    countdown->active = step.countdown_active ? 1U : 0U;
    runtime->control_current = step.drive_current;
    if (step.complete) {
        countdown->ticks = 5000U;
        motor_encoder_index_interrupt_disable();
        runtime->motion.previous_counter = (uint32_t)runtime->encoder.position;
        runtime->mode = motor_control_mode_complete(runtime->mode);
    }
    motor_startup_interlock_outputs_apply(false, true);
    motor_runtime_control_cycle(runtime, runtime->control_current, false);
}

/**
 * @brief Erases the persisted encoder correction record.
 *
 * Flash operations run with interrupts masked. The in-memory record, calibration command,
 * published version, and correction enable are cleared after a successful erase.
 *
 * @param runtime Active motor runtime and calibration state.
 */
static void motor_runtime_encoder_calibration_erase(MotorRuntime *runtime) {
    memset(&runtime->encoder_calibration.record, 0, sizeof(runtime->encoder_calibration.record));
    uint32_t interrupt_mask = DisableGlobalIRQ();
    status_t status = motor_calibration_storage_erase();
    EnableGlobalIRQ(interrupt_mask);
    if (status != kStatus_Success) {
        motor_runtime_fault();
    }

    runtime->parameters.entries[MOTOR_PARAMETER_CALIBRATION_COMMAND].value = 0U;
    runtime->parameters.entries[MOTOR_PARAMETER_CALIBRATION_VERSION].value = 0U;
    runtime->calibration_valid = false;
}

/**
 * @brief Persists a completed encoder correction record.
 *
 * Flash erase and programming run inside one interrupt-masked transaction. Success publishes the
 * new version, clears the calibration command, and enables correction lookup immediately.
 *
 * @param runtime Active motor runtime and completed calibration record.
 */
static void motor_runtime_encoder_calibration_store(MotorRuntime *runtime) {
    uint32_t interrupt_mask = DisableGlobalIRQ();
    status_t status = motor_calibration_storage_erase();
    if (status == kStatus_Success) {
        status = motor_calibration_storage_program(&runtime->encoder_calibration.record);
    }
    EnableGlobalIRQ(interrupt_mask);
    if (status != kStatus_Success) {
        motor_runtime_fault();
    }

    runtime->parameters.entries[MOTOR_PARAMETER_CALIBRATION_COMMAND].value = 0U;
    runtime->parameters.entries[MOTOR_PARAMETER_CALIBRATION_VERSION].value =
        runtime->encoder_calibration.record.version;
    runtime->calibration_valid = true;
}

/**
 * @brief Applies one run-mode maintenance request.
 *
 * Calibration and erase share parameter six and therefore take priority over the independent
 * direction diagnostic in parameter five.
 *
 * @param runtime Active motor runtime and writable parameter bank.
 */
static void motor_runtime_request_apply(MotorRuntime *runtime) {
    MotorControlRequest request = motor_control_request_decode(
        runtime->parameters.entries[MOTOR_PARAMETER_CALIBRATION_COMMAND].value,
        runtime->parameters.entries[MOTOR_PARAMETER_DIRECTION_COMMAND].value);
    if (request == kMotorControlRequestEraseEncoderCalibration) {
        motor_runtime_encoder_calibration_erase(runtime);
        return;
    }
    if (request == kMotorControlRequestCalibrateEncoder) {
        runtime->mode = motor_control_request_apply(runtime->mode, request);
        return;
    }
    if (request != kMotorControlRequestCheckEncoderDirection) {
        return;
    }

    motor_encoder_direction_initialize(&runtime->encoder_direction);
    runtime->mode = motor_control_request_apply(runtime->mode, request);
}

/**
 * @brief Advances encoder correction calibration and its motor-control cycle.
 *
 * The ADC-rate state machine selects velocity targets and captures filtered velocity error at each
 * tenth encoder count. The service-rate velocity PI supplies Q-axis current until both directional
 * sweeps are captured, the shaft returns to center, and the record is persisted.
 *
 * @param runtime Active motor runtime, velocity controller, and calibration capture state.
 */
static void motor_runtime_encoder_calibration_step(MotorRuntime *runtime) {
    motor_runtime_encoder_position_refresh(runtime);
    motor_startup_interlock_outputs_apply(false, true);

    MotorEncoderCalibrationInput input = {
        .velocity = runtime->motion_sample.filtered_position_delta,
        .correction = runtime->velocity_control.velocity_error,
        .position = runtime->encoder.position,
        .relative_position = motor_encoder_relative_position(
            (uint16_t)FTM2->CNT, (uint16_t)runtime->encoder.zero_counter,
            (uint16_t)runtime->hardware.encoder_modulus),
        .encoder_period = (uint16_t)runtime->hardware.encoder_modulus,
        .revolution_complete = motor_encoder_revolution_is_complete(),
    };
    MotorEncoderCalibrationStep step =
        motor_encoder_calibration_step(&runtime->encoder_calibration, &input);

    if (step.reset_controller) {
        motor_velocity_control_reset(&runtime->velocity_control);
    }
    motor_velocity_control_target_set(&runtime->velocity_control, step.target_velocity);
    if (step.clear_revolution) {
        motor_encoder_revolution_clear();
    }
    if (step.arm_revolution) {
        motor_encoder_revolution_arm();
    }
    if (step.result == kMotorEncoderCalibrationComplete) {
        motor_runtime_encoder_calibration_store(runtime);
        runtime->mode = motor_control_mode_complete(runtime->mode);
    }

    runtime->control_current = runtime->velocity_control.current_reference;
    motor_runtime_control_cycle(runtime, runtime->control_current, false);
}

/**
 * @brief Advances the encoder-direction diagnostic.
 *
 * Two forward index searches establish one measured revolution. A valid revolution is followed by
 * a reverse return to the captured start position. The writable direction parameter reports the
 * running, failed, or completed status.
 *
 * @param runtime Active motor runtime, encoder state, and index-search countdown.
 */
static void motor_runtime_encoder_direction_step(MotorRuntime *runtime) {
    motor_runtime_encoder_position_refresh(runtime);
    motor_startup_interlock_outputs_apply(false, true);

    MotorCountdown *countdown = &runtime->timing.countdowns[kMotorCountdownEncoderIndex];
    MotorEncoderIndexSeekStep seek =
        motor_encoder_index_seek_step(runtime->encoder_index_detected, countdown->ticks);
    MotorEncoderDirectionStep step = motor_encoder_direction_check_step(
        &runtime->encoder_direction, seek.complete, runtime->encoder.position,
        (int32_t)runtime->hardware.encoder_modulus);

    countdown->active = seek.countdown_active ? 1U : 0U;
    runtime->parameters.entries[MOTOR_PARAMETER_DIRECTION_COMMAND].value = step.status;
    if (step.reset_controller) {
        motor_foc_initialize(&runtime->foc);
    }
    if (step.reset_position) {
        motor_encoder_position_reset(&runtime->encoder);
        runtime->motion.previous_counter = 0U;
    }
    if (step.restart_index_seek) {
        motor_runtime_index_seek_restart(runtime);
    }
    if (step.result != kMotorEncoderDirectionPending) {
        countdown->active = 0U;
        motor_encoder_index_interrupt_disable();
        runtime->mode = motor_control_mode_complete(runtime->mode);
    }

    runtime->control_current = step.drive_current;
    motor_runtime_control_cycle(runtime, runtime->control_current, false);
}

/**
 * @brief Runs one normal motor-control interrupt cycle.
 *
 * The encoder position is refreshed and the normal rotor-angle FOC path consumes the current
 * command most recently published by the main-loop drive service.
 *
 * @param runtime Active motor runtime and run-mode state.
 */
static void motor_runtime_run_step(MotorRuntime *runtime) {
    motor_runtime_encoder_position_refresh(runtime);
    motor_runtime_request_apply(runtime);
    motor_startup_interlock_outputs_apply(false, true);
    motor_runtime_control_cycle(runtime, runtime->control_current, false);
}

/**
 * @brief Dispatches one completed motor ADC interrupt by control mode.
 *
 * The electrical angle and deferred-update event are published before the startup or run state
 * consumes the synchronized ADC results.
 *
 * @param electrical_angle Wrapped electrical rotor angle for this conversion.
 * @param control_update_due True after every seventh ADC interrupt.
 * @param context Active motor runtime supplied during ADC initialization.
 */
static void motor_runtime_adc_handler(int16_t electrical_angle, bool control_update_due,
                                      void *context) {
    MotorRuntime *runtime = context;
    runtime->electrical_angle = electrical_angle;
    runtime->control_update_pending |= control_update_due;

    switch (runtime->mode) {
    case kMotorControlStartupInterlockA:
    case kMotorControlStartupInterlockB:
        motor_runtime_interlock_step(runtime);
        break;
    case kMotorControlCurrentCalibration:
        motor_runtime_current_calibration_step(runtime);
        break;
    case kMotorControlStartupRamp:
        motor_runtime_startup_ramp_step(runtime);
        break;
    case kMotorControlStartupGate:
        motor_runtime_startup_gate_step(runtime);
        break;
    case kMotorControlRun:
        motor_runtime_run_step(runtime);
        break;
    case kMotorControlEncoderCalibration:
        motor_runtime_encoder_calibration_step(runtime);
        break;
    case kMotorControlEncoderDirectionCheck:
        motor_runtime_encoder_direction_step(runtime);
        break;
    case kMotorControlInactive:
    default:
        break;
    }
}

/**
 * @brief Extends one FTM2 quadrature overflow into the runtime position.
 *
 * The hardware direction selects addition or subtraction of the board-specific encoder modulus.
 *
 * @param increasing True when the hardware counter overflowed in the increasing direction.
 * @param context Active motor runtime supplied during FTM2 initialization.
 */
static void motor_runtime_encoder_overflow_handler(bool increasing, void *context) {
    MotorRuntime *runtime = context;
    motor_encoder_overflow_apply(&runtime->encoder, (int32_t)runtime->hardware.encoder_modulus,
                                 increasing);
}

/**
 * @brief Captures the one-shot encoder index event.
 *
 * The first index establishes the persistent counter zero. Every index publishes a fresh event for
 * startup, calibration, or direction-check state.
 *
 * @param counter FTM2 counter captured by the PORTE24 interrupt.
 * @param context Active motor runtime supplied during index initialization.
 */
static void motor_runtime_encoder_index_handler(uint16_t counter, void *context) {
    MotorRuntime *runtime = context;
    if (!runtime->encoder_zero_captured) {
        runtime->encoder.zero_counter = counter;
        runtime->encoder_zero_captured = true;
    }
    runtime->encoder_index_detected = true;
}

/**
 * @brief Advances motion estimation and service countdowns.
 *
 * Each FTM3 period publishes position and velocity deltas, services the parameter bus, advances the
 * product derating controller every tenth tick, and updates the remaining service-rate controls.
 *
 * @param context Active motor runtime supplied during service timer initialization.
 */
static void motor_runtime_service_handler(void *context) {
    MotorRuntime *runtime = context;
    ++runtime->service_tick;
    runtime->motion_sample =
        motor_motion_sample(&runtime->motion, &runtime->position_filter, &runtime->velocity_filter,
                            (uint32_t)runtime->encoder.position, runtime->hardware.velocity_scale);
    (void)motor_service_timing_tick(&runtime->timing);
    motor_bus_service();
    if (++runtime->derating_update_count >= 10U) {
        runtime->drive_derating.current_scale =
            GFLIB_CtrlPIpAW_F16(runtime->drive_derating.error, &runtime->derating_stop_integrator,
                                &runtime->derating_controller);
        runtime->derating_update_count = 0U;
    }
    (void)motor_velocity_control_step(&runtime->velocity_control,
                                      runtime->motion_sample.filtered_position_delta,
                                      runtime->foc.q_controller.bLimFlag);
    runtime->parameters.entries[MOTOR_PARAMETER_SERVICE_TICK].value = runtime->service_tick;
    runtime->parameters.entries[MOTOR_PARAMETER_DRIVE_CURRENT].value =
        (uint16_t)runtime->control_current;
}

/**
 * @brief Prepares the next official motor-link position response.
 *
 * The extended encoder position, measured Q-axis torque, and live primary drive current are
 * encoded into the transmit buffer before the next SPI transfer.
 *
 * @param frame Motor-link transfer buffer to populate.
 * @param context Active motor runtime supplied during SPI initialization.
 */
static void motor_runtime_spi_prepare(uint8_t frame[MOTOR_SPI_TRANSFER_SIZE], void *context) {
    MotorRuntime *runtime = context;
    int16_t measured_current = runtime->foc_output.filtered_current.f16Q;
    uint16_t torque =
        measured_current < 0 ? (uint16_t)-(int32_t)measured_current : (uint16_t)measured_current;
    MotorLinkPositionReport report = {
        .position = runtime->encoder.position,
        .torque = torque,
        .drive_current = runtime->live_drive.primary_current,
        .positive = runtime->live_drive.primary_current >= 0,
    };
    motor_link_position_frame_encode(&report, frame);
}

/**
 * @brief Applies one received official motor-link frame.
 *
 * Valid live-force commands immediately update drive current and both FOC controller coefficient
 * pairs. Local effect frames retain their state until the deferred force-feedback service runs.
 *
 * @param frame Complete received motor-link transfer buffer.
 * @param context Active motor runtime supplied during SPI initialization.
 */
static void motor_runtime_spi_receive(const uint8_t frame[MOTOR_SPI_TRANSFER_SIZE], void *context) {
    MotorRuntime *runtime = context;
    MotorLinkFrame decoded;
    if (motor_link_frame_decode(frame, &decoded) != MOTOR_LINK_FRAME_VALID ||
        !motor_protocol_frame_apply(&runtime->protocol, &decoded) ||
        !runtime->protocol.live_drive_updated) {
        return;
    }

    motor_runtime_drive_apply(runtime);
}

/**
 * @brief Initializes the complete motor runtime and its interrupt graph.
 *
 * Board identity selects the hardware profile, product configuration selects normal output scale,
 * calibration flash is loaded, and all SDK peripherals are configured while global interrupts are
 * masked. Startup begins with the first interlock delay.
 */
void motor_runtime_initialize(void) {
    memset(&motor_runtime, 0, sizeof(motor_runtime));
    motor_pins_initialize();
    motor_runtime.identity = motor_board_identity_read();

    uint32_t interrupt_mask = DisableGlobalIRQ();
    if (motor_calibration_storage_initialize() != kStatus_Success) {
        motor_runtime_fault();
    }

    motor_runtime.hardware =
        motor_hardware_profile_select(((motor_runtime.identity & 0x0fU) >> 3U) != 0U);
    motor_drive_friction_initialize(&motor_runtime.drive_friction,
                                    motor_runtime.hardware.secondary_scale);
    motor_drive_derating_initialize(&motor_runtime.drive_derating,
                                    motor_product_configuration.normal_current_scale);
    motor_runtime.derating_controller = (GFLIB_CTRL_PI_P_AW_T_A32){
        .a32IGain = 0x106,
        .f16UpperLim = motor_product_configuration.normal_current_scale,
        .f16LowerLim = motor_product_configuration.minimum_current_scale,
    };
    GFLIB_CtrlPIpAWInit_F16(motor_product_configuration.normal_current_scale,
                            &motor_runtime.derating_controller);
    motor_runtime.mode = motor_control_mode_initialize();
    motor_runtime.position_filter.shift = 4U;
    motor_runtime.velocity_filter.shift = 6U;
    motor_parameter_bank_initialize(&motor_runtime.parameters, motor_runtime.identity);
    motor_protocol_initialize(&motor_runtime.protocol,
                              motor_product_configuration.normal_output_percent);
    motor_foc_initialize(&motor_runtime.foc);
    motor_velocity_control_initialize(&motor_runtime.velocity_control,
                                      motor_product_configuration.normal_current_scale);
    motor_service_timing_initialize(&motor_runtime.timing);
    motor_encoder_calibration_initialize(&motor_runtime.encoder_calibration);
    motor_encoder_direction_initialize(&motor_runtime.encoder_direction);
    motor_runtime.calibration_valid =
        motor_calibration_storage_load(&motor_runtime.encoder_calibration.record);
    if (motor_runtime.calibration_valid) {
        motor_runtime.parameters.entries[MOTOR_PARAMETER_CALIBRATION_VERSION].value =
            motor_runtime.encoder_calibration.record.version;
    }
    motor_runtime_settings_apply(&motor_runtime);

    motor_reset_filter_initialize();
    motor_pwm_initialize();
    motor_tick_timer_initialize((uint16_t)motor_runtime.hardware.encoder_modulus,
                                motor_runtime_encoder_overflow_handler,
                                motor_runtime_encoder_index_handler, &motor_runtime);
    motor_adc_initialize(motor_runtime.hardware.position_scale, motor_runtime_adc_handler,
                         &motor_runtime);
    motor_adc_trigger_initialize();
    motor_bus_initialize(&motor_runtime.parameters, motor_runtime_settings_apply, &motor_runtime);
    motor_spi_initialize(&motor_runtime.spi, motor_runtime_spi_prepare, motor_runtime_spi_receive,
                         &motor_runtime);
    motor_service_timer_initialize(motor_runtime_service_handler, &motor_runtime);
    motor_communication_timeout_timer_initialize(motor_spi_timeout_service, &motor_runtime);
    motor_interrupts_initialize();
    EnableGlobalIRQ(interrupt_mask);
}

/**
 * @brief Services deferred motor runtime work.
 *
 * Local force feedback is mixed once per service tick and applied to the live FOC command.
 * Seven-ADC events advance the selected force interpolation response. Run mode publishes natural
 * friction and the complete product-scaled current command. Temperature windows and measured
 * torque are exposed through the read-only parameter bank.
 */
void motor_runtime_poll(void) {
    int32_t centered_position = motor_centered_position_resolve(motor_runtime.encoder.position,
                                                                motor_runtime.protocol.center);
    if (motor_protocol_force_feedback_service(
            &motor_runtime.protocol, motor_runtime.service_tick, centered_position,
            motor_runtime.motion_sample.filtered_position_delta)) {
        motor_runtime_drive_apply(&motor_runtime);
    }

    if (motor_runtime.control_update_pending) {
        (void)motor_drive_interpolation_step(
            &motor_runtime.drive_interpolation, motor_runtime.live_drive.primary_current,
            (uint8_t)motor_runtime.parameters.entries[MOTOR_PARAMETER_INTERPOLATION].value);
        motor_runtime.control_update_pending = false;
    }

    if (motor_runtime.mode == kMotorControlRun && motor_runtime.encoder_index_detected &&
        motor_runtime.parameters.entries[MOTOR_PARAMETER_CALIBRATION_COMMAND].value != 0xaaaaU) {
        motor_runtime.friction_current = motor_drive_friction_step(
            &motor_runtime.drive_friction, motor_runtime.encoder.position,
            (uint16_t)motor_runtime.parameters.entries[MOTOR_PARAMETER_NATURAL_FRICTION].value);
        motor_runtime.control_current = motor_runtime_current_resolve(&motor_runtime);
    }

    if (motor_auxiliary_samples_resolve(&motor_runtime.auxiliary_accumulator,
                                        &motor_runtime.auxiliary_telemetry)) {
        motor_runtime.parameters.entries[MOTOR_PARAMETER_MOTOR_TEMPERATURE].value =
            (uint16_t)motor_runtime.auxiliary_telemetry.motor_temperature;
        motor_runtime.parameters.entries[MOTOR_PARAMETER_DRIVER_TEMPERATURE].value =
            (uint16_t)motor_runtime.auxiliary_telemetry.driver_temperature;
    }

    int16_t measured_current = motor_runtime.foc_output.filtered_current.f16Q;
    motor_runtime.parameters.entries[MOTOR_PARAMETER_TORQUE].value =
        measured_current < 0 ? (uint16_t)-(int32_t)measured_current : (uint16_t)measured_current;
}
