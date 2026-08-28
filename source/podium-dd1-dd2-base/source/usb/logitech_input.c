#include "usb/logitech_input.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/**
 * @brief Encodes a Driving Force EX input report.
 *
 * Packs a ten-bit steering value, twelve buttons, a directional hat, and three eight-bit axes
 * into the seven-byte compatibility report.
 *
 * @param[out] report Buffer that receives the encoded report.
 * @param[in] state Logical compatibility input values.
 * @return True when the report was encoded; otherwise false.
 */
bool logitech_driving_force_ex_encode(uint8_t report[LOGITECH_DRIVING_FORCE_EX_REPORT_SIZE],
                                      const LogitechInputState *state) {
    if (report == NULL || state == NULL) {
        return false;
    }

    uint16_t steering = state->steering >> 6;
    report[0] = (uint8_t)steering;
    report[1] = (uint8_t)(((steering >> 8) & 0x03u) | ((state->buttons & 0x3fu) << 2));
    report[2] = (uint8_t)((state->buttons >> 6) & 0x3fu);
    report[3] = state->axes[0];
    report[4] = state->hat & 0x0fu;
    report[5] = state->axes[1];
    report[6] = state->axes[2];
    return true;
}

/**
 * @brief Encodes a Driving Force Pro input report.
 *
 * Packs a fourteen-bit steering value, fourteen buttons, a directional hat, and three eight-bit
 * axes into the eight-byte compatibility report.
 *
 * @param[out] report Buffer that receives the encoded report.
 * @param[in] state Logical compatibility input values.
 * @return True when the report was encoded; otherwise false.
 */
bool logitech_driving_force_pro_encode(uint8_t report[LOGITECH_DRIVING_FORCE_PRO_REPORT_SIZE],
                                       const LogitechInputState *state) {
    if (report == NULL || state == NULL) {
        return false;
    }

    uint16_t steering = state->steering >> 2;
    report[0] = (uint8_t)steering;
    report[1] = (uint8_t)(((steering >> 8) & 0x3fu) | ((state->buttons & 0x03u) << 6));
    report[2] = 0;
    report[3] = (uint8_t)(state->buttons >> 2);
    report[4] = (uint8_t)(((state->buttons >> 10) & 0x0fu) | ((state->hat & 0x0fu) << 4));
    report[5] = state->axes[0];
    report[6] = state->axes[1];
    report[7] = state->axes[2];
    return true;
}

/**
 * @brief Encodes a G27 input report.
 *
 * Packs a fourteen-bit steering value, twenty-three buttons, a directional hat, and three
 * eight-bit axes into the eleven-byte compatibility report.
 *
 * @param[out] report Buffer that receives the encoded report.
 * @param[in] state Logical compatibility input values.
 * @return True when the report was encoded; otherwise false.
 */
bool logitech_g27_encode(uint8_t report[LOGITECH_G27_REPORT_SIZE],
                         const LogitechInputState *state) {
    if (report == NULL || state == NULL) {
        return false;
    }

    uint16_t steering = state->steering >> 2;
    report[0] = (uint8_t)((state->hat & 0x0fu) | ((state->buttons & 0x0fu) << 4));
    report[1] = (uint8_t)(state->buttons >> 4);
    report[2] = (uint8_t)(state->buttons >> 12);
    report[3] = (uint8_t)(((state->buttons >> 20) & 0x03u) | ((steering & 0x3fu) << 2));
    report[4] = (uint8_t)(steering >> 6);
    report[5] = state->axes[0];
    report[6] = state->axes[1];
    report[7] = state->axes[2];
    report[8] = 0x80;
    report[9] = 0x80;
    report[10] = (uint8_t)(((state->buttons >> 22) & 1u) | 0x02u);
    return true;
}
