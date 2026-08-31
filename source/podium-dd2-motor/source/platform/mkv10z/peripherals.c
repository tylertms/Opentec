#include "platform/peripherals.h"

#include <fsl_adc16.h>
#include <fsl_common.h>
#include <fsl_ftm.h>
#include <fsl_pdb.h>
#include <fsl_port.h>

#include "motor/control.h"
#include "motor/motion.h"

static MotorTimerHandler service_timer_handler;
static void *service_timer_context;
static MotorTimerHandler communication_timer_handler;
static void *communication_timer_context;
static MotorEncoderOverflowHandler encoder_overflow_handler;
static void *encoder_overflow_context;
static MotorEncoderIndexHandler encoder_index_handler;
static void *encoder_index_context;
static volatile bool encoder_revolution_armed;
static volatile bool encoder_revolution_complete;
static MotorAdcHandler adc_handler;
static void *adc_context;
static uint32_t adc_encoder_scale;
static uint32_t adc0_auxiliary_channel;
static uint16_t adc_auxiliary_first_sample;
static uint8_t adc_control_conversion_count;
static bool adc_auxiliary_second_sample;

/**
 * @brief Calibrates and configures both motor-current ADCs for PDB triggering.
 *
 * Both ADC modules are calibrated with hardware averaging, then restored to twelve-bit,
 * divider-two, non-continuous operation. ADC1 channel ten supplies the startup interrupt.
 *
 * @param encoder_scale Board-selected fixed-point electrical-angle scale.
 * @param handler Function invoked for each completed motor ADC conversion.
 * @param context Caller context passed to the ADC handler.
 */
void motor_adc_initialize(uint32_t encoder_scale, MotorAdcHandler handler, void *context) {
    adc16_config_t config;

    adc_encoder_scale = encoder_scale;
    adc_handler = handler;
    adc_context = context;
    adc_control_conversion_count = 0U;

    ADC16_GetDefaultConfig(&config);
    config.clockSource = kADC16_ClockSourceAlt0;
    config.clockDivider = kADC16_ClockDivider1;
    config.resolution = kADC16_ResolutionSE12Bit;
    config.longSampleMode = kADC16_LongSampleDisabled;
    config.enableContinuousConversion = true;
    config.hardwareAverageMode = kADC16_HardwareAverageCount32;

    ADC16_Init(ADC0, &config);
    ADC16_Init(ADC1, &config);
    (void)ADC16_DoAutoCalibration(ADC0);
    (void)ADC16_DoAutoCalibration(ADC1);

    config.clockDivider = kADC16_ClockDivider2;
    config.enableContinuousConversion = false;
    config.hardwareAverageMode = kADC16_HardwareAverageDisabled;

    ADC16_Init(ADC0, &config);
    ADC16_Init(ADC1, &config);
    ADC16_EnableHardwareTrigger(ADC0, true);
    ADC16_EnableHardwareTrigger(ADC1, true);
    SIM->SOPT7 = SIM_SOPT7_ADC0TRGSEL(8U) | SIM_SOPT7_ADC0ALTTRGEN(2U) | SIM_SOPT7_ADC1TRGSEL(8U) |
                 SIM_SOPT7_ADC1ALTTRGEN(2U);

    adc16_channel_config_t channel_config = {
        .channelNumber = 10U,
        .enableInterruptOnConversionCompleted = true,
        .enableDifferentialConversion = false,
    };
    ADC16_SetChannelConfig(ADC1, 0U, &channel_config);
}

/**
 * @brief Publishes one official motor ADC interrupt sample to the control layer.
 *
 * The quadrature count is converted to electrical angle and the seven-conversion deferred cadence
 * is advanced before the runtime callback executes.
 */
static void motor_adc_interrupt_dispatch(void) {
    int16_t electrical_angle =
        motor_q15_scale_wrap(adc_encoder_scale, (int16_t)(uint16_t)FTM2->CNT);
    bool control_update_due = motor_control_update_due(&adc_control_conversion_count);
    if (adc_handler != NULL) {
        adc_handler(electrical_angle, control_update_due, adc_context);
    }
}

/**
 * @brief Dispatches the official ADC0 vector through the shared motor ADC handler.
 *
 * Both current ADC vectors intentionally enter the same synchronized control dispatch.
 */
