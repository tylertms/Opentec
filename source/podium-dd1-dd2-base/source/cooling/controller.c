#include "cooling/controller.h"

#include <stdbool.h>
#include <stdint.h>

enum {
    FAN_STARTUP_DUTY_PERCENT = 25,
    COOLING_DEFAULT_LOW_THRESHOLD_OFFSET = 5,
    COOLING_DEFAULT_HIGH_THRESHOLD_OFFSET = 4,
    COOLING_DEFAULT_PRIMARY_DELAY_MS = -30000,
    COOLING_DEFAULT_SECONDARY_DELAY_MS = -120000,
    MANAGED_PRIMARY_WINDOW_MS = 210000,
    MANAGED_SECONDARY_WINDOW_MS = 300000,
};

/**
 * @brief Selects fan duties for the active board configuration.
 *
 * Applies the standard single-output duty or the primary and secondary duties used by the dual-fan
 * configuration.
 *
 * @param[in,out] controller Thermal controller state and selected output duties.
 * @param[in] standard Duty for the standard configuration.
 * @param[in] dual_primary Primary duty for the dual-fan configuration.
 * @param[in] dual_secondary Secondary duty for the dual-fan configuration.
 */
static void set_fan_duty(CoolingController *controller, uint8_t standard, uint8_t dual_primary,
                         uint8_t dual_secondary) {
    controller->primary_duty_percent = controller->dual_fan_mode ? dual_primary : standard;
    controller->secondary_duty_percent = controller->dual_fan_mode ? dual_secondary : 0;
}

/**
 * @brief Advances one temperature-hysteresis transition.
 *
 * Selects the hotter phase above the upper threshold, the cooler phase below the lower threshold,
 * and otherwise preserves the current phase.
 *
 * @param[in,out] controller Thermal controller state.
 * @param[in] temperature Current motor temperature in degrees Celsius.
 * @param[in] upper_threshold Temperature that selects the hotter phase.
 * @param[in] lower_threshold Temperature that selects the cooler phase.
 * @param[in] next Hotter phase.
 * @param[in] previous Cooler phase.
 */
static void update_hysteresis(CoolingController *controller, float temperature,
                              float upper_threshold, float lower_threshold, CoolingPhase next,
                              CoolingPhase previous) {
    if (temperature > upper_threshold) {
        controller->phase = next;
    } else if (temperature < lower_threshold) {
        controller->phase = previous;
    }
}

/**
 * @brief Services the managed-motor full-output window.
 *
 * Runs both fans at full duty, enters the managed limit at the adjusted high-temperature or time
 * boundary, and returns to the full-output phase below the adjusted low-temperature boundary.
 *
 * @param[in,out] controller Thermal controller state and output duties.
 * @param[in] temperature Current motor temperature in degrees Celsius.
 * @param[in] now_ms Current monotonic time in milliseconds.
 */
static void update_managed_window(CoolingController *controller, float temperature,
                                  uint32_t now_ms) {
    set_fan_duty(controller, 100, 100, 100);
    if (temperature > (float)(controller->high_threshold_offset + 135) ||
        now_ms > controller->secondary_deadline_ms + controller->primary_delay_ms) {
        controller->phase = COOLING_PHASE_MANAGED_LIMIT;
    } else if (temperature < (float)(controller->low_threshold_offset + 125)) {
        controller->phase = COOLING_PHASE_FULL;
    }
}

/**
 * @brief Advances the fan-output thermal profile.
 *
 * Applies the standard or dual-fan duty map, temperature hysteresis, and managed-motor timing for
 * the current phase.
 *
 * @param[in,out] controller Thermal controller state and output duties.
 * @param[in] temperature Current motor temperature in degrees Celsius.
 * @param[in] managed_motor_present True after a managed motor controller is identified.
 * @param[in] now_ms Current monotonic time in milliseconds.
 */
