#ifndef OPENTEC_MOTOR_FOC_H
#define OPENTEC_MOTOR_FOC_H

#include "rtcesl.h"

/** @brief Persistent current filters, D/Q PI controllers, and anti-windup flags. */
typedef struct {
    GFLIB_CTRL_PI_P_AW_T_A32 d_controller; /**< D-axis current PI controller. */
    GFLIB_CTRL_PI_P_AW_T_A32 q_controller; /**< Q-axis current PI controller. */
    GDFLIB_FILTER_IIR1_T_F32 q_current_filter; /**< Q-axis current filter. */
    GDFLIB_FILTER_IIR1_T_F32 d_current_filter; /**< D-axis current filter. */
    bool_t stop_d_integrator; /**< True when D-axis integration is held. */
    bool_t stop_q_integrator; /**< True when Q-axis integration is held. */
} MotorFocState;

/** @brief Inputs consumed by one field-oriented current-control cycle. */
typedef struct {
    GMCLIB_3COOR_T_F16 phase_current; /**< Measured three-phase current. */
    GMCLIB_2COOR_DQ_T_F16 current_reference; /**< Requested D-axis and Q-axis currents. */
    GMCLIB_2COOR_SINCOS_T_F16 rotor_sin_cos; /**< Rotor-angle sine and cosine values. */
    frac16_t dc_bus_voltage; /**< Measured DC-bus voltage. */
} MotorFocInput;

/** @brief Outputs produced by one field-oriented current-control cycle. */
typedef struct {
    GMCLIB_2COOR_DQ_T_F16 measured_current; /**< Measured current in rotating coordinates. */
    GMCLIB_2COOR_DQ_T_F16 filtered_current; /**< Filtered rotating-coordinate current. */
    GMCLIB_2COOR_DQ_T_F16 voltage; /**< Commanded D-axis and Q-axis voltage. */
    GMCLIB_3COOR_T_F16 duty; /**< Space-vector modulation phase duties. */
    uint16_t sector; /**< Space-vector modulation sector. */
} MotorFocOutput;

/**
 * @brief Initializes current filters and D/Q PI controllers.
 *
 * Controller histories and anti-windup flags are cleared using the motor-control coefficients.
 *
 * @param[out] state Field-oriented control state to initialize.
 */
void motor_foc_initialize(MotorFocState *state);

/**
 * @brief Runs one field-oriented current-control cycle.
 *
 * Measured phase current is transformed, filtered, regulated in D/Q coordinates, and converted to
 * compensated space-vector modulation duties. A zero DC bus uses sign-derived full-scale
 * compensation before space-vector modulation.
 *
 * @param[in,out] state Persistent current-control state to update.
 * @param[in] input Phase current, current reference, rotor angle, and DC-bus voltage.
 * @param[out] output Measured and filtered currents, voltage, duties, and sector.
 */
void motor_foc_step(MotorFocState *state, const MotorFocInput *input, MotorFocOutput *output);

#endif