void ADC0_IRQHandler(void) { motor_adc_interrupt_dispatch(); }

/**
 * @brief Dispatches the official ADC1 vector through the shared motor ADC handler.
 *
 * Both current ADC vectors intentionally enter the same synchronized control dispatch.
 */
void ADC1_IRQHandler(void) { motor_adc_interrupt_dispatch(); }

/**
 * @brief Clears both official PDB ADC-channel status banks and resumes triggering.
 *
 * The interrupt temporarily disables the PDB while clearing completion and sequence-error flags.
 */
void PDB0_PDB1_IRQHandler(void) {
    PDB0->SC &= ~PDB_SC_PDBEN_MASK;
    PDB0->CH[0].S &= ~PDB_S_CF_MASK;
    PDB0->CH[0].S &= ~PDB_S_ERR_MASK;
    PDB0->CH[1].S &= ~PDB_S_CF_MASK;
    PDB0->CH[1].S &= ~PDB_S_ERR_MASK;
    PDB0->SC |= PDB_SC_PDBEN_MASK;
}

/**
 * @brief Configures the PDB timing used to trigger both ADC modules.
 *
 * The shared modulus, midpoint interrupt, and two pretrigger delays reproduce the PWM-aligned
 * current and auxiliary conversion schedule.
 */
void motor_adc_trigger_initialize(void) {
    pdb_config_t config;

    CLOCK_EnableClock(kCLOCK_Pdb0);
    PDB_SetModulusValue(PDB0, 1500U);
    PDB_SetADCPreTriggerDelayValue(PDB0, kPDB_ADCTriggerChannel0, kPDB_ADCPreTrigger0, 10U);
    PDB_SetADCPreTriggerDelayValue(PDB0, kPDB_ADCTriggerChannel0, kPDB_ADCPreTrigger1, 750U);
    PDB_SetADCPreTriggerDelayValue(PDB0, kPDB_ADCTriggerChannel1, kPDB_ADCPreTrigger0, 10U);
    PDB_SetADCPreTriggerDelayValue(PDB0, kPDB_ADCTriggerChannel1, kPDB_ADCPreTrigger1, 750U);

    PDB_GetDefaultConfig(&config);
    config.triggerInputSource = kPDB_TriggerInput8;
    PDB_Init(PDB0, &config);
    PDB_EnableInterrupts(PDB0, kPDB_DelayInterruptEnable | kPDB_SequenceErrorInterruptEnable);
    PDB_DoLoadValues(PDB0);
}

/**
 * @brief Enables both timed ADC pretriggers after current-offset calibration.
 *
 * Both ADC channels receive two enabled output pretriggers without back-to-back operation.
 */
void motor_adc_trigger_enable(void) {
    pdb_adc_pretrigger_config_t config = {
        .enablePreTriggerMask = 0x3U,
        .enableOutputMask = 0x3U,
        .enableBackToBackOperationMask = 0U,
    };

    PDB_SetADCPreTriggerConfig(PDB0, kPDB_ADCTriggerChannel0, &config);
    PDB_SetADCPreTriggerConfig(PDB0, kPDB_ADCTriggerChannel1, &config);
}

/**
 * @brief Routes both current ADCs through the PDB for offset calibration.
 *
 * The route changes only when the startup state reaches current calibration. All six zero-duty
 * PWM outputs are unmasked at the same transition.
 */
void motor_current_calibration_hardware_start(void) {
    SIM->SOPT7 |= SIM_SOPT7_ADC0TRGSEL(8U) | SIM_SOPT7_ADC0ALTTRGEN(2U) | SIM_SOPT7_ADC1TRGSEL(8U) |
                  SIM_SOPT7_ADC1ALTTRGEN(2U);
    motor_pwm_enable_outputs();
}

/**
 * @brief Polls the active ADC and advances two-phase current-offset calibration.
 *
 * The active calibration phase selects its ADC and installs the next channel routing at each phase
 * transition.
 *
 * @param state Current calibration stage, accumulation, and resulting offsets.
 * @return Pending, phase-B transition, or completed calibration.
 */