static void update_fan_profile(CoolingController *controller, float temperature,
                               bool managed_motor_present, uint32_t now_ms) {
    switch (controller->phase) {
    case COOLING_PHASE_INITIALIZE:
        set_fan_duty(controller, 100, 100, 100);
        if (temperature > 6.0f) {
            controller->phase = COOLING_PHASE_IDLE;
        }
        break;
    case COOLING_PHASE_IDLE:
        set_fan_duty(controller, 0, 0, 0);
        if (temperature > 35.0f) {
            controller->phase = COOLING_PHASE_LOW;
        } else if (temperature < 5.0f) {
            controller->phase = COOLING_PHASE_INITIALIZE;
        }
        break;
    case COOLING_PHASE_LOW:
        set_fan_duty(controller, 20, 5, 0);
        update_hysteresis(controller, temperature, 45.0f, 30.0f, COOLING_PHASE_MEDIUM,
                          COOLING_PHASE_IDLE);
        break;
    case COOLING_PHASE_MEDIUM:
        set_fan_duty(controller, 40, 5, 4);
        update_hysteresis(controller, temperature, 60.0f, 40.0f, COOLING_PHASE_HIGH,
                          COOLING_PHASE_LOW);
        break;
    case COOLING_PHASE_HIGH:
        set_fan_duty(controller, 50, 5, 5);
        update_hysteresis(controller, temperature, 75.0f, 55.0f, COOLING_PHASE_NEAR_MAXIMUM,
                          COOLING_PHASE_MEDIUM);
        break;
    case COOLING_PHASE_NEAR_MAXIMUM:
        set_fan_duty(controller, 70, 6, 6);
        update_hysteresis(controller, temperature, 95.0f, 70.0f, COOLING_PHASE_FULL,
                          COOLING_PHASE_HIGH);
        break;
    case COOLING_PHASE_FULL:
        set_fan_duty(controller, 100, 100, 100);
        if (managed_motor_present) {
            if (temperature > (float)(controller->low_threshold_offset + 125)) {
                controller->phase = COOLING_PHASE_START_MANAGED_WINDOW;
            } else if (temperature < 90.0f) {
                controller->phase = COOLING_PHASE_NEAR_MAXIMUM;
            }
        } else if (temperature > 120.0f) {
            controller->phase = COOLING_PHASE_STANDARD_LIMIT;
        } else if (temperature < 90.0f) {
            controller->phase = COOLING_PHASE_NEAR_MAXIMUM;
        }
        break;
    case COOLING_PHASE_STANDARD_LIMIT:
        set_fan_duty(controller, 100, 100, 100);
        if (temperature < 115.0f) {
            controller->phase = COOLING_PHASE_FULL;
        }
        break;
    case COOLING_PHASE_START_MANAGED_WINDOW:
        controller->secondary_deadline_ms = now_ms + MANAGED_SECONDARY_WINDOW_MS;
        controller->primary_deadline_ms = now_ms + MANAGED_PRIMARY_WINDOW_MS;
        controller->phase = COOLING_PHASE_MANAGED_WINDOW;
        update_managed_window(controller, temperature, now_ms);
        break;
    case COOLING_PHASE_MANAGED_WINDOW:
        update_managed_window(controller, temperature, now_ms);
        break;
    case COOLING_PHASE_MANAGED_LIMIT:
        set_fan_duty(controller, 100, 100, 100);
        if (temperature < (float)(controller->low_threshold_offset + 125)) {
            controller->phase = COOLING_PHASE_FULL;
        }
        break;
    default:
        controller->phase = COOLING_PHASE_IDLE;
        break;
    }
}

/**
 * @brief Updates the thermally available force-output scale.
 *
 * Disables output when inhibited or limited, preserves full output below the managed threshold,
 * and applies the managed temperature ramp above 125 degrees Celsius.
 *
 * @param[in,out] controller Thermal controller state and resulting force scale.
 * @param[in] temperature Current motor temperature in degrees Celsius.
 * @param[in] managed_motor_present True after a managed motor controller is identified.
 * @param[in] output_inhibited True when force output is inhibited.
 */
