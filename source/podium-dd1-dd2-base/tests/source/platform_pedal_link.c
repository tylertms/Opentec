#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <xc.h>

#include "platform/pedal_link.h"

void _DMA1Interrupt(void);
void _DMA2Interrupt(void);

static const uint8_t valid_frame[PEDAL_FRAME_SIZE] = {
    PEDAL_FRAME_START, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, PEDAL_FRAME_END,
};

static const uint8_t transfer_frame_a[] = {TRANSFER_FRAME_START, 0x11, TRANSFER_FRAME_END};
static const uint8_t transfer_frame_b[] = {TRANSFER_FRAME_START, 0x22, TRANSFER_FRAME_END};
static const uint8_t transfer_frame_c[] = {TRANSFER_FRAME_START, 0x33, TRANSFER_FRAME_END};

static void feed_transfer(const uint8_t *frame, uint8_t length) {
    for (uint8_t index = 0; index < length; index++) {
        platform_pedal_link_test_receive_byte(frame[index], false);
    }
}

static void test_valid_frame_is_published(void) {
    uint8_t frame[PEDAL_FRAME_SIZE] = {0};
    platform_pedal_link_begin_framed_receive();
    DMA1CONbits.CHEN = 0;
    platform_pedal_link_test_set_receive_dma(valid_frame, false);
    _DMA1Interrupt();
    assert(platform_pedal_link_take_frame(frame));
    assert(memcmp(frame, valid_frame, sizeof(frame)) == 0);
}

static void test_uart_error_enters_delimiter_recovery(void) {
    uint8_t frame[PEDAL_FRAME_SIZE] = {0};
    platform_pedal_link_begin_framed_receive();
    DMA1CONbits.CHEN = 0;
    platform_pedal_link_test_set_receive_dma(valid_frame, false);
    platform_pedal_link_test_receive_byte(PEDAL_FRAME_END, true);
    _DMA1Interrupt();
    assert(!platform_pedal_link_take_frame(frame));
    assert(DMA1CONbits.CHEN == 0);
    assert(IEC1bits.U2RXIE == 1);

    platform_pedal_link_test_receive_byte(PEDAL_FRAME_END, false);
    assert(DMA1CONbits.CHEN == 1);

    platform_pedal_link_test_set_receive_dma(valid_frame, false);
    _DMA1Interrupt();
    assert(platform_pedal_link_take_frame(frame));
    assert(memcmp(frame, valid_frame, sizeof(frame)) == 0);
}

static void test_retains_transfer_burst(void) {
    uint8_t output[sizeof(transfer_frame_a)] = {0};
    platform_pedal_link_begin_transfer_receive();
    feed_transfer(transfer_frame_a, sizeof(transfer_frame_a));
    feed_transfer(transfer_frame_b, sizeof(transfer_frame_b));
    feed_transfer(transfer_frame_c, sizeof(transfer_frame_c));

    assert(platform_pedal_link_take_transfer(output, sizeof(output)) == sizeof(transfer_frame_a));
    assert(memcmp(output, transfer_frame_a, sizeof(output)) == 0);
    assert(platform_pedal_link_take_transfer(output, sizeof(output)) == sizeof(transfer_frame_b));
    assert(memcmp(output, transfer_frame_b, sizeof(output)) == 0);
    assert(platform_pedal_link_take_transfer(output, sizeof(output)) == sizeof(transfer_frame_c));
    assert(memcmp(output, transfer_frame_c, sizeof(output)) == 0);
}

static void test_resets_transfer_generation_before_transmit(void) {
    uint8_t output[sizeof(transfer_frame_a)] = {0};
    const uint8_t transmit[] = {TRANSFER_FRAME_START, 0x44, TRANSFER_FRAME_END};
    platform_pedal_link_begin_transfer_receive();
    feed_transfer(transfer_frame_a, sizeof(transfer_frame_a));
    assert(platform_pedal_link_send_transfer(transmit, sizeof(transmit)));
    assert(DMA2CNT == sizeof(transmit) - 1);
    assert(IEC1bits.U2RXIE == 1);
    _DMA2Interrupt();

    assert(platform_pedal_link_take_transfer(output, sizeof(output)) == 0);
    feed_transfer(transfer_frame_b, sizeof(transfer_frame_b));
    assert(platform_pedal_link_take_transfer(NULL, sizeof(output)) == 0);
    assert(platform_pedal_link_take_transfer(output, sizeof(output)) == sizeof(transfer_frame_b));
    assert(memcmp(output, transfer_frame_b, sizeof(output)) == 0);
}

static void test_stop_receive_clears_generation_and_rearms(void) {
    uint8_t output[sizeof(transfer_frame_a)] = {0};
    platform_pedal_link_begin_transfer_receive();
    feed_transfer(transfer_frame_a, sizeof(transfer_frame_a));
    platform_pedal_link_stop_receive();

    assert(U2MODEbits.UARTEN == 0);
    assert(DMA1CONbits.CHEN == 0);
    assert(IEC0bits.DMA1IE == 0);
    assert(IEC1bits.U2RXIE == 0);
    assert(platform_pedal_link_take_transfer(output, sizeof(output)) == 0);

    platform_pedal_link_begin_transfer_receive();
    feed_transfer(transfer_frame_c, sizeof(transfer_frame_c));
    assert(platform_pedal_link_take_transfer(output, sizeof(output)) == sizeof(transfer_frame_c));
    assert(memcmp(output, transfer_frame_c, sizeof(output)) == 0);
}

static void test_transmit_completion_preserves_stop_state(void) {
    const uint8_t transmit[] = {TRANSFER_FRAME_START, 0x55, TRANSFER_FRAME_END};
    platform_pedal_link_begin_transfer_receive();
    assert(platform_pedal_link_send_transfer(transmit, sizeof(transmit)));
    platform_pedal_link_stop_receive();
    _DMA2Interrupt();
    assert(U2MODEbits.UARTEN == 0);
    assert(IEC1bits.U2RXIE == 0);
}

int main(void) {
    platform_pedal_link_init();
    test_valid_frame_is_published();
    test_uart_error_enters_delimiter_recovery();
    test_retains_transfer_burst();
    test_resets_transfer_generation_before_transmit();
    test_stop_receive_clears_generation_and_rearms();
    test_transmit_completion_preserves_stop_state();
    return 0;
}
