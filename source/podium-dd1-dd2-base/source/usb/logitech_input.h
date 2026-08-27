#ifndef OPENTEC_BASE_USB_LOGITECH_INPUT_H
#define OPENTEC_BASE_USB_LOGITECH_INPUT_H

#include <stdbool.h>
#include <stdint.h>

enum {
    LOGITECH_DRIVING_FORCE_EX_REPORT_SIZE = 7,
    LOGITECH_DRIVING_FORCE_PRO_REPORT_SIZE = 8,
    LOGITECH_G27_REPORT_SIZE = 11,
};

typedef struct {
    uint16_t steering;
    uint32_t buttons;
    uint8_t hat;
    uint8_t axes[3];
} LogitechInputState;

bool logitech_driving_force_ex_encode(uint8_t report[LOGITECH_DRIVING_FORCE_EX_REPORT_SIZE],
                                      const LogitechInputState *state);
bool logitech_driving_force_pro_encode(uint8_t report[LOGITECH_DRIVING_FORCE_PRO_REPORT_SIZE],
                                       const LogitechInputState *state);
bool logitech_g27_encode(uint8_t report[LOGITECH_G27_REPORT_SIZE], const LogitechInputState *state);

#endif
