#ifndef OPENTEC_BASE_MOTOR_STARTUP_CENTERING_H
#define OPENTEC_BASE_MOTOR_STARTUP_CENTERING_H

#include <stdbool.h>
#include <stdint.h>

typedef enum {
    MOTOR_STARTUP_CENTERING_IDLE,
    MOTOR_STARTUP_CENTERING_PREPARING,
    MOTOR_STARTUP_CENTERING_WAITING,
    MOTOR_STARTUP_CENTERING_ACTIVE,
    MOTOR_STARTUP_CENTERING_COMPLETE,
} MotorStartupCenteringPhase;

typedef struct {
    MotorStartupCenteringPhase phase;
    uint32_t deadline_ms;
    uint8_t parameter_value;
    bool transfer_active;
} MotorStartupCentering;

void motor_startup_centering_init(MotorStartupCentering *centering, uint32_t now_ms,
                                  bool damping_required);
int32_t motor_startup_centering_run(MotorStartupCentering *centering, uint32_t now_ms,
                                    bool position_available, int32_t centered_position);
bool motor_startup_centering_active(const MotorStartupCentering *centering);
bool motor_startup_centering_complete(const MotorStartupCentering *centering);

#endif
