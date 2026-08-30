#include "platform/adc.h"

#include <stdbool.h>
#include <stdint.h>
#include <xc.h>

enum {
    ADC_SCAN_CHANNELS = 0xff41,
    ADC_SAMPLE_COUNT = ANALOG_SCAN_SAMPLE_COUNT - 1,
    ADC_SAMPLE_TIME = 31,
    ADC_CONVERSION_CLOCK = 63,
    ADC_DMA_REQUEST = 13,
    ADC_DMA_PRIORITY = 2,
    ADC_READY_NONE = 2,
    ADC_PRIMARY_SHIFTER_Y_INDEX = 3,
    ADC_SECONDARY_SHIFTER_Y_INDEX = 5,
    ADC_SHIFTER_AVERAGE_SAMPLE_COUNT = 200,
};

static volatile uint16_t adc_primary[ANALOG_SCAN_SAMPLE_COUNT];
static volatile uint16_t adc_secondary[ANALOG_SCAN_SAMPLE_COUNT];
static volatile uint8_t adc_ready = ADC_READY_NONE;
static volatile uint8_t adc_latest;

/**
 * @brief Configures continuous ten-channel ADC sampling through DMA channel zero.
 *
 * Enables 12-bit automatic scan conversion for the analog inputs selected by mask 0xff41 and
 * transfers each ten-sample scan into alternating DMA buffers. The DMA completion interrupt runs
 * at priority two and records the completed buffer for foreground processing.
 */
void platform_adc_init(void) {
    AD1CON1 = 0;
    AD1CON1bits.AD12B = 1;
    AD1CON1bits.SSRC = 7;
    AD1CON1bits.ASAM = 1;
    AD1CON1bits.ADDMABM = 1;

    AD1CON2 = 0;
    AD1CON2bits.CSCNA = 1;
    AD1CON2bits.SMPI = ADC_SAMPLE_COUNT;

    AD1CON3 = 0;
    AD1CON3bits.SAMC = ADC_SAMPLE_TIME;
    AD1CON3bits.ADCS = ADC_CONVERSION_CLOCK;

    AD1CON4 = 0;
    AD1CON4bits.DMABL = 1;
    AD1CON4bits.ADDMAEN = 1;
    AD1CSSH = 0;
    AD1CSSL = ADC_SCAN_CHANNELS;

    ANSELBbits.ANSB0 = 1;
    ANSELBbits.ANSB6 = 1;
    ANSELBbits.ANSB8 = 1;
    ANSELBbits.ANSB9 = 1;
    ANSELBbits.ANSB10 = 1;

    IEC0bits.AD1IE = 0;
    IFS0bits.AD1IF = 0;

    DMA0CON = 0;
    DMA0CONbits.AMODE = 0;
    DMA0CONbits.MODE = 2;
    DMA0PAD = (uint16_t)&ADC1BUF0;
    DMA0CNT = ADC_SAMPLE_COUNT;
    DMA0REQbits.IRQSEL = ADC_DMA_REQUEST;
    DMA0STAL = __builtin_dmaoffset(adc_primary);
    DMA0STAH = 0;
    DMA0STBL = __builtin_dmaoffset(adc_secondary);
    DMA0STBH = 0;

    IPC1bits.DMA0IP = ADC_DMA_PRIORITY;
    IFS0bits.DMA0IF = 0;
    IEC0bits.DMA0IE = 1;
    DMA0CONbits.CHEN = 1;
    AD1CON1bits.ADON = 1;
}

/**
 * @brief Retrieves the most recently completed analog scan.
 *
 * Atomically consumes the completed-buffer indication and decodes that DMA buffer into logical
 * analog inputs. A later scan may replace an unread scan so foreground processing always receives
 * the newest complete sample set.
 *
 * @param[out] samples Destination for the decoded analog inputs.
 * @return True when a completed scan was available; otherwise false.
 */
bool platform_adc_read(AnalogSamples *samples) {
    IEC0bits.DMA0IE = 0;
    uint8_t ready = adc_ready;
    adc_ready = ADC_READY_NONE;
    IEC0bits.DMA0IE = 1;

    if (ready == ADC_READY_NONE) {
        return false;
    }
    analog_samples_decode(ready == 0 ? adc_primary : adc_secondary, samples);
    return true;
}

/**
 * @brief Averages the active H-pattern shifter's longitudinal input.
 *
 * Accumulates two hundred reads from the newest completed scan buffer. A DMA completion may select
 * a newer buffer between reads so the result follows the continuously sampled input.
 *
 * @param[in] secondary True for the secondary shifter port, false for the primary port.
 * @return Arithmetic mean of the selected longitudinal input.
 */
uint16_t platform_adc_average_shifter_y(bool secondary) {
    uint32_t total = 0;
    uint8_t index = secondary ? ADC_SECONDARY_SHIFTER_Y_INDEX : ADC_PRIMARY_SHIFTER_Y_INDEX;
    for (uint16_t sample = 0; sample < ADC_SHIFTER_AVERAGE_SAMPLE_COUNT; sample++) {
        const volatile uint16_t *scan = adc_latest == 0 ? adc_primary : adc_secondary;
        total += scan[index];
    }
    return (uint16_t)(total / ADC_SHIFTER_AVERAGE_SAMPLE_COUNT);
}

/**
 * @brief Publishes the DMA buffer completed by the latest ADC scan.
 *
 * Selects the inactive ping-pong buffer indicated by the DMA controller and clears the channel-zero
 * interrupt request.
 */
void __attribute__((interrupt, no_auto_psv)) _DMA0Interrupt(void) {
    adc_ready = DMAPPSbits.PPST0 == 0 ? 1 : 0;
    adc_latest = adc_ready;
    IFS0bits.DMA0IF = 0;
}
