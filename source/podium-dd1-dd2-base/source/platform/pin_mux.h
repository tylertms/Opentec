#ifndef OPENTEC_BASE_PLATFORM_PIN_MUX_H
#define OPENTEC_BASE_PLATFORM_PIN_MUX_H

/**
 * @brief Initializes remappable peripheral pins.
 *
 * Assigns the UART, CAN, input-capture, SPI, and output-compare signals used by the platform.
 */
void platform_pin_mux_init(void);

#endif
