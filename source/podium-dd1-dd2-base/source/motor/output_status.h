#ifndef OPENTEC_BASE_MOTOR_OUTPUT_STATUS_H
#define OPENTEC_BASE_MOTOR_OUTPUT_STATUS_H

#include <stdbool.h>
#include <stdint.h>

/**
 * @brief Runtime conditions used to build a motor output status byte.
 *
 * Captures host gates, connection state, and torque acknowledgement for one status update.
 */
typedef struct {
    bool xbox_mode;       /**< True when Xbox direct-force mode retains selected output gates. */
    bool force_enabled;   /**< True when the host permits force output. */
    bool override_active; /**< True when a force-output override is active. */
    bool transition_active;  /**< True while force output is changing state. */
    bool primary_disabled;   /**< True when the primary host output is disabled. */
    bool secondary_disabled; /**< True when the secondary host output is disabled. */
    bool usb_disconnected;   /**< True when the USB connection is disconnected. */
    bool full_torque;        /**< True when the full-torque acknowledgement is active. */
} MotorOutputStatusInput;

/**
 * @brief Persistent motor output status byte.
 *
 * Retains the last encoded status so Xbox mode can preserve its selected gate bits between
 * updates.
 */
typedef struct {
    uint8_t value; /**< Most recently encoded motor output status byte. */
} MotorOutputStatus;

/**
 * @brief Initializes motor output status state.
 *
 * Clears all status bits before the first output frame is built.
 *
 * @param[out] status Motor output status state to initialize.
 */
void motor_output_status_init(MotorOutputStatus *status);

/**
 * @brief Builds the next motor output status byte.
 *
 * Sets remote effects for every non-Xbox update, including direct-force operation, and retains the
 * selected output gates during Xbox mode.
 *
 * @param[in,out] status Persistent motor output status state.
 * @param[in] input Current force-output conditions.
 * @return Encoded status byte for the next motor output frame.
 */
uint8_t motor_output_status_update(MotorOutputStatus *status, const MotorOutputStatusInput *input);

#endif
