#include "platform/pin_mux.h"

#include <stdint.h>
#include <xc.h>

/**
 * @brief Remappable peripheral input, output, and lock values.
 */
enum {
    PERIPHERAL_PIN_SELECT_LOCK = 0x40, /**< OSCCON peripheral-pin-select lock bit. */
    UART2_RECEIVE_PIN = 96,            /**< Remappable input number for UART2 receive. */
    UART3_RECEIVE_PIN = 104,           /**< Remappable input number for UART3 receive. */
    CAN1_RECEIVE_PIN = 71,             /**< Remappable input number for CAN1 receive. */
    INPUT_CAPTURE_1_PIN = 17,          /**< Remappable input number for Input Capture 1. */
    INPUT_CAPTURE_3_PIN = 78,          /**< Remappable input number for Input Capture 3. */
    SPI1_DATA_INPUT_PIN = 119,         /**< Remappable input number for SPI1 data. */
    SPI1_CLOCK_INPUT_PIN = 118,        /**< Remappable input number for SPI1 clock. */
    SPI1_SELECT_INPUT_PIN = 121,       /**< Remappable input number for SPI1 slave select. */
    OUTPUT_UART2_TRANSMIT = 3,         /**< Remappable output function for UART2 transmit. */
    OUTPUT_SPI1_DATA = 5,              /**< Remappable output function for SPI1 data. */
    OUTPUT_CAN1_TRANSMIT = 14,         /**< Remappable output function for CAN1 transmit. */
    OUTPUT_COMPARE_1 = 16,             /**< Remappable output function for Output Compare 1. */
    OUTPUT_COMPARE_2 = 17,             /**< Remappable output function for Output Compare 2. */
    OUTPUT_COMPARE_5 = 20,             /**< Remappable output function for Output Compare 5. */
    OUTPUT_UART3_TRANSMIT = 27,        /**< Remappable output function for UART3 transmit. */
    OUTPUT_SPI4_DATA = 34,             /**< Remappable output function for SPI4 data. */
    OUTPUT_SPI4_CLOCK = 35,            /**< Remappable output function for SPI4 clock. */
    OUTPUT_SPI4_SELECT = 36,           /**< Remappable output function for SPI4 slave select. */
};

/**
 * @brief Assigns remappable peripheral inputs and outputs.
 *
 * Unlocks peripheral pin selection, maps the UART, CAN, input-capture, and SPI inputs and outputs
 * used by the board, then locks the assignments against accidental changes.
 */
void platform_pin_mux_init(void) {
    __builtin_write_OSCCONL(OSCCON & (uint16_t)~PERIPHERAL_PIN_SELECT_LOCK);

    RPINR19bits.U2RXR = UART2_RECEIVE_PIN;
    RPINR27bits.U3RXR = UART3_RECEIVE_PIN;
    RPINR26bits.C1RXR = CAN1_RECEIVE_PIN;
    RPINR7bits.IC1R = INPUT_CAPTURE_1_PIN;
    RPINR8bits.IC3R = INPUT_CAPTURE_3_PIN;
    RPINR20bits.SDI1R = SPI1_DATA_INPUT_PIN;
    RPINR20bits.SCK1R = SPI1_CLOCK_INPUT_PIN;
    RPINR21bits.SS1R = SPI1_SELECT_INPUT_PIN;

    RPOR7bits.RP97R = OUTPUT_UART2_TRANSMIT;
    RPOR8bits.RP98R = OUTPUT_UART3_TRANSMIT;
    RPOR3bits.RP70R = OUTPUT_CAN1_TRANSMIT;
    RPOR11bits.RP108R = OUTPUT_COMPARE_5;
    RPOR12bits.RP109R = OUTPUT_COMPARE_1;
    RPOR0bits.RP65R = OUTPUT_COMPARE_2;
    RPOR5bits.RP84R = OUTPUT_SPI4_SELECT;
    RPOR5bits.RP82R = OUTPUT_SPI4_CLOCK;
    RPOR4bits.RP80R = OUTPUT_SPI4_DATA;
    RPOR14bits.RP120R = OUTPUT_SPI1_DATA;

    __builtin_write_OSCCONL(OSCCON | PERIPHERAL_PIN_SELECT_LOCK);
}
