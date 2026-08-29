#ifndef OPENTEC_BASE_BOARD_TORQUE_KEY_H
#define OPENTEC_BASE_BOARD_TORQUE_KEY_H

#include <stdbool.h>
#include <stdint.h>

typedef enum {
    TORQUE_KEY_EVENT_NONE,
    TORQUE_KEY_EVENT_INSERTED,
    TORQUE_KEY_EVENT_REMOVED,
} TorqueKeyEvent;

typedef struct {
    uint32_t last_update_ms;
    uint16_t filter_position_ms;
    bool inserted;
    bool initialized;
} TorqueKey;

void torque_key_init(TorqueKey *key);
TorqueKeyEvent torque_key_update(TorqueKey *key, bool raw_inserted, uint32_t now_ms);

#endif
