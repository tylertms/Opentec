#include "platform/display.h"

#include <stdint.h>
#include <xc.h>

#include "display/controller.h"
#include "platform/time.h"

enum {
    DISPLAY_DMA_REQUEST = 0x2d,
    DISPLAY_INTERRUPT_PRIORITY = 1,
    DISPLAY_RESET_START_DELAY_MS = 2,
    DISPLAY_RESET_LOW_MS = 1,
    DISPLAY_RESET_RECOVERY_MS = 1,
};

typedef struct {
    DisplayBusMode mode;
} DisplayBus;

static DisplayBus display_bus;

static void wait_ms(uint32_t duration_ms) {
    uint32_t deadline_ms = platform_time_ms() + duration_ms;
    while (!platform_time_reached(platform_time_ms(), deadline_ms)) {
    }
}

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

static void reset_controller(void) {
    wait_ms(DISPLAY_RESET_START_DELAY_MS);
    LATGbits.LATG14 = 0;
    wait_ms(DISPLAY_RESET_LOW_MS);
    LATGbits.LATG14 = 1;
    wait_ms(DISPLAY_RESET_RECOVERY_MS);
}

static void start_dma(const uint8_t *framebuffer) {
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
    DMA10STAL = (uint16_t)framebuffer;
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
 */
void platform_display_init(void) {
    TRISGbits.TRISG14 = 0;
    TRISDbits.TRISD3 = 0;
    LATGbits.LATG14 = 1;
    LATDbits.LATD3 = 1;
    display_bus.mode = DISPLAY_BUS_DATA;
    configure_parallel_port();
    reset_controller();
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
 * @param[in] framebuffer Complete packed four-bit grayscale framebuffer in DMA-accessible memory.
 */
void platform_display_write_frame(const uint8_t framebuffer[DISPLAY_FRAMEBUFFER_SIZE]) {
    display_controller_begin_frame(write_byte, &display_bus);
    LATDbits.LATD3 = 1;
    __builtin_nop();
    display_bus.mode = DISPLAY_BUS_DATA;
    start_dma(framebuffer);
}
