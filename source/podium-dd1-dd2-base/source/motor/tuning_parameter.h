#ifndef OPENTEC_BASE_MOTOR_TUNING_PARAMETER_H
#define OPENTEC_BASE_MOTOR_TUNING_PARAMETER_H

#include <stdint.h>

#include "profile/tuning.h"

/**
 * @brief Logical tuning parameters encoded for the motor controller.
 *
 * Values follow the order used by the synchronization mask and end with a count sentinel.
 */
typedef enum {
    MOTOR_TUNING_SENSITIVITY,             /**< Steering-range sensitivity parameter. */
    MOTOR_TUNING_FORCE_FEEDBACK_STRENGTH, /**< Overall force-feedback strength parameter. */
    MOTOR_TUNING_FORCE_FEEDBACK_SCALE,    /**< Force-feedback scale mode parameter. */
    MOTOR_TUNING_NATURAL_DAMPER,          /**< Natural damper parameter. */
    MOTOR_TUNING_NATURAL_FRICTION,        /**< Natural friction parameter. */
    MOTOR_TUNING_NATURAL_INERTIA,         /**< Natural inertia parameter. */
    MOTOR_TUNING_INTERPOLATION_FILTER,    /**< Interpolation-filter parameter. */
    MOTOR_TUNING_FORCE_EFFECT_INTENSITY,  /**< Overall force-effect intensity parameter. */
    MOTOR_TUNING_FORCE_EFFECT_STRENGTH,   /**< Force-effect strength parameter. */
    MOTOR_TUNING_SPRING_EFFECT_STRENGTH,  /**< Spring-effect strength parameter. */
    MOTOR_TUNING_DAMPER_EFFECT_STRENGTH,  /**< Damper-effect strength parameter. */
    MOTOR_TUNING_PARAMETER_COUNT,         /**< Number of encodable motor tuning parameters. */
} MotorTuningParameter;

/**
 * @brief Runtime inputs used to encode motor tuning parameters.
 *
 * Supplies automatic-range, scaling, operating-mode, calibration, and controller-capability values
 * that affect encoded writes.
 */
typedef struct {
    uint16_t automatic_rotation_degrees; /**< Current automatic steering range in degrees. */
    uint8_t ramp_percent;                /**< Active force ramp percentage. */
    uint8_t strength_percent;            /**< Active hardware force strength percentage. */
    uint8_t xbox_mode;                   /**< Nonzero when Xbox operating-mode encoding applies. */
    uint8_t calibration_active;          /**< Nonzero while calibration encoding applies. */
    uint8_t extended_parameters; /**< Nonzero when the controller supports extended parameters. */
} MotorTuningContext;

/**
 * @brief Encoded motor-controller parameter write.
 *
 * Stores the controller parameter address, transfer width, and up to two little-endian data bytes.
 */
typedef struct {
    uint8_t address; /**< Motor-controller parameter address. */
    uint8_t length;  /**< Number of data bytes to transmit. */
    uint8_t data[2]; /**< Little-endian parameter data bytes. */
} MotorParameterWrite;

/**
 * @brief Encodes one logical tuning parameter for the motor controller.
 *
 * Maps the selected profile setting to its protocol address and representation, applying runtime
 * context and controller capability rules.
 *
 * @param[in] parameter Logical motor tuning parameter to encode.
 * @param[in] profile Active tuning profile.
 * @param[in] context Runtime motor tuning context.
 * @param[out] write Encoded motor-controller parameter write.
 * @return One when the parameter is supported; otherwise zero.
 */
uint8_t motor_tuning_parameter_encode(MotorTuningParameter parameter, const TuningProfile *profile,
                                      const MotorTuningContext *context,
                                      MotorParameterWrite *write);

#endif
