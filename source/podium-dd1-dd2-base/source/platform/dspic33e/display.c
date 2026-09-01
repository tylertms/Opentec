#include "platform/display.h"

#include <stdint.h>
#include <xc.h>

#include "display/controller.h"
#include "platform/time.h"

/**
 * @brief Display DMA and reset timing values.
 */
enum {
    DISPLAY_DMA_REQUEST = 0x2d,       /**< DMA request number for parallel-master writes. */
    DISPLAY_INTERRUPT_PRIORITY = 1,   /**< Display DMA and parallel-master interrupt priority. */
    DISPLAY_RESET_START_DELAY_MS = 2, /**< Delay before asserting display reset low. */
    DISPLAY_RESET_LOW_MS = 1,         /**< Display reset-low pulse duration. */
    DISPLAY_RESET_RECOVERY_MS = 1,    /**< Delay after releasing display reset. */
    DISPLAY_FRAME_INTERVAL_MS = 33,
};

/**
 * @brief State used while writing display bus bytes.
 */
typedef struct {
    DisplayBusMode mode; /**< D/C mode most recently driven on the display bus. */
} DisplayBus;

/**
 * @brief Shared display bus write state.
 */
static DisplayBus display_bus;
static uint32_t next_frame_ms;

/**
 * @brief Waits until a millisecond interval is strictly past.
 *
 * Preserves the controller reset cadence by advancing only after the current time exceeds the
 * calculated deadline.
 *
 * @param[in] duration_ms Interval to add to the current system time.
 */
static void wait_past(uint32_t duration_ms) {
    uint32_t deadline_ms = platform_time_ms() + duration_ms;
    while (!platform_time_reached(platform_time_ms(), deadline_ms + 1)) {
    }
}

/**
 * @brief Writes one command or data byte to the display controller.
 *
 * Changes the D/C level when necessary, starts an eight-bit parallel-master transfer, and waits
 * for that transfer to complete.
 *
 * @param[in,out] context Display bus state used to avoid redundant D/C transitions.
 * @param[in] mode Command or data mode required for the byte.
 * @param[in] value Byte to send to the display controller.
 */
static void write_byte(void *context, DisplayBusMode mode, uint8_t value) {
    DisplayBus *bus = context;
    if (bus->mode != mode) {
        LATDbits.LATD3 = mode == DISPLAY_BUS_DATA;
        __builtin_nop();
        bus->mode = mode;
    }
    PMDIN1 = value;
    while (PMMODEbits.BUSY != 0) {
    }
}

/**
 * @brief Configures the eight-bit parallel-master display bus.
 *
 * Drives RE0 through RE7 as the data bus, selects CS2 through PMA15, configures the board control
 * pins, and enables byte-wide master mode with 2/15/2 wait states.
 *
 */
static void configure_parallel_port(void) {
    TRISE &= 0xff00u;
    TRISDbits.TRISD10 = 0;
    TRISDbits.TRISD4 = 0;
    TRISDbits.TRISD5 = 0;
    TRISGbits.TRISG13 = 0;
    TRISGbits.TRISG12 = 0;
    TRISGbits.TRISG0 = 1;
    LATGbits.LATG13 = 0;
    LATGbits.LATG12 = 1;

    PMCON = 0;
    PMCONbits.PTWREN = 1;
    PMCONbits.PTRDEN = 1;
    PMCONbits.CSF = 2;

    PMMODE = 0;
    PMMODEbits.IRQM = 1;
    PMMODEbits.MODE = 2;
    PMMODEbits.WAITB = 2;
    PMMODEbits.WAITM = 15;
    PMMODEbits.WAITE = 2;

    PMADDR = 0;
    PMADDRbits.CS2 = 1;
    PMAEN = 0;
    PMAENbits.PTEN15 = 1;
    PADCFG1bits.PMPTTL = 0;
    IFS2bits.PMPIF = 0;
    IPC11bits.PMPIP = DISPLAY_INTERRUPT_PRIORITY;
    PMCONbits.PMPEN = 1;
}

