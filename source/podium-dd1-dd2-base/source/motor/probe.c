#include "motor/probe.h"

#include <stdbool.h>

#include "platform/aux_bus.h"

enum {
    MOTOR_AUX_BUS_ADDRESS = 0x78,
    MOTOR_STATUS_REGISTER = 0,
    MOTOR_VERSION_REGISTER = 1,
    MOTOR_PROBE_FAILURE_LIMIT = 3,
};

void motor_probe_init(MotorProbe *probe) {
    probe->phase = MOTOR_PROBE_IDLE;
    probe->failures = 0;
    probe->transfer_active = false;
}

void motor_probe_start(MotorProbe *probe) {
    if (probe->transfer_active) {
        return;
    }

    probe->phase = MOTOR_PROBE_STATUS;
    probe->failures = 0;
}

static void fail_or_retry(MotorProbe *probe) {
    probe->failures++;
    if (probe->failures >= MOTOR_PROBE_FAILURE_LIMIT) {
        probe->phase = MOTOR_PROBE_FAILED;
    }
}

static void complete_transfer(MotorProbe *probe, bool succeeded) {
    platform_aux_bus_clear();
    probe->transfer_active = false;

    if (!succeeded) {
        fail_or_retry(probe);
        return;
    }

    probe->failures = 0;
    if (probe->phase == MOTOR_PROBE_STATUS) {
        probe->phase = MOTOR_PROBE_VERSION;
    } else if (motor_identity_decode(probe->status, probe->version, &probe->identity)) {
        probe->phase = MOTOR_PROBE_COMPLETE;
    } else {
        probe->phase = MOTOR_PROBE_FAILED;
    }
}

void motor_probe_run(MotorProbe *probe) {
    PlatformAuxBusStatus bus_status = platform_aux_bus_status();
    if (probe->transfer_active) {
        if (bus_status == PLATFORM_AUX_BUS_BUSY) {
            return;
        }
        complete_transfer(probe, bus_status == PLATFORM_AUX_BUS_SUCCEEDED);
        bus_status = PLATFORM_AUX_BUS_IDLE;
    }

    if (bus_status != PLATFORM_AUX_BUS_IDLE) {
        return;
    }

    if (probe->phase == MOTOR_PROBE_STATUS) {
        probe->transfer_active = platform_aux_bus_start_read(
            MOTOR_AUX_BUS_ADDRESS, MOTOR_STATUS_REGISTER, &probe->status, 1);
    } else if (probe->phase == MOTOR_PROBE_VERSION) {
        probe->transfer_active = platform_aux_bus_start_read(
            MOTOR_AUX_BUS_ADDRESS, MOTOR_VERSION_REGISTER, probe->version, sizeof(probe->version));
    }

    if (!probe->transfer_active &&
        (probe->phase == MOTOR_PROBE_STATUS || probe->phase == MOTOR_PROBE_VERSION)) {
        fail_or_retry(probe);
    }
}

const MotorIdentity *motor_probe_identity(const MotorProbe *probe) {
    return probe->phase == MOTOR_PROBE_COMPLETE ? &probe->identity : 0;
}
