#ifndef OPENTEC_BASE_WHEEL_AUXILIARY_OUTPUT_H
#define OPENTEC_BASE_WHEEL_AUXILIARY_OUTPUT_H

#include <stdbool.h>
#include <stdint.h>

/** @brief Operating-mode opcodes that control attached-wheel auxiliary output. */
enum {
    WHEEL_AUXILIARY_OPTION_OPCODE = 0x06,
    WHEEL_AUXILIARY_CODE_MODE_OPCODE = 0x07,
    WHEEL_AUXILIARY_REPORT_OPCODE = 0x08,
};

/** @brief Shared auxiliary report and attached-wheel scan encoding policy. */
typedef struct {
    uint16_t report;
    uint8_t latched_bands;
    bool disabled;
    bool code_mode;
    bool exclusive_mode;
} WheelAuxiliaryOutput;

uint8_t wheel_auxiliary_output_encode(const WheelAuxiliaryOutput *output);

#endif
