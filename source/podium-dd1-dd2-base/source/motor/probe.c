#include "motor/probe.h"

#include <stdbool.h>
#include <stdint.h>

#include "platform/aux_bus.h"
#include "platform/time.h"

/**
 * @brief Auxiliary-bus settings for motor-controller discovery.
 */
enum {
    MOTOR_AUX_BUS_ADDRESS = 0x78, /**< Auxiliary-bus address of the motor controller. */
    MOTOR_STATUS_REGISTER = 0,    /**< Register containing the initial controller status byte. */
    MOTOR_VERSION_REGISTER = 1,   /**< Register containing the four-byte controller version. */
    MOTOR_PROBE_TIMEOUT_MS =
        1000, /**< Maximum duration of one discovery attempt in milliseconds. */
};

/**
 * @brief Resets motor-controller identification state.
 *
 * Clears the probe phase, deadline, response buffers, and transfer ownership so a new discovery
 * attempt can start from an idle auxiliary bus.
 *
 * @param[out] probe Motor-controller probe state.
 */
void motor_probe_init(MotorProbe *probe) {
    probe->phase = MOTOR_PROBE_IDLE;
    probe->deadline_ms = 0;
    probe->status = 0;
    for (uint8_t index = 0; index < sizeof(probe->version); index++) {
        probe->version[index] = 0;
    }
    probe->transfer_active = false;
}

/**
 * @brief Starts the one-second motor-controller identification window.
 *
 * Selects the status-read phase and establishes the common deadline for all status and version
 * retries. An in-flight transfer is left undisturbed.
 *
 * @param[in,out] probe Motor-controller probe state.
 * @param[in] now_ms Current monotonic time in milliseconds.
 */
void motor_probe_start(MotorProbe *probe, uint32_t now_ms) {
    if (probe->transfer_active) {
        return;
    }

    platform_aux_bus_clear();
    probe->phase = MOTOR_PROBE_STATUS;
    probe->deadline_ms = now_ms + MOTOR_PROBE_TIMEOUT_MS;
}

/**
 * @brief Applies the result of one motor-controller identification transfer.
 *
 * Releases the auxiliary bus, advances a successful status read to the version phase, and decodes
 * a successful version read. Failed transfers leave the current phase available for retry until
 * the shared deadline expires.
 *
 * @param[in,out] probe Motor-controller probe state and response buffers.
 * @param[in] succeeded True when the auxiliary-bus transfer completed successfully.
 */
static void complete_transfer(MotorProbe *probe, bool succeeded) {
    platform_aux_bus_clear();
    probe->transfer_active = false;

    if (!succeeded || probe->phase == MOTOR_PROBE_FAILED) {
        return;
    }

    if (probe->phase == MOTOR_PROBE_STATUS) {
        probe->phase = MOTOR_PROBE_VERSION;
    } else if (motor_identity_decode(probe->status, probe->version, &probe->identity)) {
        probe->phase = MOTOR_PROBE_COMPLETE;
    } else {
        probe->phase = MOTOR_PROBE_FAILED;
    }
}

/**
 * @brief Advances status and version reads until identification succeeds or its deadline expires.
 *
 * Completes any prior auxiliary-bus transfer, retries failed reads while time remains, and starts
 * the next status or four-byte version read whenever the bus is idle.
 *
 * @param[in,out] probe Motor-controller probe state and decoded identity.
 * @param[in] now_ms Current monotonic time in milliseconds.
 */
void motor_probe_run(MotorProbe *probe, uint32_t now_ms) {
    PlatformAuxBusStatus bus_status = platform_aux_bus_status();
    if (probe->transfer_active) {
        if (bus_status == PLATFORM_AUX_BUS_BUSY) {
            if (platform_time_reached(now_ms, probe->deadline_ms)) {
                probe->phase = MOTOR_PROBE_FAILED;
            }
            return;
        }
        complete_transfer(probe, bus_status == PLATFORM_AUX_BUS_SUCCEEDED);
        bus_status = PLATFORM_AUX_BUS_IDLE;
    }

    if (probe->phase != MOTOR_PROBE_COMPLETE && probe->phase != MOTOR_PROBE_FAILED &&
        probe->phase != MOTOR_PROBE_IDLE && platform_time_reached(now_ms, probe->deadline_ms)) {
        probe->phase = MOTOR_PROBE_FAILED;
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
}

/**
 * @brief Returns the identified motor controller after a successful probe.
 *
 * Hides partial and failed identification state from downstream services.
 *
 * @param[in] probe Motor-controller probe state.
 * @return Decoded identity, or null before completion and after failure.
 */
const MotorIdentity *motor_probe_identity(const MotorProbe *probe) {
    return probe->phase == MOTOR_PROBE_COMPLETE ? &probe->identity : 0;
}
