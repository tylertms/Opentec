#ifndef OPENTEC_BASE_USB_FANATEC_ENCODER_H
#define OPENTEC_BASE_USB_FANATEC_ENCODER_H

#include <stdbool.h>
#include <stdint.h>

#include "usb/fanatec_input.h"

typedef struct {
    uint32_t deadline_ms;
    uint8_t position;
    bool quiet_phase;
} FanatecEncoder;

void fanatec_encoder_init(FanatecEncoder *encoder);
bool fanatec_encoder_update(FanatecEncoder *encoder, int8_t pending_direction, uint32_t now_ms,
                            fanatec_input_state *input);

#endif
