#include <assert.h>
#include <stdint.h>
#include <string.h>
#include <xc.h>

#include "platform/motor_link.h"

void _DMA9Interrupt(void);

static const uint8_t initial_frame[PLATFORM_MOTOR_LINK_FRAME_SIZE] = {0};

static const uint8_t first_received_frame[PLATFORM_MOTOR_LINK_FRAME_SIZE] = {
    0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d,
};

static const uint8_t second_received_frame[PLATFORM_MOTOR_LINK_FRAME_SIZE] = {
    0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d,
};

static void prepare_link(void) {
    platform_motor_link_init(initial_frame);
    DMA8CONbits.CHEN = 0;
    DMA9CONbits.CHEN = 0;
    IFS7bits.DMA9IF = 0;
}

static void test_dma9_completion_rearms_both_channels(void) {
    prepare_link();
    platform_motor_link_test_set_receive_dma(first_received_frame);
    _DMA9Interrupt();

    assert(DMA9CONbits.CHEN == 1);
    assert(DMA8CONbits.CHEN == 1);
    assert(IFS7bits.DMA9IF == 0);
}

static void test_recovery_preserves_error_interrupt_after_synchronization(void) {
    prepare_link();
    IFS0bits.SPI1EIF = 1;
    assert(IEC0bits.SPI1EIE == 0);

    platform_motor_link_confirm_synchronized();

    assert(IFS0bits.SPI1EIF == 0);
    assert(IEC0bits.SPI1EIE == 1);

    platform_motor_link_init(initial_frame);
    assert(IEC0bits.SPI1EIE == 1);
}

static void test_multiple_completions_are_queued(void) {
    uint8_t frame[PLATFORM_MOTOR_LINK_FRAME_SIZE] = {0};
    prepare_link();

    platform_motor_link_test_set_receive_dma(first_received_frame);
    _DMA9Interrupt();
    platform_motor_link_test_set_receive_dma(second_received_frame);
    _DMA9Interrupt();

    assert(platform_motor_link_take_received(frame));
    assert(memcmp(frame, first_received_frame, sizeof(frame)) == 0);
    assert(platform_motor_link_take_received(frame));
    assert(memcmp(frame, second_received_frame, sizeof(frame)) == 0);
    assert(!platform_motor_link_take_received(frame));
}

static void test_transmit_frame_waits_for_completion(void) {
    static const uint8_t next_frame[PLATFORM_MOTOR_LINK_FRAME_SIZE] = {
        0xa1, 0xa2, 0xa3, 0xa4, 0xa5, 0xa6, 0xa7, 0xa8, 0xa9, 0xaa, 0xab, 0xac, 0xad,
    };

    prepare_link();
    platform_motor_link_set_transmit(next_frame);
    assert(DMA8CONbits.CHEN == 0);

    platform_motor_link_test_set_receive_dma(first_received_frame);
    _DMA9Interrupt();
    assert(DMA8CONbits.CHEN == 1);
}

static void test_reinitialization_clears_pending_dma_completion(void) {
    uint8_t frame[PLATFORM_MOTOR_LINK_FRAME_SIZE] = {0};

    prepare_link();
    platform_motor_link_test_set_receive_dma(first_received_frame);
    DMA8CONbits.CHEN = 1;
    DMA9CONbits.CHEN = 1;
    IFS7bits.DMA9IF = 1;

    platform_motor_link_init(second_received_frame);

    assert(SPI1STATbits.SPIEN == 1);
    assert(DMA8CONbits.CHEN == 1);
    assert(DMA9CONbits.CHEN == 1);
    assert(IFS7bits.DMA8IF == 0);
    assert(IFS7bits.DMA9IF == 0);
    assert(!platform_motor_link_take_received(frame));

    platform_motor_link_test_set_receive_dma(second_received_frame);
    _DMA9Interrupt();
    assert(platform_motor_link_take_received(frame));
    assert(memcmp(frame, second_received_frame, sizeof(frame)) == 0);
}

int main(void) {
    test_recovery_preserves_error_interrupt_after_synchronization();
    test_dma9_completion_rearms_both_channels();
    test_multiple_completions_are_queued();
    test_transmit_frame_waits_for_completion();
    test_reinitialization_clears_pending_dma_completion();
    return 0;
}
