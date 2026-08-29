#include "common/motor/board.h"

#include <fsl_gpio.h>
#include <fsl_port.h>

static void motor_filtered_pullup_input_initialize(PORT_Type *port, GPIO_Type *gpio, uint32_t pin) {
    PORT_SetPinMux(port, pin, kPORT_MuxAsGpio);
    gpio->PDDR &= ~(1UL << pin);
    port->PCR[pin] |= PORT_PCR_PE_MASK | PORT_PCR_PS_MASK | PORT_PCR_PFE_MASK;
}

static void motor_gpio_output_initialize(PORT_Type *port, GPIO_Type *gpio, uint32_t pin,
                                         bool high) {
    PORT_SetPinMux(port, pin, kPORT_MuxAsGpio);
    gpio->PDDR |= 1UL << pin;
    if (high) {
        GPIO_PortSet(gpio, 1UL << pin);
    }
}

/**
 * @brief Configures all motor-controller GPIO, PWM, serial, and timer pins.
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

    PORTD->PCR[7] |= PORT_PCR_PE_MASK | PORT_PCR_PS_MASK | PORT_PCR_PFE_MASK;
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
