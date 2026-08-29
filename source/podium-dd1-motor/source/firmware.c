#include <freemaster.h>
#include <freemaster_serial_uart.h>
#include <fsl_common.h>

#include "common/motor/runtime.h"

/**
 * @brief Services the UART0 FreeMASTER transport.
 */
void UART0_IRQHandler(void) { FMSTR_SerialIsr(); }

/**
 * @brief Initializes and runs the DD1 motor firmware.
 *
 * Motor control owns the foreground loop while FreeMASTER processes UART0 commands in its
 * interrupt handler.
 */
void firmware_main(void) {
    FMSTR_Init();
    motor_runtime_initialize();
    EnableIRQ(UART0_IRQn);

    for (;;) {
        motor_runtime_poll();
    }
}
