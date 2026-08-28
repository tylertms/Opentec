#ifndef OPENTEC_BASE_BOARD_POWER_BUTTON_H
#define OPENTEC_BASE_BOARD_POWER_BUTTON_H

#include <stdbool.h>
#include <stdint.h>

typedef enum {
    POWER_BUTTON_ACTION_NONE = 0,
    POWER_BUTTON_ACTION_SHUTDOWN = 1 << 0,
    POWER_BUTTON_ACTION_BLANK_DISPLAY = 1 << 1,
    POWER_BUTTON_ACTION_CLEAR_DISPLAY = 1 << 2,
} PowerButtonAction;

typedef struct {
    uint32_t press_ready_ms;
    uint32_t clear_after_ms;
    bool shutdown_started;
    bool active;
} PowerButton;

void power_button_init(PowerButton *button);
PowerButtonAction power_button_update(PowerButton *button, bool pressed, bool enabled,
                                      uint32_t now_ms);

#endif
