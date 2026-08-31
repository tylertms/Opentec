#ifndef OPENTEC_MOTOR_PROTOCOL_H
#define OPENTEC_MOTOR_PROTOCOL_H

#include <stdbool.h>
#include <stdint.h>

#include "force_feedback/engine.h"
#include "link/frame.h"
#include "motor/drive.h"

/**
 * @brief Persistent motor-link protocol and force-feedback state.
 *
 * Retains the latest status, center, drive command, service deadlines, force-feedback engine, and
 * response flags between motor service ticks.
 */
typedef struct {
    MotorForceFeedbackEngine force_feedback; /**< Local force-feedback engine state. */
    MotorDriveCommand live_drive;             /**< Most recently resolved motor drive command. */
    int16_t center;                            /**< Commanded encoder center. */
    uint8_t status;                            /**< Latest motor-link status bit field. */
    uint8_t normal_output_percent;             /**< Product output scale outside full-torque mode. */
    uint32_t next_force_feedback_tick;         /**< Next tick eligible for force mixing. */
    uint32_t next_force_ramp_tick;             /**< Next tick eligible for ramp advancement. */
    bool live_drive_updated;                   /**< Whether a new drive command awaits publication. */
    bool replay;                               /**< Whether the latest received frame failed validation. */
} MotorProtocolState;

/**
 * @brief Initializes motor-link protocol, drive, and force-feedback state.
 *
 * Clears persistent state, stores the product output scale, schedules initial service deadlines,
 * and installs the force-feedback engine defaults.
 *
 * @param[out] state Motor protocol state to initialize.
 * @param[in] normal_output_percent Product-specific output scale outside full-torque mode.
 */
void motor_protocol_initialize(MotorProtocolState *state, uint8_t normal_output_percent);

/**
 * @brief Applies one decoded motor-link force or status frame.
 *
 * Live-force frames update the center and resolve a drive command unless local effects are selected.
 * Status frames publish status and apply their seven-byte local-effect command.
 *
 * @param[in,out] state Persistent motor protocol state.
 * @param[in] frame Validated motor-link frame.
 * @return False for an unknown frame type or rejected effect command.
 */
bool motor_protocol_frame_apply(MotorProtocolState *state, const MotorLinkFrame *frame);

/**
 * @brief Applies a frame validation result and updates replay state.
 *
 * Invalid boundaries or checksums set the replay indication for the next position response.
 * Valid frames clear that indication before their decoded payload is applied.
 *
 * @param[in,out] state Persistent motor protocol and replay state.
 * @param[in] result Boundary and checksum validation result.
 * @param[in] frame Decoded frame supplied when validation succeeds.
 * @return True when the frame was valid and its type was supported.
 */
bool motor_protocol_frame_result_apply(MotorProtocolState *state, MotorLinkFrameResult result,
                                       const MotorLinkFrame *frame);

/**
 * @brief Services the local force-feedback path for one motor tick.
 *
 * Applies status gates, advances ramp and service deadlines, mixes active effects, and resolves a
 * new product-scaled drive command when local effects are selected.
 *
 * @param[in,out] state Persistent motor protocol and force-feedback state.
 * @param[in] now Current motor service tick.
 * @param[in] centered_position Centered and clamped position used by ordinary effects.
 * @param[in] position Raw extended encoder position used by the travel-limit effect.
 * @param[in] velocity Current signed filtered encoder velocity.
 * @return True when a new live drive command was produced.
 */
bool motor_protocol_force_feedback_service(MotorProtocolState *state, uint32_t now,
                                           int32_t centered_position, int32_t position,
                                           int32_t velocity);

#endif
