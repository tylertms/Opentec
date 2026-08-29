#include "common/motor/peripherals.h"

#include <fsl_adc16.h>
#include <fsl_ftm.h>
#include <fsl_pdb.h>

/**
 * @brief Calibrates and configures both motor-current ADCs for PDB triggering.
 */
void motor_adc_initialize(void) {
    adc16_config_t config;

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

    SIM->SOPT7 = SIM_SOPT7_ADC0TRGSEL(8U) | SIM_SOPT7_ADC0ALTTRGEN_MASK | SIM_SOPT7_ADC1TRGSEL(8U) |
                 SIM_SOPT7_ADC1ALTTRGEN_MASK;

    adc16_channel_config_t channel_config = {
        .channelNumber = 10U,
        .enableInterruptOnConversionCompleted = true,
        .enableDifferentialConversion = false,
    };
    ADC16_SetChannelConfig(ADC1, 0U, &channel_config);
}

/**
 * @brief Configures the PDB timing used to trigger both ADC modules.
 */
void motor_adc_trigger_initialize(void) {
    pdb_config_t config;

    CLOCK_EnableClock(kCLOCK_Pdb0);
    PDB_SetModulusValue(PDB0, 1500U);
    PDB_SetCounterDelayValue(PDB0, 750U);
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
 * @brief Configures masked complementary PWM for all three motor phases.
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
        FTM0->CONTROLS[channel].CnSC |= FTM_CnSC_ELSA_MASK;
    }

    FTM0->PWMLOAD |= FTM_PWMLOAD_LDOK_MASK;
    FTM0->EXTTRIG |= FTM_EXTTRIG_INITTRIGEN_MASK;
    FTM0->MODE |= FTM_MODE_INIT_MASK;
    FTM_StartTimer(FTM0, kFTM_SystemClock);
}

/**
 * @brief Unmasks all six PWM outputs after current-offset calibration.
 */
void motor_pwm_enable_outputs(void) { FTM0->OUTMASK = 0U; }

/**
 * @brief Configures the FTM2 motor scheduling timer.
 * @param modulus Runtime timer period selected by the motor configuration.
 */
void motor_tick_timer_initialize(uint16_t modulus) {
    CLOCK_EnableClock(kCLOCK_Ftm2);
    FTM2->MODE = FTM_MODE_FTMEN_MASK;
    FTM2->SYNCONF = 0xc0U;
    FTM2->MOD = modulus;
    FTM2->CNTIN = 0U;
    FTM2->EXTTRIG = 0U;
    FTM2->CONF = FTM_CONF_BDMMODE(3U) | FTM_CONF_NUMTOF(1U);
    FTM2->SC &= ~FTM_SC_TOF_MASK;
    FTM_StartTimer(FTM2, kFTM_SystemClock);
}
