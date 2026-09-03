#include "usb/fanatec_encoder.h"

#include <assert.h>
#include <stdbool.h>
#include <stdint.h>

static bool update(FanatecEncoder *encoder, int8_t direction, uint32_t now_ms,
                   fanatec_input_state *input) {
    *input = (fanatec_input_state){0};
    return fanatec_encoder_update(encoder, direction, now_ms, input);
}

int main(void) {
    FanatecEncoder encoder;
    fanatec_input_state input;
    fanatec_encoder_init(&encoder);

    assert(!update(&encoder, 0, 100, &input));
    assert(input.encoder_position == 0);
    assert(input.button_banks[3] == 0);

    assert(!update(&encoder, 1, 101, &input));
    assert(input.encoder_position == 0);
    assert(input.button_banks[3] == (1 << 3));
    assert(!update(&encoder, 1, 116, &input));
    assert(!update(&encoder, 1, 117, &input));
    assert(update(&encoder, 1, 118, &input));
    assert(input.encoder_position == 1);
    assert(input.button_banks[3] == (1 << 3));

    assert(!update(&encoder, 1, 119, &input));
    assert(input.button_banks[3] == 0);
    assert(!update(&encoder, 1, 134, &input));
    assert(!update(&encoder, 1, 135, &input));
    assert(input.button_banks[3] == 0);
    assert(!update(&encoder, 1, 136, &input));
    assert(!update(&encoder, 1, 152, &input));
    assert(input.button_banks[3] == (1 << 3));
    assert(!update(&encoder, 1, 153, &input));
    assert(update(&encoder, 1, 154, &input));
    assert(input.encoder_position == 2);

    assert(!update(&encoder, -1, 171, &input));
    assert(!update(&encoder, -1, 172, &input));
    assert(input.button_banks[3] == 0);
    assert(!update(&encoder, -1, 188, &input));
    assert(input.button_banks[3] == (1 << 2));
    assert(!update(&encoder, -1, 189, &input));
    assert(update(&encoder, -1, 190, &input));
    assert(input.encoder_position == 1);

    encoder.position = 0x7f;
    encoder.quiet_phase = false;
    encoder.deadline_ms = 0;
    assert(update(&encoder, 1, 1, &input));
    assert((uint8_t)input.encoder_position == 0x80);

    return 0;
}
