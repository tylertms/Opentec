#ifndef OPENTEC_MOTOR_PERIPHERALS_H
#define OPENTEC_MOTOR_PERIPHERALS_H

#include <stdbool.h>
#include <stdint.h>

#include "motor/calibration.h"

/**
 * @brief Handles one periodic motor timer event.
 *
 * The registered timer interrupt invokes the handler with its stored context.
 *
 * @param[in] context Caller context supplied during timer initialization.
 */
typedef void (*MotorTimerHandler)(void *context);

/**
 * @brief Handles one FTM2 quadrature overflow event.
 *
 * The handler receives the direction captured from the quadrature decoder for the overflow.
 *
 * @param[in] increasing True when the quadrature counter overflowed while increasing.
 * @param[in] context Caller context supplied during timer initialization.
 */
typedef void (*MotorEncoderOverflowHandler)(bool increasing, void *context);

/**
 * @brief Handles one captured encoder-index counter value.
 *
 * The handler receives the FTM2 count before the one-shot index interrupt is disabled.
 *
 * @param[in] counter FTM2 counter captured at the encoder index.
 * @param[in] context Caller context supplied during timer initialization.
 */
typedef void (*MotorEncoderIndexHandler)(uint16_t counter, void *context);

/**
 * @brief Handles one completed motor ADC conversion.
 *
 * The handler receives the electrical angle and whether the deferred control update is due.
 *
 * @param[in] electrical_angle Electrical angle derived from the encoder counter.
 * @param[in] control_update_due True when the deferred control update is due.
 * @param[in] context Caller context supplied during ADC initialization.
 */
typedef void (*MotorAdcHandler)(int16_t electrical_angle, bool control_update_due, void *context);

/**
 * @brief Holds the alternating motor and driver auxiliary ADC samples.
 */
typedef struct {
    uint16_t motor;  /**< Motor-temperature ADC sample. */
    uint16_t driver; /**< Motor-driver-temperature ADC sample. */
} MotorAdcAuxiliarySamples;

/**
 * @brief Calibrates and configures both motor-current ADCs.
 *
 * Hardware averaging is used for calibration, then both ADCs are configured for PWM-aligned PDB
 * triggering and the registered handler receives completed current conversions.
 *
 * @param[in] encoder_scale Fixed-point scale for converting the encoder count to electrical angle.
 * @param[in] handler Function invoked for each completed motor ADC conversion.
 * @param[in] context Caller context passed to the ADC handler.
 * @return True when both ADC calibration sequences complete successfully.
 */
bool motor_adc_initialize(uint32_t encoder_scale, MotorAdcHandler handler, void *context);

/**
 * @brief Configures the PDB timing used to trigger the ADC modules.
 *
 * The shared modulus and pretrigger delays establish the PWM-aligned current and auxiliary sample
 * schedule.
 */
void motor_adc_trigger_initialize(void);

/**
 * @brief Enables the configured PDB ADC pretriggers.
 *
 * Both ADC trigger channels use two output pretriggers without back-to-back operation.
 */
void motor_adc_trigger_enable(void);

/**
 * @brief Routes both current ADCs through the PDB for offset calibration.
 *
 * Zero-duty PWM outputs are unmasked when the current-calibration hardware phase begins.
 */
void motor_current_calibration_hardware_start(void);

/**
 * @brief Polls the active ADC and advances current-offset calibration.
 *
 * The active phase consumes one conversion when available and switches ADC channel routing at each
 * phase transition.
 *
 * @param[in,out] state Current calibration stage, accumulation, and resulting offsets.
 * @return Pending, phase-B transition, or completed calibration result.
 */
MotorCurrentCalibrationResult motor_current_calibration_poll(MotorCurrentCalibrationState *state);

/**
 * @brief Selects the ADC channels used during motor control.
 *
 * ADC0 is configured for phase-B current and DC-bus voltage, while ADC1 is configured for phase-A
 * current and the alternating motor and driver temperature samples.
 *
 * @param[in] adc0_auxiliary_channel Board-selected ADC0 DC-bus channel, either four or seven.
 */
void motor_adc_runtime_initialize(uint32_t adc0_auxiliary_channel);

