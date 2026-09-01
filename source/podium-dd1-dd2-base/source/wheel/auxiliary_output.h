#ifndef OPENTEC_BASE_WHEEL_AUXILIARY_OUTPUT_H
#define OPENTEC_BASE_WHEEL_AUXILIARY_OUTPUT_H

#include <stdbool.h>
#include <stdint.h>

/**
 * @brief Operating-mode opcodes that control attached-wheel auxiliary output.
 *
 * The opcodes select the persisted option, code-mode flag, or two-byte auxiliary report.
 */
enum {
    WHEEL_AUXILIARY_OPTION_OPCODE = 0x06,    /**< Selects the auxiliary-output option. */
    WHEEL_AUXILIARY_CODE_MODE_OPCODE = 0x07, /**< Selects cumulative code encoding. */
    WHEEL_AUXILIARY_REPORT_OPCODE = 0x08,    /**< Supplies the two-byte auxiliary report. */
};

/**
 * @brief Shared auxiliary report and attached-wheel scan encoding policy.
 *
 * The service updates this state from host commands and uses it to encode the next scan output.
 */
typedef struct {
    uint16_t report;       /**< Nine-bit cumulative auxiliary report value. */
    uint8_t latched_bands; /**< Retained low, middle, and high band latch flags. */
    uint8_t option;        /**< Raw host option; option one disables encoded output. */
    bool code_mode;        /**< True to map cumulative patterns to compact scan codes. */
    bool exclusive_mode;   /**< True to select only the highest-priority active band. */
} WheelAuxiliaryOutput;

/**
 * @brief Encodes the attached-wheel auxiliary scan output.
 *
 * Masks the report to its nine supported bits, then applies exclusive, cumulative-code, or
 * combined-band encoding. Option one and a null state produce no output.
 *
 * @param[in] output Auxiliary report and encoding policy.
 * @return Encoded auxiliary scan byte, or zero when output is null or disabled.
 */
uint8_t wheel_auxiliary_output_encode(const WheelAuxiliaryOutput *output);

#endif
