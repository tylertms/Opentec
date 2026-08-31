#ifndef OPENTEC_MOTOR_FORCE_FEEDBACK_H
#define OPENTEC_MOTOR_FORCE_FEEDBACK_H

#include <stdbool.h>
#include <stdint.h>

/** @brief Maximum number of samples retained by the force-feedback filter. */
#define MOTOR_FORCE_FEEDBACK_FILTER_CAPACITY 40U

/** @brief Reserved effect slots used by the built-in force-feedback paths. */
enum {
    MOTOR_FORCE_FEEDBACK_POSITION_SLOT = 16, /**< Built-in position-window effect slot. */
    MOTOR_FORCE_FEEDBACK_DAMPER_SLOT = 18,   /**< Built-in velocity-damper effect slot. */
};

/**
 * @brief Decoded constant-force effect.
 *
 * Stores the signed force contribution recovered from a five-byte constant-effect payload.
 */
typedef struct {
    int32_t magnitude; /**< Signed force contribution before per-effect gain. */
} MotorConstantEffect;

/**
 * @brief Decoded position-window effect.
 *
 * Stores the signed boundaries, restoring directions, coefficients, and saturation recovered from
 * a five-byte position-window payload.
 */
typedef struct {
    int32_t lower_position;       /**< Lower signed position boundary. */
    int32_t upper_position;       /**< Upper signed position boundary. */
    uint8_t lower_coefficient;    /**< Coefficient applied below the lower boundary. */
    uint8_t upper_coefficient;    /**< Coefficient applied above the upper boundary. */
    int8_t lower_direction;       /**< Restoring direction below the lower boundary. */
    int8_t upper_direction;       /**< Restoring direction above the upper boundary. */
    uint16_t saturation;          /**< Maximum absolute position-window force. */
    uint16_t reserved;            /**< Reserved storage retained for effect layout compatibility. */
} MotorWindowEffect;

/**
 * @brief Decoded two-direction velocity effect.
 *
 * Stores independent responses for negative and positive velocity together with their shared
 * saturation and output-scaling mode.
 */
typedef struct {
    uint16_t positive_coefficient; /**< Coefficient selected for positive velocity. */
    uint16_t negative_coefficient; /**< Coefficient selected for negative velocity. */
    int8_t positive_direction;     /**< Output direction selected for positive velocity. */
    int8_t negative_direction;     /**< Output direction selected for negative velocity. */
    uint16_t saturation;            /**< Maximum absolute velocity-effect force. */
    bool steering_scaled;           /**< Whether the effect uses steering-scaled output. */
} MotorDirectionalEffect;

/**
 * @brief Live force-feedback scaling and filter settings.
 *
 * These values control position scaling, overall gain, per-effect gains, filtering, and the
 * position-window multiplier used by the motor force mixer.
 */
typedef struct {
    int32_t position_half_range;     /**< Positive position half-range used by window effects. */
    uint8_t overall_gain_percent;    /**< Overall force gain in percent. */
    uint8_t filter_setting;          /**< Force-filter setting from zero through one hundred. */
    uint8_t constant_gain_tenths;    /**< Constant-effect gain in tenths. */
    uint8_t window_gain_tenths;      /**< Position-window gain in tenths. */
    uint8_t directional_gain_tenths; /**< Directional-effect gain in tenths. */
    uint8_t window_multiplier;       /**< Position-window coefficient multiplier. */
} MotorForceFeedbackSettings;

/**
 * @brief Running state for the force-feedback moving-average filter.
 *
 * The state keeps a ring of signed samples and its running sum so each new force can replace one
 * sample without rescanning the window.
 */
typedef struct {
    int32_t samples[MOTOR_FORCE_FEEDBACK_FILTER_CAPACITY]; /**< Ring of signed force samples. */
    int32_t sum;       /**< Sum of the samples in the active window. */
    uint8_t length;    /**< Active moving-average window length. */
    uint8_t index;     /**< Ring index of the next sample to replace. */
    uint8_t setting;   /**< Filter setting used to configure the state. */
    bool initialized;  /**< Whether the state has been configured. */
} MotorForceFeedbackFilter;

/**
 * @brief Direction and magnitude fields for one resolved primary force.
 *
 * The direction flag and unsigned magnitude are the representation consumed by the motor drive
 * command.
 */
typedef struct {
    bool positive;       /**< True for nonnegative signed force. */
    uint16_t magnitude;  /**< Absolute force magnitude limited to the wire range. */
} MotorForceFeedbackOutput;

/**
 * @brief Returns the official force-feedback default settings.
 *
 * The defaults cover steering range, overall and per-effect gains, filtering, and position-window
 * scaling.
 *
 * @return Default motor effect scaling and filter settings.
 */
MotorForceFeedbackSettings motor_force_feedback_settings_default(void);

/**
 * @brief Applies live force-feedback parameter settings.
 *
 * Steering values below the maximum use linear conversion, while the maximum selects the fixed
 * full-range value. Each gain setting is updated only when it is within its accepted maximum.
 *
 * @param[in,out] settings Force-feedback settings to update.
 * @param[in] steering_range Signed steering-range parameter encoding.
 * @param[in] overall_gain_percent Overall force gain from zero through one hundred.
 * @param[in] filter_setting Moving-average filter setting from zero through one hundred.
 * @param[in] constant_gain_tenths Constant-effect gain from zero through twelve.
 * @param[in] window_gain_tenths Position-window gain from zero through twelve.
 * @param[in] directional_gain_tenths Directional-effect gain from zero through twelve.
 */