static void update_force_scale(CoolingController *controller, float temperature,
                               bool managed_motor_present, bool output_inhibited) {
    if (output_inhibited) {
        controller->force_scale_percent = 0;
        return;
    }
    if (!managed_motor_present) {
        if (controller->phase <= COOLING_PHASE_FULL) {
            controller->force_scale_percent = 100;
        } else if (controller->phase == COOLING_PHASE_STANDARD_LIMIT) {
            controller->force_scale_percent = 0;
        }
        return;
    }

    switch (controller->phase) {
    case COOLING_PHASE_INITIALIZE:
    case COOLING_PHASE_IDLE:
    case COOLING_PHASE_LOW:
    case COOLING_PHASE_MEDIUM:
    case COOLING_PHASE_HIGH:
    case COOLING_PHASE_NEAR_MAXIMUM:
    case COOLING_PHASE_FULL:
    case COOLING_PHASE_START_MANAGED_WINDOW:
    case COOLING_PHASE_MANAGED_WINDOW:
        if (temperature > 125.0f) {
            uint8_t next_scale = (uint8_t)((temperature - 125.0f) * -5.0f + 100.0f);
            controller->force_scale_percent = next_scale <= 69 ? 0 : next_scale;
        } else {
            controller->force_scale_percent = 100;
        }
        break;
    case COOLING_PHASE_MANAGED_LIMIT:
        controller->force_scale_percent = 0;
        break;
    default:
        break;
    }
}

/**
 * @brief Initializes the thermal controller with its startup fan and force limits.
 *
 * Starts in the idle phase with the retained threshold and timing defaults, requests 25 percent
 * from both fan outputs, permits full force output, and selects the standard or dual-fan duty map.
 *
 * @param[out] controller Thermal fan and force-derating state.
 * @param[in] dual_fan_mode True for the alternate two-output fan duty map.
 */
void cooling_controller_init(CoolingController *controller, bool dual_fan_mode) {
    *controller = (CoolingController){
        .phase = COOLING_PHASE_IDLE,
        .low_threshold_offset = COOLING_DEFAULT_LOW_THRESHOLD_OFFSET,
        .high_threshold_offset = COOLING_DEFAULT_HIGH_THRESHOLD_OFFSET,
        .primary_delay_ms = COOLING_DEFAULT_PRIMARY_DELAY_MS,
        .secondary_delay_ms = COOLING_DEFAULT_SECONDARY_DELAY_MS,
        .primary_duty_percent = FAN_STARTUP_DUTY_PERCENT,
        .secondary_duty_percent = FAN_STARTUP_DUTY_PERCENT,
        .force_scale_percent = 100,
        .dual_fan_mode = dual_fan_mode,
    };
}

/**
 * @brief Updates the low managed-motor thermal threshold offset when it is in range.
 *
 * Stores adjustments from minus five through five degrees and leaves the previous setting
 * unchanged for values outside that range.
 *
 * @param[in,out] controller Thermal controller state.
 * @param[in] offset Signed threshold adjustment from -5 through 5 degrees.
 */
void cooling_controller_set_low_threshold_offset(CoolingController *controller, int8_t offset) {
    if (offset >= -5 && offset <= 5) {
        controller->low_threshold_offset = offset;
    }
}

/**
 * @brief Updates the high managed-motor thermal threshold offset when it is in range.
 *
 * Stores adjustments from minus five through five degrees and leaves the previous setting
 * unchanged for values outside that range.
 *
 * @param[in,out] controller Thermal controller state.
 * @param[in] offset Signed threshold adjustment from -5 through 5 degrees.
 */
void cooling_controller_set_high_threshold_offset(CoolingController *controller, int8_t offset) {
    if (offset >= -5 && offset <= 5) {
        controller->high_threshold_offset = offset;
    }
}

