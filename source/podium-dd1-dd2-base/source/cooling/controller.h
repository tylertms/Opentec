#ifndef OPENTEC_BASE_COOLING_CONTROLLER_H
#define OPENTEC_BASE_COOLING_CONTROLLER_H

#include <stdbool.h>
#include <stdint.h>

typedef enum {
    COOLING_PHASE_INITIALIZE,
    COOLING_PHASE_IDLE,
    COOLING_PHASE_LOW,
    COOLING_PHASE_MEDIUM,
    COOLING_PHASE_HIGH,
    COOLING_PHASE_NEAR_MAXIMUM,
    COOLING_PHASE_FULL,
    COOLING_PHASE_STANDARD_LIMIT,
    COOLING_PHASE_START_MANAGED_WINDOW,
    COOLING_PHASE_MANAGED_WINDOW,
    COOLING_PHASE_MANAGED_LIMIT,
} CoolingPhase;

typedef struct {
    CoolingPhase phase;
    int8_t low_threshold_offset;
    int8_t high_threshold_offset;
    int32_t primary_delay_ms;
    int32_t secondary_delay_ms;
    uint32_t primary_deadline_ms;
    uint32_t secondary_deadline_ms;
    uint8_t primary_duty_percent;
    uint8_t secondary_duty_percent;
    uint8_t force_scale_percent;
    bool dual_fan_mode;
    bool automatic_control_suspended;
} CoolingController;

void cooling_controller_init(CoolingController *controller, bool dual_fan_mode);
void cooling_controller_set_low_threshold_offset(CoolingController *controller, int8_t offset);
void cooling_controller_set_high_threshold_offset(CoolingController *controller, int8_t offset);
void cooling_controller_set_primary_delay_seconds(CoolingController *controller, int8_t seconds);
void cooling_controller_set_secondary_delay_seconds(CoolingController *controller, int8_t seconds);
void cooling_controller_set_suspend_request(CoolingController *controller, uint8_t request);
void cooling_controller_update(CoolingController *controller, float motor_temperature_c,
                               bool managed_motor_present, bool output_inhibited, uint32_t now_ms);

#endif
