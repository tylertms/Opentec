#ifndef OPENTEC_BASE_BOARD_STATUS_LED_H
#define OPENTEC_BASE_BOARD_STATUS_LED_H

#include <stdbool.h>
#include <stdint.h>

typedef enum {
    STATUS_LED_CYCLE_START,
    STATUS_LED_WAITING_TO_TURN_ON,
    STATUS_LED_WAITING_TO_TURN_OFF,
    STATUS_LED_CYCLE_END,
} StatusLedPhase;

typedef struct {
    StatusLedPhase phase;
    uint32_t deadline_ms;
    bool on;
} StatusLed;

void status_led_init(StatusLed *led);
bool status_led_update(StatusLed *led, uint32_t now_ms);

#endif