MotorCurrentCalibrationResult motor_current_calibration_poll(MotorCurrentCalibrationState *state) {
    ADC_Type *adc;

    if (state->stage == kMotorCurrentCalibrationPhaseA) {
        adc = ADC1;
    } else if (state->stage == kMotorCurrentCalibrationPhaseB) {
        adc = ADC0;
    } else {
        return kMotorCurrentCalibrationPending;
    }

    bool sample_ready = false;
    uint16_t sample = 0U;
    if (state->sample_count < MOTOR_CURRENT_CALIBRATION_SAMPLE_COUNT) {
        sample_ready =
            (ADC16_GetChannelStatusFlags(adc, 0U) & kADC16_ChannelConversionDoneFlag) != 0U;
        if (sample_ready) {
            sample = (uint16_t)ADC16_GetChannelConversionValue(adc, 0U);
        }
    }

    MotorCurrentCalibrationResult result =
        motor_current_calibration_step(state, sample_ready, sample);
    adc16_channel_config_t channel_config = {
        .channelNumber = 0U,
        .enableInterruptOnConversionCompleted = false,
        .enableDifferentialConversion = false,
    };

    if (result == kMotorCurrentCalibrationPhaseBStarted) {
        ADC16_SetChannelConfig(ADC1, 0U, &channel_config);
        channel_config.channelNumber = 9U;
        channel_config.enableInterruptOnConversionCompleted = true;
        ADC16_SetChannelConfig(ADC0, 0U, &channel_config);
    } else if (result == kMotorCurrentCalibrationFinished) {
        ADC16_SetChannelConfig(ADC0, 0U, &channel_config);
    }

    return result;
}

/**
 * @brief Selects the four PDB-triggered ADC channels used during motor control.
 *
 * ADC0 samples phase B and the board-selected auxiliary input. ADC1 samples phase A and starts
 * the alternating auxiliary sequence on channel two.
 *
 * @param auxiliary_channel Board-selected ADC0 auxiliary channel, either four or seven.
 */
void motor_adc_runtime_initialize(uint32_t auxiliary_channel) {
    SIM->SOPT7 = 0U;
    adc16_channel_config_t channel_config = {
        .channelNumber = 9U,
        .enableInterruptOnConversionCompleted = true,
        .enableDifferentialConversion = false,
    };

    ADC16_SetChannelConfig(ADC0, 0U, &channel_config);
    channel_config.channelNumber = 10U;
    channel_config.enableInterruptOnConversionCompleted = false;
    ADC16_SetChannelConfig(ADC1, 0U, &channel_config);
    adc0_auxiliary_channel = auxiliary_channel;
    adc_auxiliary_first_sample = 0U;
    adc_auxiliary_second_sample = false;
    channel_config.channelNumber = adc0_auxiliary_channel;
    ADC16_SetChannelConfig(ADC0, 1U, &channel_config);
    channel_config.channelNumber = 2U;
    ADC16_SetChannelConfig(ADC1, 1U, &channel_config);
    motor_adc_trigger_enable();
}

bool motor_adc_auxiliary_capture(MotorAdcAuxiliarySamples *samples) {
    bool pair_complete = adc_auxiliary_second_sample;
    if (pair_complete) {
        samples->motor = adc_auxiliary_first_sample;
        samples->driver = (uint16_t)ADC1->R[1];
    } else {
        adc_auxiliary_first_sample = (uint16_t)ADC1->R[1];
    }

    return pair_complete;
}

void motor_adc_auxiliary_rearm(void) {
    adc16_channel_config_t channel_config = {
        .channelNumber = 9U,
        .enableInterruptOnConversionCompleted = true,
        .enableDifferentialConversion = false,
    };
    ADC16_SetChannelConfig(ADC0, 0U, &channel_config);
    channel_config.channelNumber = 10U;
    channel_config.enableInterruptOnConversionCompleted = false;
    ADC16_SetChannelConfig(ADC1, 0U, &channel_config);
    channel_config.channelNumber = adc0_auxiliary_channel;
    ADC16_SetChannelConfig(ADC0, 1U, &channel_config);
    channel_config.channelNumber = adc_auxiliary_second_sample ? 0U : 2U;
    ADC16_SetChannelConfig(ADC1, 1U, &channel_config);
    adc_auxiliary_second_sample = !adc_auxiliary_second_sample;
}

/**
 * @brief Configures the reset-pin filter for run, wait, and stop operation.
 *
 * The recovered reset filter mode and maximum filter width are applied without changing other RCM
 * fields.
 */
