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
};

static volatile uint16_t adc_primary[ANALOG_SCAN_SAMPLE_COUNT];
static volatile uint16_t adc_secondary[ANALOG_SCAN_SAMPLE_COUNT];
static volatile uint8_t adc_ready = ADC_READY_NONE;

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

void __attribute__((interrupt, no_auto_psv)) _DMA0Interrupt(void) {
    adc_ready = DMAPPSbits.PPST0 == 0 ? 1 : 0;
    IFS0bits.DMA0IF = 0;
}
