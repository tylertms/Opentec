#include "common/motor/foc.h"

volatile uint16_t gu16CntMmdvsq;

/**
 * @brief Runs one field-oriented current-control cycle.
 * @param state Persistent current filters, PI controllers, and anti-windup flags.
 * @param input Phase currents, current references, rotor angle, and DC-bus voltage.
 * @param output Filtered currents, commanded voltage, SVM duties, and sector.
 */
void motor_foc_step(MotorFocState *state, const MotorFocInput *input, MotorFocOutput *output) {
    GMCLIB_2COOR_ALBE_T_F16 stationary_current;
    GMCLIB_2COOR_DQ_T_F16 rotating_current;
    GMCLIB_2COOR_DQ_T_F16 current_error;
    GMCLIB_2COOR_ALBE_T_F16 stationary_voltage;
    GMCLIB_2COOR_ALBE_T_F16 compensated_voltage;

    GMCLIB_Clark_F16(&input->phase_current, &stationary_current);
    GMCLIB_Park_F16(&stationary_current, &input->rotor_sin_cos, &rotating_current);

    output->filtered_current.f16Q =
        GDFLIB_FilterIIR1_F16(rotating_current.f16Q, &state->q_current_filter);
    output->filtered_current.f16D =
        GDFLIB_FilterIIR1_F16(rotating_current.f16D, &state->d_current_filter);

    current_error.f16D = MLIB_Sub_F16(input->current_reference.f16D, output->filtered_current.f16D);
    current_error.f16Q = MLIB_Sub_F16(input->current_reference.f16Q, output->filtered_current.f16Q);

    state->d_controller.f16UpperLim = input->dc_bus_voltage;
    state->d_controller.f16LowerLim = MLIB_Neg_F16(input->dc_bus_voltage);
    output->voltage.f16D =
        GFLIB_CtrlPIpAW_F16(current_error.f16D, &state->stop_d_integrator, &state->d_controller);

    state->q_controller.f16UpperLim =
        GFLIB_Sqrt_F16(MLIB_Sub_F16(MLIB_Mul_F16(input->dc_bus_voltage, input->dc_bus_voltage),
                                    MLIB_Mul_F16(output->voltage.f16D, output->voltage.f16D)));
    state->q_controller.f16LowerLim = MLIB_Neg_F16(state->q_controller.f16UpperLim);
    output->voltage.f16Q =
        GFLIB_CtrlPIpAW_F16(current_error.f16Q, &state->stop_q_integrator, &state->q_controller);

    GMCLIB_ParkInv_F16(&output->voltage, &input->rotor_sin_cos, &stationary_voltage);
    GMCLIB_ElimDcBusRipFOC_F16(input->dc_bus_voltage, &stationary_voltage, &compensated_voltage);
    output->sector = GMCLIB_SvmStd_F16(&compensated_voltage, &output->duty);
}
