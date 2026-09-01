#ifndef OPENTEC_BASE_MOTOR_PROBE_H
#define OPENTEC_BASE_MOTOR_PROBE_H

#include <stdbool.h>
#include <stdint.h>

#include "motor/identity.h"

/**
 * @brief Motor-controller discovery phases.
 *
 * Tracks the bounded status and version exchange used to classify the connected controller.
 */
typedef enum {
    MOTOR_PROBE_IDLE,     /**< Discovery has not started. */
    MOTOR_PROBE_STATUS,   /**< The initial status register is being read. */
    MOTOR_PROBE_VERSION,  /**< The four-byte version register is being read. */
    MOTOR_PROBE_COMPLETE, /**< A valid motor-controller identity is available. */
    MOTOR_PROBE_FAILED,   /**< Discovery timed out or returned an invalid identity. */
} MotorProbePhase;

/**
 * @brief Motor-controller discovery state and responses.
 *
 * Stores the in-progress auxiliary-bus transaction, its deadline, and the responses used to decode
 * the motor-controller identity.
 */
typedef struct {
    MotorIdentity identity; /**< Decoded identity from the status and version responses. */
    MotorProbePhase phase;  /**< Current discovery phase. */
    uint32_t deadline_ms;   /**< Monotonic deadline for the current discovery attempt. */
    uint8_t status;         /**< Initial status-register response byte. */
    uint8_t version[4];     /**< Four-byte version-register response. */
    bool transfer_active;   /**< True while an auxiliary-bus discovery read is in progress. */
} MotorProbe;

/**
 * @brief Resets motor-controller discovery state.
 *
 * Clears the discovery phase, deadline, response buffers, and transfer ownership before a new
 * identification attempt.
 *
 * @param[out] probe Motor-controller discovery state to initialize.
 */
void motor_probe_init(MotorProbe *probe);

/**
 * @brief Starts a motor-controller identification attempt.
 *
 * Begins the status-register phase and sets its shared one-second deadline when no transfer is
 * already active.
 *
 * @param[in,out] probe Motor-controller discovery state.
 * @param[in] now_ms Current monotonic time in milliseconds.
 */
void motor_probe_start(MotorProbe *probe, uint32_t now_ms);

/**
 * @brief Advances motor-controller identification.
 *
 * Completes an active read, retries status or version reads while the deadline permits, and marks
 * the probe complete only after a valid identity is decoded.
 *
 * @param[in,out] probe Motor-controller discovery state and response buffers.
 * @param[in] now_ms Current monotonic time in milliseconds.
 */
void motor_probe_run(MotorProbe *probe, uint32_t now_ms);

/**
 * @brief Returns the identified motor controller.
 *
 * Hides partial and failed discovery state by returning an identity only after the probe completes.
 *
 * @param[in] probe Motor-controller discovery state.
 * @return Decoded motor-controller identity, or null before completion and after failure.
 */
const MotorIdentity *motor_probe_identity(const MotorProbe *probe);

#endif
