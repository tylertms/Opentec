#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <xc.h>

#include "platform/pedal_link.h"

void _U2RXInterrupt(void);
void _DMA1Interrupt(void);
void _DMA2Interrupt(void);

static const uint8_t valid_frame[PEDAL_FRAME_SIZE] = {
    PEDAL_FRAME_START, 1, 2, 3, 4, 5, 6, 7, 8, 9, 0x58, PEDAL_FRAME_END,
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

static void test_uart_error_policy(void) {
    uint8_t value = 0;
    uint8_t frame[PEDAL_FRAME_SIZE] = {0};
    bool error_interrupt_enabled = IEC4bits.U2EIE;
    IEC4bits.U2EIE = 0;

    platform_pedal_link_begin_discovery();
    platform_pedal_link_test_receive_byte(0x42, true);
    assert(platform_pedal_link_take_byte(&value));
    assert(value == 0x42);

    U2STAbits.OERR = 1;
    _U2RXInterrupt();
    assert(U2STAbits.OERR == 0);

    platform_pedal_link_begin_framed_receive();
    DMA1CONbits.CHEN = 0;
    platform_pedal_link_test_set_receive_dma(valid_frame, true);
    _DMA1Interrupt();
    assert(platform_pedal_link_take_frame(frame));
    assert(memcmp(frame, valid_frame, sizeof(frame)) == 0);
    platform_pedal_link_stop_receive();
    IFS4bits.U2EIF = 0;
    IEC4bits.U2EIE = error_interrupt_enabled;
}

static void test_recovery_uses_final_fifo_value(void) {
    uint8_t frame[PEDAL_FRAME_SIZE] = {0};
    uint8_t invalid_frame[PEDAL_FRAME_SIZE] = {0};
    const uint8_t early_delimiter[] = {PEDAL_FRAME_END, 0x01};
    const uint8_t final_delimiter[] = {0x01, PEDAL_FRAME_END};
    memcpy(invalid_frame, valid_frame, sizeof(invalid_frame));
    invalid_frame[0] = 0;

    platform_pedal_link_begin_framed_receive();
    DMA1CONbits.CHEN = 0;
    platform_pedal_link_test_set_receive_dma(invalid_frame, false);
    _DMA1Interrupt();
    assert(!platform_pedal_link_take_frame(frame));
    assert(DMA1CONbits.CHEN == 0);
    assert(IEC1bits.U2RXIE == 1);

    platform_pedal_link_test_receive_burst(early_delimiter, sizeof(early_delimiter));
    assert(DMA1CONbits.CHEN == 0);

    platform_pedal_link_test_receive_burst(final_delimiter, sizeof(final_delimiter));
    assert(DMA1CONbits.CHEN == 1);

    platform_pedal_link_test_set_receive_dma(valid_frame, false);
    _DMA1Interrupt();
    assert(platform_pedal_link_take_frame(frame));
    assert(memcmp(frame, valid_frame, sizeof(frame)) == 0);
}

static void test_dma_gates_checksum_and_type(void) {
    uint8_t value = 0;
    uint8_t frame[PEDAL_FRAME_SIZE] = {0};
    uint8_t invalid_checksum[PEDAL_FRAME_SIZE] = {0};
    uint8_t unsupported_type[PEDAL_FRAME_SIZE] = {
        PEDAL_FRAME_START, 2, 2, 3, 4, 5, 6, 7, 8, 9, 0xad, PEDAL_FRAME_END,
    };
    memcpy(invalid_checksum, valid_frame, sizeof(invalid_checksum));
    invalid_checksum[PEDAL_FRAME_SIZE - 2] ^= 1;

    platform_pedal_link_begin_transfer_receive();
    IFS0bits.DMA1IF = 1;
    platform_pedal_link_test_set_receive_dma(valid_frame, false);
    _DMA1Interrupt();
    assert(!platform_pedal_link_take_frame(frame));
    assert(IFS0bits.DMA1IF == 0);

    platform_pedal_link_begin_framed_receive();
    platform_pedal_link_test_receive_byte(0x42, false);
    assert(!platform_pedal_link_take_byte(&value));

    DMA1CONbits.CHEN = 0;
    platform_pedal_link_test_set_receive_dma(invalid_checksum, false);
    _DMA1Interrupt();
    assert(!platform_pedal_link_take_frame(frame));
    assert(DMA1CONbits.CHEN == 1);

    DMA1CONbits.CHEN = 0;
    platform_pedal_link_test_set_receive_dma(unsupported_type, false);
    _DMA1Interrupt();
    assert(!platform_pedal_link_take_frame(frame));
    assert(DMA1CONbits.CHEN == 1);
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

static void test_limits_transfer_frame_to_official_size(void) {
    uint8_t frame[TRANSFER_FRAME_MAX_SEND_PAYLOAD_SIZE] = {0};
    frame[0] = TRANSFER_FRAME_START;
    memset(frame + 1, 0x55, sizeof(frame) - 2);
    frame[sizeof(frame) - 1] = TRANSFER_FRAME_END;

    platform_pedal_link_begin_transfer_receive();
    feed_transfer(frame, sizeof(frame));
    uint8_t output[sizeof(frame)] = {0};
    assert(platform_pedal_link_take_transfer(output, sizeof(output)) == sizeof(frame));
    assert(memcmp(output, frame, sizeof(output)) == 0);

    uint8_t oversized[TRANSFER_FRAME_MAX_SEND_PAYLOAD_SIZE + 1] = {0};
    oversized[0] = TRANSFER_FRAME_START;
    memset(oversized + 1, 0x66, sizeof(oversized) - 2);
    oversized[sizeof(oversized) - 1] = TRANSFER_FRAME_END;
    platform_pedal_link_begin_transfer_receive();
    feed_transfer(oversized, sizeof(oversized));
    assert(platform_pedal_link_take_transfer(output, sizeof(output)) == 0);
}

static void test_appends_nested_start(void) {
    const uint8_t nested_frame[] = {
        TRANSFER_FRAME_START, 0x11, TRANSFER_FRAME_START, 0x22, TRANSFER_FRAME_END,
    };
    uint8_t output[sizeof(nested_frame)] = {0};
    platform_pedal_link_begin_transfer_receive();
    feed_transfer(nested_frame, sizeof(nested_frame));
    assert(platform_pedal_link_take_transfer(output, sizeof(output)) == sizeof(nested_frame));
    assert(memcmp(output, nested_frame, sizeof(output)) == 0);
}

static void test_resets_transfer_generation_around_transmit(void) {
    uint8_t output[sizeof(transfer_frame_a)] = {0};
    const uint8_t transmit[] = {TRANSFER_FRAME_START, 0x44, TRANSFER_FRAME_END};
    platform_pedal_link_begin_transfer_receive();
    feed_transfer(transfer_frame_a, sizeof(transfer_frame_a));
    assert(platform_pedal_link_send_transfer(transmit, sizeof(transmit)));
    assert(DMA2CNT == sizeof(transmit) - 1);
    assert(IEC1bits.U2RXIE == 1);
    feed_transfer(transfer_frame_c, sizeof(transfer_frame_c));
    IFS0bits.DMA1IF = 1;
    IFS1bits.DMA2IF = 1;
    U2STAbits.OERR = 1;
    _DMA2Interrupt();

    assert(!platform_pedal_link_transmit_busy());
    assert(IFS0bits.DMA1IF == 0);
    assert(IFS1bits.DMA2IF == 0);
    assert(U2STAbits.OERR == 0);
    assert(DMA1CNT == TRANSFER_FRAME_MAX_ENCODED_SIZE - 1);
    assert(IEC1bits.U2RXIE == 1);
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
    test_uart_error_policy();
    test_recovery_uses_final_fifo_value();
    test_dma_gates_checksum_and_type();
    test_retains_transfer_burst();
    test_limits_transfer_frame_to_official_size();
    test_appends_nested_start();
    test_resets_transfer_generation_around_transmit();
    test_stop_receive_clears_generation_and_rearms();
    test_transmit_completion_preserves_stop_state();
    return 0;
}
