#ifndef OPENTEC_BASE_USB_FANATEC_INPUT_H
#define OPENTEC_BASE_USB_FANATEC_INPUT_H

#include <stdbool.h>
#include <stdint.h>

#include "shifter/h_pattern.h"
#include "shifter/input.h"

enum {
    FANATEC_INPUT_REPORT_ID = 1,
    FANATEC_INPUT_REPORT_SIZE = 34,
    FANATEC_INPUT_BUTTON_BANKS = 5,
    FANATEC_INPUT_ROTARY_BYTES = 5,
    FANATEC_INPUT_ACCESSORY_BYTES = 5,
    FANATEC_INPUT_PEDAL_AXES = 3,
    FANATEC_INPUT_DIRECT_DRIVE_MODE = 0xfe
};

typedef struct {
    uint8_t button_banks[FANATEC_INPUT_BUTTON_BANKS];
    uint8_t rotary[FANATEC_INPUT_ROTARY_BYTES];
    uint8_t accessory[FANATEC_INPUT_ACCESSORY_BYTES];
    uint16_t steering;
    uint16_t pedals[FANATEC_INPUT_PEDAL_AXES];
    uint8_t clutch_paddles[2];
    uint8_t auxiliary_pedal;
    int8_t encoder_delta;
    uint8_t transfer_code;
    uint8_t status_flags;
    uint8_t wheel_mode;
    uint8_t axis_limit;
} fanatec_input_state;

bool fanatec_input_encode(uint8_t report[FANATEC_INPUT_REPORT_SIZE],
                          const fanatec_input_state *state);
void fanatec_input_apply_shifter(fanatec_input_state *state, const ShifterInputState *shifter,
                                 ShifterGear gear);

#endif