/**
 * @brief Updates the primary managed-motor time offset when it is in range.
 *
 * Accepts signed second values from minus 120 through 120 and stores the corresponding millisecond
 * delay.
 *
 * @param[in,out] controller Thermal controller state.
 * @param[in] seconds Signed delay adjustment from -120 through 120 seconds.
 */
void cooling_controller_set_primary_delay_seconds(CoolingController *controller, int8_t seconds) {
    if (seconds >= -120 && seconds <= 120) {
        controller->primary_delay_ms = (int32_t)seconds * 1000;
    }
}

/**
 * @brief Updates the secondary managed-motor time offset when it is in range.
 *
 * Accepts signed second values from minus 120 through 120 and stores the corresponding millisecond
 * delay.
 *
 * @param[in,out] controller Thermal controller state.
 * @param[in] seconds Signed delay adjustment from -120 through 120 seconds.
 */
void cooling_controller_set_secondary_delay_seconds(CoolingController *controller, int8_t seconds) {
    if (seconds >= -120 && seconds <= 120) {
        controller->secondary_delay_ms = (int32_t)seconds * 1000;
    }
}

/**
 * @brief Suspends automatic thermal control only for the protocol's 0xFF request value.
 *
 * Enables suspension for 0xFF and clears suspension for every other request value.
 *
 * @param[in,out] controller Thermal controller state.
 * @param[in] request Suspend request byte.
 */
void cooling_controller_set_suspend_request(CoolingController *controller, uint8_t request) {
    controller->automatic_control_suspended = request == UINT8_MAX;
}

/**
 * @brief Applies the service cooling and output override.
 *
 * A request of 0xFF suspends automatic thermal control and replaces both fan duties and the force
 * availability scale. Each percentage is constrained to one hundred. Other request values resume
 * automatic control without replacing its last outputs.
 *
 * @param[in,out] controller Thermal controller state and output percentages.
 * @param[in] request Suspension request byte.
 * @param[in] primary_duty_percent Requested primary fan duty.
 * @param[in] secondary_duty_percent Requested secondary fan duty.
 * @param[in] force_scale_percent Requested available force percentage.
 */
void cooling_controller_apply_service_override(CoolingController *controller, uint8_t request,
                                               uint8_t primary_duty_percent,
                                               uint8_t secondary_duty_percent,
                                               uint8_t force_scale_percent) {
    cooling_controller_set_suspend_request(controller, request);
    if (!controller->automatic_control_suspended) {
        return;
    }
    controller->primary_duty_percent = primary_duty_percent > 100 ? 100 : primary_duty_percent;
    controller->secondary_duty_percent =
        secondary_duty_percent > 100 ? 100 : secondary_duty_percent;
    controller->force_scale_percent = force_scale_percent > 100 ? 100 : force_scale_percent;
}

/**
 * @brief Advances fan output, thermal phase, timed limits, and force-feedback derating.
 *
 * Leaves suspended state unchanged; otherwise updates the fan profile and output-strength scale
 * from the current motor temperature and availability state.
 *
 * @param[in,out] controller Thermal controller state and resulting output percentages.
 * @param[in] motor_temperature_c Current motor temperature in degrees Celsius.
 * @param[in] managed_motor_present True after a motor controller is identified.
 * @param[in] output_inhibited True when force output is inhibited.
 * @param[in] now_ms Current monotonic time in milliseconds.
 */
void cooling_controller_update(CoolingController *controller, float motor_temperature_c,
                               bool managed_motor_present, bool output_inhibited, uint32_t now_ms) {
    if (controller->automatic_control_suspended) {
        return;
    }
    update_fan_profile(controller, motor_temperature_c, managed_motor_present, now_ms);
    update_force_scale(controller, motor_temperature_c, managed_motor_present, output_inhibited);
}
