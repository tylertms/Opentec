#include <assert.h>
#include <stdint.h>
#include <string.h>
#include <xc.h>

#include "platform/serial_link.h"

void _DMA5Interrupt(void);
void _DMA6Interrupt(void);
void _T6Interrupt(void);
void _U3RXInterrupt(void);
void _U3ErrInterrupt(void);

static void disable_automatic_interrupts(void) {
    IEC0 = 0;
    IEC1 = 0;
    IEC2 = 0;
    IEC3 = 0;
    IEC4 = 0;
    IEC5 = 0;
    IEC6 = 0;
    IEC7 = 0;
}

static void test_initialization_leaves_receive_dma_disarmed(void) {
    platform_serial_link_init();

    assert(U3MODEbits.BRGH == 1);
    assert(U3MODEbits.URXINV == 1);
    assert(U3BRG == 2);
    assert(U3STAbits.UTXINV == 1);
    assert(U3MODEbits.UARTEN == 1);
    assert(U3STAbits.UTXEN == 1);
    assert(DMA5CONbits.CHEN == 0);
    assert(DMA6CONbits.CHEN == 0);
    assert(IEC3bits.DMA5IE == 1);
    assert(IEC4bits.DMA6IE == 0);
    assert(IEC5bits.U3EIE == 1);
    assert(IEC5bits.U3TXIE == 0);
    assert(IEC5bits.U3RXIE == 0);

    platform_serial_link_reset();
}

static void test_reset_preserves_dma_descriptors(void) {
    uint8_t packet[SERIAL_PACKET_SIZE] = {0};
    platform_serial_link_init();

    uint16_t transmit_pad = DMA5PAD;
    uint16_t transmit_start = DMA5STAL;
    uint16_t receive_pad = DMA6PAD;
    uint16_t receive_start = DMA6STAL;

    platform_serial_link_reset();
    assert(DMA5CNT == 71);
    assert(DMA6CNT == 67);
    U3MODEbits.UARTEN = 0;
    U3STAbits.UTXEN = 0;
    assert(platform_serial_link_start(packet));

    assert(DMA5CONbits.SIZE == 1);
    assert(DMA5CONbits.DIR == 1);
    assert(DMA5CONbits.AMODE == 0);
    assert(DMA5CONbits.MODE == 1);
    assert(DMA5REQbits.IRQSEL == 0x53);
    assert(DMA5PAD == transmit_pad);
    assert(DMA5STAL == transmit_start);
    assert(DMA5STAH == 0);
    assert(DMA5CNT == 71);

    assert(DMA6CONbits.SIZE == 1);
    assert(DMA6CONbits.DIR == 0);
    assert(DMA6CONbits.AMODE == 0);
    assert(DMA6CONbits.MODE == 1);
    assert(DMA6REQbits.IRQSEL == 0x52);
    assert(DMA6PAD == receive_pad);
    assert(DMA6STAL == receive_start);
    assert(DMA6STAH == 0);
    assert(DMA6CNT == 67);

    platform_serial_link_reset();
}

static void test_dma_completion_keeps_non_timeout_timer_active(void) {
    uint8_t packet[SERIAL_PACKET_SIZE] = {0};
    uint8_t receive_dma[PLATFORM_SERIAL_LINK_RECEIVE_DMA_SIZE] = {0};
    platform_serial_link_init();
    U3MODEbits.UARTEN = 0;
    U3STAbits.UTXEN = 0;
    assert(platform_serial_link_start(packet));
    disable_automatic_interrupts();
    _DMA5Interrupt();
    assert(T6CONbits.TON == 1);
    PR6 = UINT16_MAX;
    receive_dma[0] = SERIAL_PACKET_START;
    platform_serial_link_test_set_receive_dma(receive_dma);
    IFS4bits.DMA6IF = 1;
    _DMA6Interrupt();

    assert(T6CONbits.TON == 1);
    assert(platform_serial_link_take_received(packet));
    assert(packet[0] == SERIAL_PACKET_START);
    assert(IFS4bits.DMA6IF == 0);
    assert(platform_serial_link_start(packet));
    platform_serial_link_reset();
}

static void test_unaligned_dma_completion_publishes_raw_result(void) {
    uint8_t packet[SERIAL_PACKET_SIZE] = {0};
    uint8_t receive_dma[PLATFORM_SERIAL_LINK_RECEIVE_DMA_SIZE];
    for (uint8_t index = 0; index < sizeof(receive_dma); index++) {
        receive_dma[index] = (uint8_t)(index + 1);
    }
    platform_serial_link_init();
    U3MODEbits.UARTEN = 0;
    U3STAbits.UTXEN = 0;
    platform_serial_link_test_set_receive_dma(receive_dma);
    disable_automatic_interrupts();
    IFS4bits.DMA6IF = 1;
    _DMA6Interrupt();

    assert(IFS4bits.DMA6IF == 0);
    assert(platform_serial_link_take_received(packet));
    assert(memcmp(packet, receive_dma, sizeof(packet)) == 0);
    assert(platform_serial_link_start_periodic_recovery());
    assert(PR6 == 0x27d8);
    assert(T6CONbits.TON == 1);
    assert(!platform_serial_link_start(packet));
    _T6Interrupt();
    assert(T6CONbits.TON == 0);
    assert(platform_serial_link_start(packet));
    platform_serial_link_reset();
}

