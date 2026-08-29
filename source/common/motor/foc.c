#include "common/motor/foc.h"

#include <mlib.h>

volatile uint16_t gu16CntMmdvsq;

/**
 * @brief Initializes both current controllers and current filters.
 * @param state Field-oriented control state to initialize.
 */
void motor_foc_initialize(MotorFocState *state) {
    state->d_controller.a32PGain = 0x9999;
    state->d_controller.a32IGain = 0x147;
    state->d_controller.f16UpperLim = 0x1999;
    state->d_controller.f16LowerLim = (frac16_t)0xe667;
    GFLIB_CtrlPIpAWInit_F16(0, &state->d_controller);

    state->q_controller.a32PGain = 0x9999;
    state->q_controller.a32IGain = 0x147;
    state->q_controller.f16UpperLim = 0x1999;
    state->q_controller.f16LowerLim = (frac16_t)0xe667;
    GFLIB_CtrlPIpAWInit_F16(0, &state->q_controller);

    state->q_current_filter.sFltCoeff.f32B0 = 0x05bcffd5;
    state->q_current_filter.sFltCoeff.f32B1 = 0x05bcffd5;
    state->q_current_filter.sFltCoeff.f32A1 = 0x34860055;
    GDFLIB_FilterIIR1Init_F16(&state->q_current_filter);

    state->d_current_filter.sFltCoeff.f32B0 = 0x05bcffd5;
    state->d_current_filter.sFltCoeff.f32B1 = 0x05bcffd5;
    state->d_current_filter.sFltCoeff.f32A1 = 0x34860055;
    GDFLIB_FilterIIR1Init_F16(&state->d_current_filter);

    state->stop_d_integrator = 0;
    state->stop_q_integrator = 0;
}

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
    output->measured_current = rotating_current;

    output->filtered_current.f16Q =
        GDFLIB_FilterIIR1_F16(rotating_current.f16Q, &state->q_current_filter);
    output->filtered_current.f16D =
        GDFLIB_FilterIIR1_F16(rotating_current.f16D, &state->d_current_filter);

    current_error.f16D =
        MLIB_SubSat_F16(input->current_reference.f16D, output->filtered_current.f16D);
    current_error.f16Q =
        MLIB_SubSat_F16(input->current_reference.f16Q, output->filtered_current.f16Q);

    state->d_controller.f16UpperLim = input->dc_bus_voltage;
    state->d_controller.f16LowerLim = MLIB_NegSat_F16(input->dc_bus_voltage);
    output->voltage.f16D =
        GFLIB_CtrlPIpAW_F16(current_error.f16D, &state->stop_d_integrator, &state->d_controller);

    state->q_controller.f16UpperLim =
        GFLIB_Sqrt_F16(MLIB_SubSat_F16(MLIB_Mul_F16(input->dc_bus_voltage, input->dc_bus_voltage),
                                       MLIB_Mul_F16(output->voltage.f16D, output->voltage.f16D)));
    state->q_controller.f16LowerLim = MLIB_NegSat_F16(state->q_controller.f16UpperLim);
    output->voltage.f16Q =
        GFLIB_CtrlPIpAW_F16(current_error.f16Q, &state->stop_q_integrator, &state->q_controller);

    GMCLIB_ParkInv_F16(&output->voltage, &input->rotor_sin_cos, &stationary_voltage);
    GMCLIB_ElimDcBusRipFOC_F16(input->dc_bus_voltage, &stationary_voltage, &compensated_voltage);
    output->sector = GMCLIB_SvmStd_F16(&compensated_voltage, &output->duty);
}