/**
 * @brief Applies the display controller reset sequence.
 *
 * Holds reset high through the setup interval, asserts it low for the pulse interval, then restores
 * it high through the recovery interval.
 *
 */
void platform_display_reset(void) {
    wait_past(DISPLAY_RESET_START_DELAY_MS);
    LATGbits.LATG14 = 0;
    wait_past(DISPLAY_RESET_LOW_MS);
    LATGbits.LATG14 = 1;
    wait_past(DISPLAY_RESET_RECOVERY_MS);
}

/**
 * @brief Starts a full framebuffer transfer on DMA channel 10.
 *
 * Configures byte-wide, one-shot RAM-to-peripheral transfers for the parallel-master request,
 * selects PMDIN1 as the destination, and forces the first transfer after enabling the channel.
 *
 * @param[in] framebuffer Complete packed display framebuffer in extended data space.
 */
static void start_dma(ConstDisplayFramebuffer framebuffer) {
    uint32_t source = (uint32_t)framebuffer;
    DMA10CONbits.CHEN = 0;
    IFS7bits.DMA10IF = 0;
    IEC7bits.DMA10IE = 0;
    DMA10CON = 0;
    DMA10CONbits.SIZE = 1;
    DMA10CONbits.DIR = 1;
    DMA10CONbits.AMODE = 0;
    DMA10CONbits.MODE = 1;
    DMA10REQbits.IRQSEL = DISPLAY_DMA_REQUEST;
    DMA10PAD = (uint16_t)&PMDIN1;
    DMA10CNT = DISPLAY_FRAMEBUFFER_SIZE - 1;
    DMA10STAL = (uint16_t)source;
    DMA10STAH = 0;
    IPC30bits.DMA10IP = DISPLAY_INTERRUPT_PRIORITY;
    IFS7bits.DMA10IF = 0;
    DMA10CONbits.CHEN = 1;
    DMA10REQbits.FORCE = 1;
}

/**
 * @brief Initializes the base display bus and controller.
 *
 * Configures the eight-bit parallel-master port, applies the high-low-high reset timing, sends the
 * controller setup stream, and leaves the bus in display-data mode.
 *
 */
void platform_display_init(void) {
    TRISGbits.TRISG14 = 0;
    TRISDbits.TRISD3 = 0;
    LATGbits.LATG14 = 1;
    LATDbits.LATD3 = 1;
    display_bus.mode = DISPLAY_BUS_DATA;
    next_frame_ms = 0;
    configure_parallel_port();
    platform_display_reset();
    display_controller_initialize(write_byte, &display_bus);
    LATDbits.LATD3 = 1;
    __builtin_nop();
    display_bus.mode = DISPLAY_BUS_DATA;
}

/**
 * @brief Starts a full base-display framebuffer transfer.
 *
 * Selects the 256-by-64 display window, enters display-RAM write mode, and starts an 8,192-byte
 * one-shot transfer from the framebuffer through DMA channel 10.
 *
 * @param[in] framebuffer Complete packed four-bit grayscale framebuffer in DMA-accessible storage.
 */
void platform_display_write_frame(ConstDisplayFramebuffer framebuffer) {
    uint32_t now_ms = platform_time_ms();
    if (!platform_time_reached(now_ms, next_frame_ms)) {
        return;
    }
    next_frame_ms = now_ms + DISPLAY_FRAME_INTERVAL_MS;
    display_controller_begin_frame(write_byte, &display_bus);
    LATDbits.LATD3 = 1;
    __builtin_nop();
    display_bus.mode = DISPLAY_BUS_DATA;
    start_dma(framebuffer);
}

void __attribute__((interrupt, no_auto_psv)) _PMPInterrupt(void) { IFS2bits.PMPIF = 0; }

void __attribute__((interrupt, no_auto_psv)) _DMA10Interrupt(void) { IFS7bits.DMA10IF = 0; }