static void test_receive_timeout_uses_receive_state_and_recovers_dma(void) {
    uint8_t packet[SERIAL_PACKET_SIZE] = {0};
    uint8_t receive_dma[PLATFORM_SERIAL_LINK_RECEIVE_DMA_SIZE] = {0};
    receive_dma[0] = SERIAL_PACKET_START;
    receive_dma[1] = 0x22;
    receive_dma[2] = 0x33;
    receive_dma[63] = 0x44;
    platform_serial_link_init();
    U3MODEbits.UARTEN = 0;
    U3STAbits.UTXEN = 0;
    assert(platform_serial_link_start(packet));
    disable_automatic_interrupts();
    _U3RXInterrupt();
    assert(T6CONbits.TON == 0);

    _DMA5Interrupt();
    _T6Interrupt();
    assert(DMA6CONbits.CHEN == 1);
    _U3RXInterrupt();
    assert(T6CONbits.TON == 1);
    assert(PR6 == 10000);

    DMA6CNT = 0;
    IFS2bits.T6IF = 1;
    platform_serial_link_test_set_receive_dma(receive_dma);
    _T6Interrupt();

    assert(T6CONbits.TON == 0);
    assert(IFS2bits.T6IF == 0);
    assert(DMA6CONbits.CHEN == 0);
    assert(IEC4bits.DMA6IE == 0);
    assert(DMA6CNT == PLATFORM_SERIAL_LINK_RECEIVE_DMA_SIZE - 1);
    assert(platform_serial_link_take_received(packet));
    assert(memcmp(packet, receive_dma, sizeof(packet)) == 0);
    assert(!platform_serial_link_take_received(packet));
    assert(platform_serial_link_start_periodic_recovery());
    assert(PR6 == 0x27d8);
    assert(TMR6 < PR6);
    assert(T6CONbits.TON == 1);
    assert(!platform_serial_link_start(packet));
    _T6Interrupt();
    assert(platform_serial_link_start(packet));
    platform_serial_link_reset();
}

static void test_poll_before_dma_completion_preserves_receive_interrupt(void) {
    uint8_t packet[SERIAL_PACKET_SIZE] = {0};
    platform_serial_link_init();
    U3MODEbits.UARTEN = 0;
    U3STAbits.UTXEN = 0;
    assert(platform_serial_link_start(packet));
    disable_automatic_interrupts();
    _DMA5Interrupt();
    _T6Interrupt();

    assert(DMA6CONbits.CHEN == 1);
    assert(IEC4bits.DMA6IE == 1);
    assert(!platform_serial_link_take_received(packet));
    assert(IEC4bits.DMA6IE == 1);
    platform_serial_link_reset();
}

static void test_uart_error_clears_interrupt_before_uart_state(void) {
    platform_serial_link_init();
    U3STAbits.OERR = 1;
    U3STAbits.FERR = 1;
    U3STAbits.PERR = 1;
    IFS5bits.U3EIF = 1;
    _U3ErrInterrupt();

    assert(IFS5bits.U3EIF == 0);
    assert(U3STAbits.OERR == 0);
    assert(U3STAbits.FERR == 0);
    assert(U3STAbits.PERR == 0);
    platform_serial_link_reset();
}

static void test_periodic_recovery_blocks_receive_timeout(void) {
    uint8_t packet[SERIAL_PACKET_SIZE] = {0};
    platform_serial_link_init();
    disable_automatic_interrupts();
    assert(platform_serial_link_start_periodic_recovery());
    assert(PR6 == 0x27d8);
    assert(TMR6 < PR6);
    assert(T6CONbits.TON == 1);
    assert(IFS2bits.T6IF == 0);
    assert(!platform_serial_link_start(packet));
    _U3RXInterrupt();
    assert(PR6 == 0x27d8);
    assert(TMR6 < PR6);
    assert(T6CONbits.TON == 1);
    _T6Interrupt();
    assert(T6CONbits.TON == 0);
    assert(IFS2bits.T6IF == 0);
    U3MODEbits.UARTEN = 0;
    U3STAbits.UTXEN = 0;
    assert(platform_serial_link_start(packet));
    _DMA5Interrupt();
    _T6Interrupt();
    assert(DMA6CONbits.CHEN == 1);
    assert(IEC4bits.DMA6IE == 1);
    assert(IEC5bits.U3RXIE == 1);
    assert(DMA6CNT == PLATFORM_SERIAL_LINK_RECEIVE_DMA_SIZE - 1);
    platform_serial_link_reset();
}

static void test_direct_turnaround_clears_overrun_before_idle_wait(void) {
    platform_serial_link_init();
    platform_serial_link_enter_direct_mode();
    disable_automatic_interrupts();
    U3STAbits.OERR = 1;
    U3STAbits.RIDLE = 1;
    IFS2bits.T6IF = 1;
    _T6Interrupt();

    assert(U3STAbits.OERR == 0);
    assert(T6CONbits.TON == 1);
    assert(IFS2bits.T6IF == 0);
    assert(IEC5bits.U3RXIE == 1);
}

int main(void) {
    test_initialization_leaves_receive_dma_disarmed();
    test_reset_preserves_dma_descriptors();
    test_dma_completion_keeps_non_timeout_timer_active();
    test_unaligned_dma_completion_publishes_raw_result();
    test_receive_timeout_uses_receive_state_and_recovers_dma();
    test_poll_before_dma_completion_preserves_receive_interrupt();
    test_uart_error_clears_interrupt_before_uart_state();
    test_periodic_recovery_blocks_receive_timeout();
    test_direct_turnaround_clears_overrun_before_idle_wait();
    return 0;
}