void motor_reset_filter_initialize(void) {
    RCM->RPFC |= RCM_RPFC_RSTFLTSRW(1U);
    RCM->RPFW |= RCM_RPFW_RSTFLTSEL(31U);
}

/**
 * @brief Enables the eight interrupt sources and priorities used by motor firmware.
 *
 * ADC, PDB, I2C, FTM, and combined-port vectors receive the priorities recovered from both images.
 */
void motor_interrupts_initialize(void) {
    EnableIRQ(ADC1_IRQn);
    NVIC_SetPriority(ADC1_IRQn, 2U);
    EnableIRQ(ADC0_IRQn);
    NVIC_SetPriority(ADC0_IRQn, 0U);
    EnableIRQ(PDB0_PDB1_IRQn);
    NVIC_SetPriority(PDB0_PDB1_IRQn, 10U);
    EnableIRQ(I2C0_IRQn);
    NVIC_SetPriority(I2C0_IRQn, 0U);
    EnableIRQ(FTM3_IRQn);
    NVIC_SetPriority(FTM3_IRQn, 13U);
    EnableIRQ(PORTB_PORTC_PORTD_PORTE_IRQn);
    NVIC_SetPriority(PORTB_PORTC_PORTD_PORTE_IRQn, 1U);
    EnableIRQ(FTM2_IRQn);
    NVIC_SetPriority(FTM2_IRQn, 1U);
    EnableIRQ(FTM4_IRQn);
    NVIC_SetPriority(FTM4_IRQn, 1U);
}

/**
 * @brief Configures masked complementary PWM for all three motor phases.
 *
 * FTM0 receives the recovered period, dead time, complementary pairs, fault behavior, initial
 * compares, synchronization, and masked startup state.
 */
void motor_pwm_initialize(void) {
    CLOCK_EnableClock(kCLOCK_Ftm0);

    FTM0->OUTMASK = 0x3fU;
    FTM0->MODE |= FTM_MODE_WPDIS_MASK | FTM_MODE_FTMEN_MASK;
    FTM0->MODE = (FTM0->MODE & ~FTM_MODE_FAULTM_MASK) | FTM_MODE_FAULTM(2U);
    FTM0->CONF |= FTM_CONF_BDMMODE(3U);
    FTM0->MOD = 2249U;
    FTM0->CNTIN = 0xfffff736U;
    FTM0->SYNC |= FTM_SYNC_CNTMAX_MASK;
    FTM0->COMBINE |= FTM_COMBINE_COMBINE0_MASK | FTM_COMBINE_COMP0_MASK | FTM_COMBINE_DTEN0_MASK |
                     FTM_COMBINE_SYNCEN0_MASK | FTM_COMBINE_FAULTEN0_MASK |
                     FTM_COMBINE_COMBINE1_MASK | FTM_COMBINE_COMP1_MASK | FTM_COMBINE_DTEN1_MASK |
                     FTM_COMBINE_SYNCEN1_MASK | FTM_COMBINE_FAULTEN1_MASK |
                     FTM_COMBINE_COMBINE2_MASK | FTM_COMBINE_COMP2_MASK | FTM_COMBINE_DTEN2_MASK |
                     FTM_COMBINE_SYNCEN2_MASK | FTM_COMBINE_FAULTEN2_MASK;
    FTM0->DEADTIME = (FTM0->DEADTIME & ~FTM_DEADTIME_DTVAL_MASK) | FTM_DEADTIME_DTVAL(50U);

    FTM0->CONTROLS[0].CnV = 0xfffffb9bU;
    FTM0->CONTROLS[1].CnV = 1125U;
    FTM0->CONTROLS[2].CnV = 0xfffffb9bU;
    FTM0->CONTROLS[3].CnV = 1125U;
    FTM0->CONTROLS[4].CnV = 0xfffffb9bU;
    FTM0->CONTROLS[5].CnV = 1125U;

    for (uint32_t channel = 0U; channel < 6U; ++channel) {
        FTM0->CONTROLS[channel].CnSC |= FTM_CnSC_ELSB_MASK;
    }

    FTM0->PWMLOAD |= FTM_PWMLOAD_LDOK_MASK;
    FTM0->EXTTRIG |= FTM_EXTTRIG_INITTRIGEN_MASK;
    FTM0->MODE |= FTM_MODE_INIT_MASK;
    FTM_StartTimer(FTM0, kFTM_SystemClock);
}

