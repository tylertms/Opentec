#include "usb/logitech_input.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

static const uint8_t hat_map[16] = {8, 2, 6, 8, 4, 3, 5, 0, 0, 1, 7, 0, 8, 0, 2, 5};

static uint8_t map_hat(uint8_t buttons) {
    uint8_t index = (uint8_t)(((buttons & 0x01u) << 3) | ((buttons >> 1) & 0x04u) |
                              (buttons & 0x02u) | ((buttons >> 2) & 0x01u));
    return hat_map[index];
}

static uint8_t combined_axis(const LogitechInputSource *source) {
    return (uint8_t)(((uint16_t)~source->pedals[1] >> 9) +
                     ((uint8_t)(source->pedals[0] >> 8) >> 1));
}

static uint8_t primary_buttons(uint8_t buttons) {
    return (uint8_t)((buttons & 0x01u) | ((buttons & 0x08u) >> 2) | ((buttons & 0x02u) << 1) |
                     ((buttons & 0x10u) >> 1) | ((buttons & 0x40u) >> 2) |
                     ((buttons & 0x80u) >> 2) | ((buttons & 0x04u) << 4) |
                     ((buttons & 0x20u) << 2));
}

static uint8_t driving_force_pro_primary_buttons(uint8_t first, uint8_t second) {
    return (uint8_t)(((first & 0xc0u) >> 6) | ((second & 0x01u) << 2) | (second & 0x08u) |
                     ((second & 0x02u) << 3) | ((second & 0x10u) << 1) | (second & 0xc0u));
}

/**
 * @brief Maps native controls to the selected Logitech logical layout.
 *
 * @param[out] state Logitech state receiving mapped values.
 * @param[in] model Compatibility layout to apply.
 * @param[in] source Native control source.
 */
void logitech_input_map(LogitechInputState *state, LogitechInputModel model,
                        const LogitechInputSource *source) {
    *state = (LogitechInputState){.steering = source->steering, .hat = map_hat(source->buttons[0])};
    uint8_t first = source->buttons[0];
    uint8_t second = source->buttons[1];
    uint8_t third = source->buttons[2];

    if (model == LOGITECH_INPUT_MODEL_DRIVING_FORCE_EX) {
        uint32_t buttons = (uint32_t)((first >> 4) & 0x0fu) | (uint32_t)((third & 0x02u) >> 1) |
                           (uint32_t)((second & 0x01u) << 4) | (uint32_t)((second & 0x08u) << 2);
        uint8_t upper =
            (uint8_t)(((second & 0x02u) >> 1) | ((second & 0x10u) >> 3) | ((second & 0x40u) >> 4) |
                      ((second & 0x80u) >> 4) | ((second & 0x04u) << 2) | (second & 0x20u));
        state->buttons = buttons | (uint32_t)upper << 6;
        state->axes[0] = combined_axis(source);
        state->axes[1] = (uint8_t)(source->pedals[0] >> 8);
        state->axes[2] = (uint8_t)(source->pedals[1] >> 8);
        return;
    }

    if (model == LOGITECH_INPUT_MODEL_DRIVING_FORCE_PRO) {
        uint8_t steering_buttons = (uint8_t)(((first & 0x30u) >> 4) | ((third & 0x02u) >> 1));
        uint8_t secondary = (uint8_t)(((second & 0x04u) >> 2) | ((second & 0x20u) >> 4));
        if (source->sequential) {
            secondary |= (uint8_t)((source->sequential_buttons & 0x03u) << 2);
        }
        state->buttons = steering_buttons |
                         (uint32_t)driving_force_pro_primary_buttons(first, second) << 2 |
                         (uint32_t)secondary << 10;
        state->axes[0] = combined_axis(source);
        state->axes[1] = (uint8_t)(source->pedals[1] >> 8);
        state->axes[2] = (uint8_t)(source->pedals[2] >> 8);
        return;
    }

    uint8_t primary = primary_buttons(second);
    uint8_t shifter = (uint8_t)(((third & 0x02u) << 5) | ((third & 0x08u) << 4));
    uint8_t centered = 0;
    if (source->sequential) {
        primary = (uint8_t)((primary & 0xfcu) | ((source->sequential_buttons & 0x01u) << 1) |
                            ((source->sequential_buttons & 0x02u) >> 1));
    } else {
        shifter |= (uint8_t)((source->gear >> 1) & 0x3fu);
        centered = source->gear & 1u;
    }
    uint8_t hat_buttons = (uint8_t)((first >> 4) | ((third & 0x02u) >> 1));
    state->buttons = hat_buttons | (uint32_t)primary << 4 | (uint32_t)shifter << 12 |
                     (uint32_t)((third >> 2) & 1u) << 20 | (uint32_t)centered << 22;
    state->axes[0] = (uint8_t)(source->pedals[0] >> 8);
    state->axes[1] = (uint8_t)(source->pedals[1] >> 8);
    state->axes[2] = (uint8_t)(source->pedals[2] >> 8);
}

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
    return true;
}

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
