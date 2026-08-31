#include "platform/board.h"

#include <fsl_common.h>
#include <fsl_gpio.h>
#include <fsl_port.h>

enum {
    MOTOR_CLOCK_STATUS_TIMEOUT = 1000000U,
};

static bool motor_clock_status_wait(uint8_t mask, uint8_t expected) {
    uint32_t remaining = MOTOR_CLOCK_STATUS_TIMEOUT;
    while ((MCG->S & mask) != expected && remaining != 0U) {
        --remaining;
    }
    return remaining != 0U;
}

void SystemInitHook(void) {
    if ((RCM->SRS0 & RCM_SRS0_WAKEUP_MASK) != 0U && (PMC->REGSC & PMC_REGSC_ACKISO_MASK) != 0U) {
        PMC->REGSC |= PMC_REGSC_ACKISO_MASK;
    }

    SMC->PMPROT = SMC_PMPROT_AVLP_MASK | SMC_PMPROT_AVLLS_MASK;
    SIM->CLKDIV1 = SIM_CLKDIV1_OUTDIV4(2U);
    SIM->SOPT1 &= ~SIM_SOPT1_OSC32KSEL_MASK;
    SIM->SCGC5 |= SIM_SCGC5_PORTA_MASK;
    PORTA->PCR[18] &= ~(PORT_PCR_ISF_MASK | PORT_PCR_MUX_MASK);
    MCG->SC = 0U;
    MCG->C1 = MCG_C1_IRCLKEN_MASK | MCG_C1_IREFS_MASK;
    if (!motor_clock_status_wait(MCG_S_IREFST_MASK, MCG_S_IREFST_MASK)) {
        NVIC_SystemReset();
    }
    MCG->C2 &= ~MCG_C2_FCFTRIM_MASK;
    MCG->C4 = (MCG->C4 & ~(MCG_C4_DMX32_MASK | MCG_C4_DRST_DRS_MASK)) | MCG_C4_DMX32_MASK |
              MCG_C4_DRST_DRS(2U);
    OSC0->CR = OSC_CR_ERCLKEN_MASK;
    MCG->C6 = 0U;
    if (!motor_clock_status_wait(MCG_S_CLKST_MASK, 0U)) {
        NVIC_SystemReset();
    }
}

/**
 * @brief Configures one official hardware-strap input.
 *
 * The pin uses the GPIO mux with an enabled pull-up and passive input filter.
 *
 * @param port Port-control block for the pin.
 * @param gpio GPIO block containing the pin direction.
 * @param pin Zero-based pin number.
 */
static void motor_filtered_pullup_input_initialize(PORT_Type *port, GPIO_Type *gpio, uint32_t pin) {
    PORT_SetPinMux(port, pin, kPORT_MuxAsGpio);
    gpio->PDDR &= ~(1UL << pin);
    port->PCR[pin] |= PORT_PCR_PE_MASK | PORT_PCR_PS_MASK | PORT_PCR_PFE_MASK;
}

/**
 * @brief Configures one official digital output without exposing an intermediate level.
 *
 * The requested output latch is installed before the pin direction changes to output.
 *
 * @param port Port-control block for the pin.
 * @param gpio GPIO block containing the output and direction registers.
 * @param pin Zero-based pin number.
 * @param high True to preset a high output, or false to preset a low output.
 */
static void motor_gpio_output_initialize(PORT_Type *port, GPIO_Type *gpio, uint32_t pin,
                                         bool high) {
    PORT_SetPinMux(port, pin, kPORT_MuxAsGpio);
    if (high) {
        GPIO_PortSet(gpio, 1UL << pin);
    } else {
        GPIO_PortClear(gpio, 1UL << pin);
    }
    gpio->PDDR |= 1UL << pin;
}

/**
 * @brief Configures all motor-controller GPIO, PWM, serial, and timer pins.
 *
 * Output latch levels are established before direction changes, then each pin receives its
 * recovered mux, drive strength, filter, and pull configuration.
 */