/**
 * @brief Unmasks all six PWM outputs for zero-duty current-offset calibration.
 *
 * The output mask is cleared only after zero-duty compares have been installed.
 */
void motor_pwm_enable_outputs(void) { FTM0->OUTMASK = 0U; }

/**
 * @brief Configures the FTM2 motor scheduling timer.
 *
 * The timer runs as a filtered quadrature decoder and retains callbacks for overflow and one-shot
 * encoder-index events.
 *
 * @param modulus Runtime timer period selected by the motor configuration.
 * @param handler Function invoked with the quadrature overflow direction.
 * @param index_handler Function invoked with the counter captured at the encoder index.
 * @param context Caller context passed to the overflow handler.
 */
void motor_tick_timer_initialize(uint16_t modulus, MotorEncoderOverflowHandler handler,
                                 MotorEncoderIndexHandler index_handler, void *context) {
    encoder_overflow_handler = handler;
    encoder_overflow_context = context;
    encoder_index_handler = index_handler;
    encoder_index_context = context;
    encoder_revolution_armed = false;
    encoder_revolution_complete = false;
    CLOCK_EnableClock(kCLOCK_Ftm2);
    FTM2->MODE = FTM_MODE_FTMEN_MASK;
    FTM2->SYNCONF = 0U;
    FTM2->MOD = modulus;
    FTM2->CNTIN = 0U;
    FTM2->EXTTRIG = 0U;
    FTM2->CONF = 0xc0U;
    FTM2->QDCTRL = FTM_QDCTRL_QUADEN_MASK | FTM_QDCTRL_PHBFLTREN_MASK | FTM_QDCTRL_PHAFLTREN_MASK;
    FTM2->FILTER = 0U;
    FTM2->SC = 8U;
}

/**
 * @brief Enables the official FTM2 overflow interrupt after the startup current ramp.
 *
 * The hardware count and calibration-revolution state are cleared before overflow extension starts.
 */
void motor_encoder_overflow_interrupt_enable(void) {
    FTM2->CNT = 0U;
    encoder_revolution_armed = false;
    encoder_revolution_complete = false;
    FTM_ClearStatusFlags(FTM2, kFTM_TimeOverflowFlag);
    FTM_EnableInterrupts(FTM2, kFTM_TimeOverflowInterruptEnable);
}

/**
 * @brief Arms the official encoder calibration full-revolution event.
 *
 * The next FTM2 overflow publishes the one-shot revolution completion state.
 */
void motor_encoder_revolution_arm(void) { encoder_revolution_armed = true; }

/**
 * @brief Clears the encoder calibration full-revolution capture.
 *
 * Both the one-shot arm and completion state are released after each captured sweep, matching the
 * two flags cleared by both official motor images.
 */
void motor_encoder_revolution_clear(void) {
    encoder_revolution_armed = false;
    encoder_revolution_complete = false;
}

/**
 * @brief Reads the official encoder calibration full-revolution event.
 *
 * The event remains set until the calibration state machine explicitly clears it.
 *
 * @return True after an armed FTM2 overflow.
 */
bool motor_encoder_revolution_is_complete(void) { return encoder_revolution_complete; }

/**
 * @brief Enables the official falling-edge encoder-index interrupt on PORTE24.
 *
 * Any stale port flag is cleared before the one-shot falling-edge trigger is armed.
 */
void motor_encoder_index_interrupt_enable(void) {
    PORT_ClearPinsInterruptFlags(PORTE, 1UL << 24U);
    PORT_SetPinInterruptConfig(PORTE, 24U, kPORT_InterruptFallingEdge);
}

/**
 * @brief Disables and clears the official encoder-index interrupt on PORTE24.
 *
 * The port trigger is removed and any captured flag is discarded.
 */
void motor_encoder_index_interrupt_disable(void) {
    PORT_SetPinInterruptConfig(PORTE, 24U, kPORT_InterruptOrDMADisabled);
    PORT_ClearPinsInterruptFlags(PORTE, 1UL << 24U);
}

/**
 * @brief Extends the official FTM2 quadrature count in its hardware overflow direction.
 *
 * Overflow status is cleared before the runtime position and optional calibration revolution event
 * are published.
 */
