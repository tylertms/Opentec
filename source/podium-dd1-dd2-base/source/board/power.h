#ifndef OPENTEC_BASE_BOARD_POWER_H
#define OPENTEC_BASE_BOARD_POWER_H

#include <stdbool.h>
#include <stdint.h>

typedef enum {
    POWER_PHASE_WAITING_FOR_START,
    POWER_PHASE_READY,
    POWER_PHASE_BUTTON_HELD,
    POWER_PHASE_SHUTDOWN_DELAY,
    POWER_PHASE_OFF,
} PowerPhase;

typedef enum {
    POWER_ACTION_NONE,
    POWER_ACTION_ENABLE_LATCH,
    POWER_ACTION_TORQUE_REQUEST_CHANGED,
    POWER_ACTION_BEGIN_SHUTDOWN,
    POWER_ACTION_FINISH_SHUTDOWN,
} PowerAction;

typedef struct {
    PowerPhase phase;
    uint32_t deadline_ms;
    bool torque_disabled;
} PowerController;

void power_controller_init(PowerController *controller);
PowerAction power_controller_update(PowerController *controller, bool button_pressed,
                                    bool button_control_enabled, uint32_t now_ms);

#endif
