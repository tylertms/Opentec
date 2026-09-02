#include <assert.h>
#include <stdint.h>
#include <xc.h>

#include "platform/serial_link.h"

static void test_restart_restores_dma_descriptors(void) {
    uint8_t packet[SERIAL_PACKET_SIZE] = {0};
    platform_serial_link_init();

    uint16_t transmit_pad = DMA5PAD;
    uint16_t transmit_start = DMA5STAL;
    uint16_t receive_pad = DMA6PAD;
    uint16_t receive_start = DMA6STAL;

    platform_serial_link_reset();
    assert(DMA5CNT == 0);
    assert(DMA6CNT == 0);
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

int main(void) {
    test_restart_restores_dma_descriptors();
    return 0;
}