void FTM2_IRQHandler(void) {
    bool increasing = (FTM2->QDCTRL & FTM_QDCTRL_TOFDIR_MASK) != 0U;
    FTM_ClearStatusFlags(FTM2, kFTM_TimeOverflowFlag);
    if (encoder_overflow_handler != NULL) {
        encoder_overflow_handler(increasing, encoder_overflow_context);
    }
    if (encoder_revolution_armed) {
        encoder_revolution_complete = true;
    }
}

/**
 * @brief Captures and dispatches the official PORTE24 encoder-index interrupt.
 *
 * Only the encoder pin is handled; its counter is captured before the one-shot interrupt is
 * disabled and forwarded.
 */
void PORTB_PORTC_PORTD_PORTE_IRQHandler(void) {
    if ((PORT_GetPinsInterruptFlags(PORTE) & (1UL << 24U)) == 0U) {
        return;
    }

    uint16_t counter = (uint16_t)FTM2->CNT;
    motor_encoder_index_interrupt_disable();
    if (encoder_index_handler != NULL) {
        encoder_index_handler(counter, encoder_index_context);
    }
}

/**
 * @brief Configures one official periodic FTM channel.
 *
 * The timer uses the system clock, divide-by-two prescaling, three overflow repetitions, and an
 * enabled overflow interrupt.
 *
 * @param timer FTM peripheral to configure.
 * @param clock Clock gate associated with the FTM peripheral.
 * @param modulus Timer modulus in peripheral clock counts.
 */
static void motor_periodic_timer_initialize(FTM_Type *timer, clock_ip_name_t clock,
                                            uint16_t modulus) {
    CLOCK_EnableClock(clock);
    timer->MODE |= FTM_MODE_FTMEN_MASK;
    timer->CNTIN = 0U;
    timer->CNT = 0U;
    timer->MOD = modulus;
    timer->SC = 0U;
    timer->SC = FTM_SC_CLKS(1U);
    timer->SC |= FTM_SC_TOIE_MASK;
    timer->MODE |= FTM_MODE_WPDIS_MASK;
    timer->MODE |= FTM_MODE_INIT_MASK;
    timer->MODE |= FTM_MODE_PWMSYNC_MASK;
    timer->CONF = FTM_CONF_NUMTOF(3U);
    timer->SC = (timer->SC & ~FTM_SC_PS_MASK) | FTM_SC_PS(1U);
}

/**
 * @brief Configures the FTM3 periodic motor-service interrupt at modulus 9000.
 *
 * The recovered shared periodic-timer configuration is bound to the runtime service callback.
 *
 * @param handler Function invoked for each service period.
 * @param context Caller context passed to the service handler.
 */
void motor_service_timer_initialize(MotorTimerHandler handler, void *context) {
    service_timer_handler = handler;
    service_timer_context = context;
    motor_periodic_timer_initialize(FTM3, kCLOCK_Ftm3, 9000U);
}

/**
 * @brief Configures the FTM4 communication-timeout interrupt at modulus 3600.
 *
 * The recovered shared periodic-timer configuration is bound to delayed SPI response service.
 *
 * @param handler Function invoked for each communication period.
 * @param context Caller context passed to the communication handler.
 */
void motor_communication_timeout_timer_initialize(MotorTimerHandler handler, void *context) {
    communication_timer_handler = handler;
    communication_timer_context = context;
    motor_periodic_timer_initialize(FTM4, kCLOCK_Ftm4, 3600U);
}

/**
 * @brief Clears the official FTM3 overflow and runs one periodic motor-service step.
 *
 * The registered service callback executes once for each acknowledged overflow.
 */
void FTM3_IRQHandler(void) {
    FTM_ClearStatusFlags(FTM3, kFTM_TimeOverflowFlag);
    if (service_timer_handler != NULL) {
        service_timer_handler(service_timer_context);
    }
}

/**
 * @brief Runs the official delayed communication step and clears the FTM4 overflow.
 *
 * Response scheduling executes before the timer overflow is acknowledged.
 */
void FTM4_IRQHandler(void) {
    if (communication_timer_handler != NULL) {
        communication_timer_handler(communication_timer_context);
    }
    FTM_ClearStatusFlags(FTM4, kFTM_TimeOverflowFlag);
}