/**
 * @brief Captures one alternating pair of auxiliary ADC samples.
 *
 * The first call retains the motor sample and the next call publishes it with the driver sample.
 *
 * @param[out] samples Completed motor and driver ADC sample pair when the function returns true.
 * @return True when both samples are available.
 */
bool motor_adc_auxiliary_capture(MotorAdcAuxiliarySamples *samples);

/**
 * @brief Rearms the alternating auxiliary ADC channel selection.
 *
 * Phase-current channels are restored and the next motor or driver auxiliary channel is selected.
 */
void motor_adc_auxiliary_rearm(void);

/**
 * @brief Configures the reset-pin filter for run, wait, and stop operation.
 *
 * The reset filter mode and maximum filter width are applied to the reset-controller registers.
 */
void motor_reset_filter_initialize(void);

/**
 * @brief Enables the interrupt sources used by motor firmware.
 *
 * ADC, PDB, I2C, FTM, and combined-port vectors receive their configured priorities.
 */
void motor_interrupts_initialize(void);

/**
 * @brief Configures masked complementary PWM for all three motor phases.
 *
 * The FTM0 period, dead time, complementary pairs, fault behavior, initial compares, and startup
 * output mask are initialized.
 */
void motor_pwm_initialize(void);

/**
 * @brief Unmasks all PWM outputs for zero-duty current-offset calibration.
 *
 * The output mask is cleared to expose the zero-duty compares configured for calibration.
 */
void motor_pwm_enable_outputs(void);

/**
 * @brief Masks all complementary motor PWM outputs.
 *
 * The complete FTM0 output mask is applied for fault and maintenance safety.
 */
void motor_pwm_disable_outputs(void);

/**
 * @brief Configures the FTM2 motor scheduling timer.
 *
 * The timer runs as a filtered quadrature decoder and invokes the registered overflow and index
 * callbacks.
 *
 * @param[in] modulus FTM2 quadrature-counter MOD value selected by the motor configuration.
 * @param[in] handler Function invoked with the quadrature overflow direction.
 * @param[in] index_handler Function invoked with the counter captured at the encoder index.
 * @param[in] context Caller context passed to both timer handlers.
 */
void motor_tick_timer_initialize(uint16_t modulus, MotorEncoderOverflowHandler handler,
                                 MotorEncoderIndexHandler index_handler, void *context);

/**
 * @brief Enables the FTM2 overflow interrupt.
 *
 * The hardware count and calibration-revolution state are cleared before overflow extension starts.
 */
void motor_encoder_overflow_interrupt_enable(void);

/**
 * @brief Arms the next encoder full-revolution completion event.
 *
 * The next FTM2 overflow publishes the one-shot revolution completion state.
 */
void motor_encoder_revolution_arm(void);

/**
 * @brief Clears the encoder full-revolution completion state.
 *
 * Both the one-shot arm and completion flags are released for another calibration sweep.
 */
void motor_encoder_revolution_clear(void);

/**
 * @brief Tests whether an encoder full revolution has completed.
 *
 * The completion state remains set until it is explicitly cleared.
 *
 * @return True after an armed FTM2 overflow.
 */
bool motor_encoder_revolution_is_complete(void);

/**
 * @brief Enables the falling-edge encoder-index interrupt.
 *
 * Any stale port flag is cleared before the one-shot trigger is armed.
 */
void motor_encoder_index_interrupt_enable(void);

/**
 * @brief Disables and clears the encoder-index interrupt.
 *
 * The port trigger is removed and any captured flag is discarded.
 */
void motor_encoder_index_interrupt_disable(void);

/**
 * @brief Configures the periodic motor-service timer.
 *
 * The FTM3 overflow callback runs at the configured service cadence.
 *
 * @param[in] handler Function invoked for each service period.
 * @param[in] context Caller context passed to the service handler.
 */
void motor_service_timer_initialize(MotorTimerHandler handler, void *context);

/**
 * @brief Configures the communication-timeout timer.
 *
 * The FTM4 overflow callback runs the delayed motor-link response service.
 *
 * @param[in] handler Function invoked for each communication period.
 * @param[in] context Caller context passed to the communication handler.
 */
void motor_communication_timeout_timer_initialize(MotorTimerHandler handler, void *context);

#endif
