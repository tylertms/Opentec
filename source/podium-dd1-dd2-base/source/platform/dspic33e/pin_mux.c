#include "platform/pin_mux.h"

#include <stdint.h>
#include <xc.h>

enum {
    PERIPHERAL_PIN_SELECT_LOCK = 0x40,
    UART2_RECEIVE_PIN = 96,
    UART3_RECEIVE_PIN = 104,
    CAN1_RECEIVE_PIN = 71,
    INPUT_CAPTURE_1_PIN = 17,
    INPUT_CAPTURE_3_PIN = 78,
    SPI1_DATA_INPUT_PIN = 119,
    SPI1_CLOCK_INPUT_PIN = 118,
    SPI1_SELECT_INPUT_PIN = 121,
    OUTPUT_UART2_TRANSMIT = 3,
    OUTPUT_SPI1_DATA = 5,
    OUTPUT_CAN1_TRANSMIT = 14,
    OUTPUT_COMPARE_1 = 16,
    OUTPUT_COMPARE_2 = 17,
    OUTPUT_COMPARE_5 = 20,
    OUTPUT_UART3_TRANSMIT = 27,
    OUTPUT_SPI4_DATA = 34,
    OUTPUT_SPI4_CLOCK = 35,
    OUTPUT_SPI4_SELECT = 36,
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