void motor_force_feedback_settings_apply(MotorForceFeedbackSettings *settings,
                                         int8_t steering_range, uint8_t overall_gain_percent,
                                         uint8_t filter_setting, uint8_t constant_gain_tenths,
                                         uint8_t window_gain_tenths,
                                         uint8_t directional_gain_tenths);

/**
 * @brief Decodes one official constant-force effect payload.
 *
 * The payload encoding selector chooses the short or full-width axis representation used to
 * reconstruct the signed force magnitude.
 *
 * @param[in] payload Five-byte constant-force payload following effect kind eight.
 * @return Decoded signed force magnitude.
 */
MotorConstantEffect motor_force_feedback_constant_decode(const uint8_t payload[5]);

/**
 * @brief Decodes one official position-window effect payload.
 *
 * Position boundaries are expanded relative to the supplied steering half-range, while the
 * coefficients, directions, and saturation are recovered from the payload.
 *
 * @param[in] payload Five-byte position-window payload following effect kind eleven.
 * @param[in] position_half_range Positive steering-position half-range used to expand boundaries.
 * @return Decoded position-window effect.
 */
MotorWindowEffect motor_force_feedback_window_decode(const uint8_t payload[5],
                                                     int32_t position_half_range);

/**
 * @brief Decodes one official two-direction velocity-effect payload.
 *
 * Positive and negative velocity responses retain independent coefficients and directions with a
 * shared saturation limit.
 *
 * @param[in] payload Five-byte velocity-effect payload following effect kind twelve.
 * @return Decoded directional effect.
 */
MotorDirectionalEffect motor_force_feedback_directional_decode(const uint8_t payload[5]);

/**
 * @brief Applies the official tenths-scale gain to a constant-force effect.
 *
 * The signed constant magnitude is multiplied by its live per-effect gain.
 *
 * @param[in] effect Constant-force effect to evaluate.
 * @param[in] gain_tenths Gain where ten selects the unscaled magnitude.
 * @return Scaled constant-force contribution.
 */
int32_t motor_force_feedback_constant_evaluate(const MotorConstantEffect *effect,
                                               uint8_t gain_tenths);

/**
 * @brief Evaluates the official signed velocity-effect transfer function.
 *
 * Velocity sign selects an independent coefficient and direction before saturation and either
 * normal or steering-scaled gain processing is applied.
 *
 * @param[in] effect Directional velocity effect to evaluate.
 * @param[in] velocity Signed motor velocity sample.
 * @param[in] gain_tenths Normal-effect gain where ten is unity.
 * @param[in] overall_gain_percent Overall gain used by steering-scaled effects and the damper slot.
 * @param[in] slot Effect slot number; the damper slot always uses steering-scaled processing.
 * @return Signed velocity-effect contribution.
 */
int32_t motor_force_feedback_directional_evaluate(const MotorDirectionalEffect *effect,
                                                  int32_t velocity, uint8_t gain_tenths,
                                                  uint8_t overall_gain_percent, uint8_t slot);

/**
 * @brief Evaluates one position-window effect and its internal velocity compensation.
 *
 * Position relative to the window selects a restoring branch, and every window effect adds the
 * supplied internal velocity compensation. The built-in position slot bypasses normal window gain.
 *
 * @param[in] effect Position-window effect to evaluate.
 * @param[in] position Signed motor position sample.
 * @param[in] velocity Signed motor velocity sample.
 * @param[in] multiplier Position-window coefficient multiplier.
 * @param[in] gain_tenths Normal window-effect gain where ten is unity.
 * @param[in] slot Effect slot number; the built-in position slot bypasses normal window gain.
 * @param[in] internal_effect Internal directional compensation added to the window result.
 * @param[in] directional_gain_tenths Internal directional-effect gain where ten is unity.
 * @param[in] overall_gain_percent Overall gain used by the internal directional effect.
 * @return Signed position-window contribution including velocity compensation.
 */
int32_t motor_force_feedback_window_evaluate(const MotorWindowEffect *effect, int32_t position,
                                             int32_t velocity, uint8_t multiplier,
                                             uint8_t gain_tenths, uint8_t slot,
                                             const MotorDirectionalEffect *internal_effect,
                                             uint8_t directional_gain_tenths,
                                             uint8_t overall_gain_percent);

/**
 * @brief Maps an official force-filter setting to its moving-average window length.
 *
 * Decade settings select the recovered table; all intermediate settings use a one-sample window.
 *
 * @param[in] setting Filter setting from zero through one hundred.
 * @return Moving-average sample count.
 */
uint8_t motor_force_feedback_filter_length(uint8_t setting);

/**
 * @brief Configures and clears the force-feedback moving-average window.
 *
 * A changed filter setting resets the ring position, accumulated sum, and stored samples.
 *
 * @param[in,out] filter Moving-average state to configure.
 * @param[in] setting Filter setting from zero through one hundred.
 */
void motor_force_feedback_filter_configure(MotorForceFeedbackFilter *filter, uint8_t setting);

/**
 * @brief Advances the force-feedback moving-average filter by one sample.
 *
 * The oldest ring sample is replaced and the signed accumulated average is returned.
 *
 * @param[in,out] filter Configured moving-average state.
 * @param[in] force New signed force sample.
 * @return Average across the complete configured window.
 */
int32_t motor_force_feedback_filter_apply(MotorForceFeedbackFilter *filter, int32_t force);

/**
 * @brief Converts signed primary force into direction and magnitude fields.
 *
 * Negative force clears the direction flag while preserving its unsigned magnitude.
 *
 * @param[in] force Signed primary force after filtering and safety compensation.
 * @return Direction flag and magnitude clamped to 65535.
 */
MotorForceFeedbackOutput motor_force_feedback_output_resolve(int32_t force);

#endif
