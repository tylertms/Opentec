#ifndef OPENTEC_MOTOR_DRIVE_H
#define OPENTEC_MOTOR_DRIVE_H

#include <stdbool.h>
#include <stdint.h>

/** @brief Number of indexed force-interpolation response settings. */
#define MOTOR_DRIVE_INTERPOLATION_SETTING_COUNT 20U

/** @brief Force command and controller coefficients produced for one drive update. */
typedef struct {
    int16_t primary_current; /**< Signed primary current command. */
    int16_t secondary_current; /**< Signed secondary current command. */
    uint16_t controller_coefficient; /**< FOC controller proportional coefficient. */
    uint16_t controller_scale; /**< FOC controller integral scale. */
    uint16_t primary_positive; /**< Primary force direction flag. */
} MotorDriveCommand;

/** @brief Persistent state for the force-interpolation filter. */
typedef struct {
    uint32_t accumulator; /**< Fixed-point interpolation accumulator. */
    int16_t output; /**< Last interpolated output. */
    int16_t error; /**< Last sample-to-output error. */
} MotorDriveInterpolationState;

/** @brief Persistent state for the retained-position friction effect. */
typedef struct {
    int32_t anchor_position; /**< Encoder position used as the friction anchor. */
    int32_t previous_raw; /**< Previous unbounded friction output. */
    uint32_t excursion_limit; /**< Maximum anchor displacement before repositioning. */
    uint32_t output_scale; /**< Position-to-current scale. */
} MotorDriveFrictionState;

/** @brief Persistent state for product-current derating. */
typedef struct {
    int16_t current_scale; /**< Current applied product scale. */
    int16_t target_scale; /**< Latest thermal and command-derived target scale. */
    int16_t error; /**< Difference between target and current scale. */
} MotorDriveDeratingState;

/** @brief Latched state for the over-speed current safety cutoff. */
typedef struct {
    bool latched; /**< True after an over-speed sample has armed the cutoff. */
} MotorDriveOverspeedState;

/**
 * @brief Converts force inputs into signed current commands and FOC coefficients.
 *
 * Primary and positive secondary inputs are capped at the force limit, optionally scaled by the
 * normal output percentage, and combined with the controller-selection flags.
 *
 * @param[in] positive True for positive primary-force direction.
 * @param[in] primary Unsigned primary force magnitude.
 * @param[in] secondary Signed secondary force component.
 * @param[in] normal_output_percent Normal-mode output percentage.
 * @param[in] full_torque True to bypass normal-mode percentage scaling.
 * @param[in] reduced_controller True to select the reduced controller coefficient.
 * @param[in] secondary_disabled True to suppress the secondary current.
 * @return Product-scaled current commands and FOC coefficients.
 */
MotorDriveCommand motor_drive_command_resolve(bool positive, uint32_t primary, int32_t secondary,
                                              uint8_t normal_output_percent, bool full_torque,
                                              bool reduced_controller, bool secondary_disabled);

/**
 * @brief Advances the force-interpolation filter for one drive sample.
 *
 * Indexed settings apply their fixed-point response; an out-of-range setting bypasses the filter
 * and clears its dynamic state.
 *
 * @param[in,out] state Persistent interpolation state to update.
 * @param[in] sample Signed primary current sample.
 * @param[in] setting Interpolation response index.
 * @return Interpolated or bypassed primary current.
 */
int16_t motor_drive_interpolation_step(MotorDriveInterpolationState *state, int16_t sample,
                                       uint8_t setting);

/**
 * @brief Resolves a natural damping or inertia current component.
 *
 * The tuning setting scales the supplied motion sample through the shared fixed-point
 * natural-effect gain.
 *
 * @param[in] motion Signed filtered velocity or acceleration sample.
 * @param[in] setting Natural-effect tuning value.
 * @return Signed natural-effect current component.
 */
int16_t motor_drive_motion_resistance_resolve(int16_t motion, uint8_t setting);

/**
 * @brief Initializes retained-position friction state.
 *
 * The hardware scale determines the excursion limit and position-to-current output scale.
 *
 * @param[out] state Friction state to initialize.
 * @param[in] hardware_scale Board-selected motion scale.
 */
void motor_drive_friction_initialize(MotorDriveFrictionState *state, uint32_t hardware_scale);

/**
 * @brief Advances the retained-position friction effect.
 *
 * The setting limits displacement from the anchor and a direction reversal emits one zero current
 * sample before the new direction is passed through.
 *
 * @param[in,out] state Persistent friction state to update.
 * @param[in] position Current extended encoder position.
 * @param[in] setting Natural-friction tuning value.
 * @return Signed friction current component.
 */
int16_t motor_drive_friction_step(MotorDriveFrictionState *state, int32_t position,
                                  uint16_t setting);

/**
 * @brief Initializes product-current derating state.
 *
 * The current scale starts at the supplied normal scale, while target and error are cleared.
 *
 * @param[out] state Derating state to initialize.
 * @param[in] normal_scale Product normal current scale.
 */
void motor_drive_derating_initialize(MotorDriveDeratingState *state, int16_t normal_scale);

/**
 * @brief Applies product scaling and updates the derating target and error.
 *
 * Minimum mode applies the minimum scale directly; normal mode derives thermal and command-load
 * limits and scales the current using the current derating state.
 *
 * @param[in,out] state Persistent derating state to update in normal mode.
 * @param[in] current Signed current after natural effects.
 * @param[in] motor_temperature_sample Averaged motor-temperature ADC sample.
 * @param[in] normal_scale Product normal current scale.
 * @param[in] minimum_scale Product minimum current scale.
 * @param[in] minimum_mode True to apply minimum_scale directly.
 * @return Product-scaled signed current.
 */
int16_t motor_drive_product_scale(MotorDriveDeratingState *state, int16_t current,
                                  uint16_t motor_temperature_sample, int16_t normal_scale,
                                  int16_t minimum_scale, bool minimum_mode);

/**
 * @brief Applies the latched over-speed current cutoff.
 *
 * The first sample outside the speed threshold arms the latch and is returned; later samples return
 * zero until the state is cleared.
 *
 * @param[in,out] state Over-speed latch state to update.
 * @param[in] current Product-scaled signed current.
 * @param[in] velocity Filtered position delta used for the safety check.
 * @return Current before latching, or zero after latching.
 */
int16_t motor_drive_overspeed_apply(MotorDriveOverspeedState *state, int16_t current,
                                    int16_t velocity);

#endif