void motor_pins_initialize(void) {
    CLOCK_EnableClock(kCLOCK_PortA);
    CLOCK_EnableClock(kCLOCK_PortB);
    CLOCK_EnableClock(kCLOCK_PortC);
    CLOCK_EnableClock(kCLOCK_PortD);
    CLOCK_EnableClock(kCLOCK_PortE);

    motor_filtered_pullup_input_initialize(PORTC, GPIOC, 3U);
    motor_filtered_pullup_input_initialize(PORTE, GPIOE, 17U);
    motor_filtered_pullup_input_initialize(PORTE, GPIOE, 20U);
    motor_filtered_pullup_input_initialize(PORTC, GPIOC, 4U);
    motor_filtered_pullup_input_initialize(PORTE, GPIOE, 29U);

    motor_gpio_output_initialize(PORTA, GPIOA, 19U, true);
    motor_gpio_output_initialize(PORTC, GPIOC, 1U, true);

    PORT_SetPinMux(PORTC, 5U, kPORT_MuxAlt2);
    PORT_SetPinMux(PORTD, 7U, kPORT_MuxAlt7);
    PORT_SetPinMux(PORTD, 6U, kPORT_MuxAlt7);
    motor_gpio_output_initialize(PORTC, GPIOC, 0U, true);

    PORTD->PCR[7] &= ~PORT_PCR_PS_MASK;
    PORTD->PCR[7] |= PORT_PCR_PE_MASK | PORT_PCR_PFE_MASK;
    PORTD->PCR[6] |= PORT_PCR_DSE_MASK | PORT_PCR_PFE_MASK;

    PORT_SetPinMux(PORTC, 6U, kPORT_MuxAlt7);
    PORT_SetPinMux(PORTC, 7U, kPORT_MuxAlt7);

    for (uint32_t pin = 0U; pin < 6U; ++pin) {
        PORT_SetPinMux(PORTD, pin, kPORT_MuxAlt4);
    }

    PORT_SetPinMux(PORTA, 1U, kPORT_MuxAlt5);
    PORT_SetPinMux(PORTA, 2U, kPORT_MuxAlt5);

    PORT_SetPinMux(PORTE, 24U, kPORT_MuxAsGpio);
    GPIOE->PDDR &= ~(1UL << 24U);
    motor_gpio_output_initialize(PORTB, GPIOB, 17U, false);
    motor_gpio_output_initialize(PORTB, GPIOB, 16U, false);
}

/**
 * @brief Reads the five hardware straps and packs the motor board identity byte.
 *
 * Active-low strap samples occupy the recovered identity bit positions with fixed marker bits.
 *
 * @return Identity byte with fixed marker bits in positions zero and seven.
 */
uint8_t motor_board_identity_read(void) {
    uint8_t identity = 0x81U;
    identity |= (uint8_t)(((GPIOC->PDIR >> 3U) & 1U) << 2U);
    identity |= (uint8_t)(((GPIOE->PDIR >> 17U) & 1U) << 3U);
    identity |= (uint8_t)(((GPIOE->PDIR >> 20U) & 1U) << 4U);
    identity |= (uint8_t)(((GPIOC->PDIR >> 4U) & 1U) << 5U);
    identity |= (uint8_t)(((GPIOE->PDIR >> 29U) & 1U) << 6U);
    return identity;
}

/**
 * @brief Applies the two official active-low startup interlock outputs.
 *
 * Each logical interlock state is translated to the corresponding board GPIO level.
 *
 * @param interlock_a True to pull GPIOC1 low, or false to release it high.
 * @param interlock_b True to pull GPIOA19 low, or false to release it high.
 */
void motor_startup_interlock_outputs_apply(bool interlock_a, bool interlock_b) {
    if (interlock_a) {
        GPIO_PortClear(GPIOC, 1UL << 1U);
    } else {
        GPIO_PortSet(GPIOC, 1UL << 1U);
    }

    if (interlock_b) {
        GPIO_PortClear(GPIOA, 1UL << 19U);
    } else {
        GPIO_PortSet(GPIOA, 1UL << 19U);
    }
}
