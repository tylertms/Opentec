#ifndef FREEMASTER_CFG_H
#define FREEMASTER_CFG_H

/** @brief Selects the Cortex-M FreeMASTER platform implementation. */
#define FMSTR_PLATFORM_CORTEX_M 1
/** @brief Enables the long FreeMASTER interrupt handler. */
#define FMSTR_LONG_INTR 1
/** @brief Disables the short FreeMASTER interrupt handler. */
#define FMSTR_SHORT_INTR 0
/** @brief Selects interrupt-driven FreeMASTER servicing. */
#define FMSTR_POLL_DRIVEN 0
/** @brief Selects the serial FreeMASTER transport. */
#define FMSTR_TRANSPORT FMSTR_SERIAL
/** @brief Selects the MCUXpresso UART FreeMASTER serial driver. */
#define FMSTR_SERIAL_DRV FMSTR_SERIAL_MCUX_UART
/** @brief Selects UART0 as the FreeMASTER serial peripheral. */
#define FMSTR_SERIAL_BASE UART0
/** @brief Sets the FreeMASTER communication buffer size in bytes. */
#define FMSTR_COMM_BUFFER_SIZE 60U
/** @brief Enables FreeMASTER memory-read commands. */
#define FMSTR_USE_READMEM 1
/** @brief Enables FreeMASTER memory-write commands. */
#define FMSTR_USE_WRITEMEM 1
/** @brief Enables masked FreeMASTER memory-write commands. */
#define FMSTR_USE_WRITEMEMMASK 1

#endif
