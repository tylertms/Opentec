#ifndef OPENTEC_MOTOR_CENTER_H
#define OPENTEC_MOTOR_CENTER_H

#include <stdbool.h>
#include <stdint.h>

/** @brief Persistent center command and activation state. */
typedef struct {
    int16_t requested; /**< Last center command accepted from the motor link. */
    bool active; /**< True after center-command handling has been enabled. */
} MotorCenterState;

/**
 * @brief Applies a changed center command and normalizes the encoder offset.
 *
 * The offset remains within one encoder revolution, with the shared positive or negative endpoint
 * resolved from the current counter value.
 *
 * @param[in,out] state Persistent center command and activation state.
 * @param[in] requested New signed center command.
 * @param[in] encoder_modulus FTM2 modulus for one encoder revolution.
 * @param[in] encoder_counter Volatile raw encoder counter to resolve a shared endpoint.
 * @param[in] wrap_threshold Counter threshold separating endpoint representations.
 * @param[in,out] encoder_offset Persistent signed revolution offset to normalize.
 * @return True when an active center command changed.
 */
bool motor_center_command_apply(MotorCenterState *state, int16_t requested, int32_t encoder_modulus,
                                const volatile uint32_t *encoder_counter, uint16_t wrap_threshold,
                                int32_t *encoder_offset);

/**
 * @brief Subtracts the center command from an encoder position and limits the result.
 *
 * The result is constrained to the force-feedback position range of plus or minus 82,880 counts.
 *
 * @param[in] position Current extended encoder position.
 * @param[in] center Signed center command.
 * @return Centered position limited to the force-feedback range.
 */
int32_t motor_centered_position_resolve(int32_t position, int16_t center);

#endif
