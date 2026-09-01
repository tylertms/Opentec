#ifndef OPENTEC_BASE_USB_LOGITECH_INPUT_H
#define OPENTEC_BASE_USB_LOGITECH_INPUT_H

#include <stdbool.h>
#include <stdint.h>

/** @brief Encoded report sizes for the supported Logitech layouts. */
enum {
    LOGITECH_DRIVING_FORCE_EX_REPORT_SIZE = 7,  /**< Driving Force EX report size in bytes. */
    LOGITECH_DRIVING_FORCE_PRO_REPORT_SIZE = 8, /**< Driving Force Pro report size in bytes. */
    LOGITECH_G27_REPORT_SIZE = 11,              /**< G27 report size in bytes. */
};

/** @brief Logical controls encoded into a Logitech-compatible input report. */
typedef struct {
    uint16_t steering; /**< Sixteen-bit source steering value reduced by each report layout. */
    uint32_t buttons;  /**< Button bit field shared by the supported layouts. */
    uint8_t hat;       /**< Directional-hat value; only the low four bits are encoded. */
    uint8_t axes[3];   /**< Three source eight-bit axis values. */
} LogitechInputState;

/**
 * @brief Encodes a Logitech Driving Force EX input report.
 *
 * Packs the ten-bit steering value, twelve button bits, directional hat, and three auxiliary axes
 * into the seven-byte report layout.
 *
 * @param[out] report Buffer with room for LOGITECH_DRIVING_FORCE_EX_REPORT_SIZE bytes.
 * @param[in] state Logical Logitech input values.
 * @return True when both pointers are valid and the report is encoded; otherwise false.
 */
bool logitech_driving_force_ex_encode(uint8_t report[LOGITECH_DRIVING_FORCE_EX_REPORT_SIZE],
                                      const LogitechInputState *state);

/**
 * @brief Encodes a Logitech Driving Force Pro input report.
 *
 * Packs the fourteen-bit steering value, fourteen button bits, directional hat, and three auxiliary
 * axes into the eight-byte report layout.
 *
 * @param[out] report Buffer with room for LOGITECH_DRIVING_FORCE_PRO_REPORT_SIZE bytes.
 * @param[in] state Logical Logitech input values.
 * @return True when both pointers are valid and the report is encoded; otherwise false.
 */
bool logitech_driving_force_pro_encode(uint8_t report[LOGITECH_DRIVING_FORCE_PRO_REPORT_SIZE],
                                       const LogitechInputState *state);

/**
 * @brief Encodes a Logitech G27 input report.
 *
 * Packs the fourteen-bit steering value, twenty-three button bits, directional hat, and three
 * auxiliary axes into the eleven-byte report layout.
 *
 * @param[out] report Buffer with room for LOGITECH_G27_REPORT_SIZE bytes.
 * @param[in] state Logical Logitech input values.
 * @return True when both pointers are valid and the report is encoded; otherwise false.
 */
bool logitech_g27_encode(uint8_t report[LOGITECH_G27_REPORT_SIZE], const LogitechInputState *state);

#endif
