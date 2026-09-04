#ifndef OPENTEC_BASE_COOLING_CONTROLLER_H
#define OPENTEC_BASE_COOLING_CONTROLLER_H

#include <stdbool.h>
#include <stdint.h>

/**
 * @brief Thermal phases used to select fan duty and force availability.
 *
 * The controller moves through these phases with temperature hysteresis and, when a managed motor
 * is present, a timed full-output window before limiting force.
 */
typedef enum {
    COOLING_PHASE_INITIALIZE,     /**< Run both fans at full duty until temperature rises above the
                                     startup threshold. */
    COOLING_PHASE_IDLE,           /**< Keep fans off while temperature remains in the idle range. */
    COOLING_PHASE_LOW,            /**< Apply the low-temperature fan duty. */
    COOLING_PHASE_MEDIUM,         /**< Apply the medium-temperature fan duty. */
    COOLING_PHASE_HIGH,           /**< Apply the high-temperature fan duty. */
    COOLING_PHASE_NEAR_MAXIMUM,   /**< Apply the near-maximum-temperature fan duty. */
    COOLING_PHASE_FULL,           /**< Run fans at full duty without an active thermal limit. */
    COOLING_PHASE_STANDARD_LIMIT, /**< Run fans at full duty with standard force limiting. */
    COOLING_PHASE_START_MANAGED_WINDOW, /**< Start managed-motor timing on the next update. */
    COOLING_PHASE_MANAGED_WINDOW,       /**< Run the managed-motor full-output timing window. */
    COOLING_PHASE_MANAGED_LIMIT,        /**< Run fans at full duty with managed force limiting. */
} CoolingPhase;

/**
 * @brief Stateful fan and force-availability thermal controller.
 *
 * Stores the current thermal phase, configurable managed-motor offsets, timing deadlines, selected
 * fan duties, force scale, fan topology, and service-override state.
 */
typedef struct {
    CoolingPhase phase;           /**< Current thermal phase. */
    int8_t low_threshold_offset;  /**< Managed low-threshold adjustment in degrees Celsius. */
    int8_t high_threshold_offset; /**< Managed high-threshold adjustment in degrees Celsius. */
    int32_t primary_delay_ms;     /**< Managed primary-window delay adjustment in milliseconds. */
    int32_t secondary_delay_ms;   /**< Managed secondary-window delay adjustment in milliseconds. */
    uint32_t primary_deadline_ms; /**< Deadline for the managed primary timing window. */
    uint32_t secondary_deadline_ms; /**< Deadline for the managed secondary timing window. */
    uint8_t primary_duty_percent;   /**< Selected primary fan duty percentage. */
    uint8_t secondary_duty_percent; /**< Selected secondary fan duty percentage. */
    uint8_t force_scale_percent;    /**< Available force-feedback scale percentage. */
    bool dual_fan_mode;             /**< True when the dual-fan duty map is selected. */
    bool
        automatic_control_suspended; /**< True while service override suspends automatic control. */
} CoolingController;

/**
 * @brief Initializes the thermal fan and force controller.
 *
 * Starts in the initialization phase with default managed-motor offsets, startup fan duty, full
 * force scale, and the selected standard or dual-fan output map. The phase remains initialization
 * until the first automatic update publishes the first thermal state.
 *
 * @param[out] controller Thermal controller state to initialize.
 * @param[in] dual_fan_mode True to select the alternate two-output fan duty map.
 */
void cooling_controller_init(CoolingController *controller, bool dual_fan_mode);

/**
 * @brief Sets the managed-motor low-temperature threshold offset.
 *
 * Stores values from -5 through 5 degrees Celsius and leaves the previous offset unchanged when
 * the request is outside that range.
 *
 * @param[in,out] controller Thermal controller state to update.
 * @param[in] offset Signed threshold adjustment in degrees Celsius.
 */
void cooling_controller_set_low_threshold_offset(CoolingController *controller, int8_t offset);

/**
 * @brief Sets the managed-motor high-temperature threshold offset.
 *
 * Stores values from -5 through 5 degrees Celsius and leaves the previous offset unchanged when
 * the request is outside that range.
 *
 * @param[in,out] controller Thermal controller state to update.
 * @param[in] offset Signed threshold adjustment in degrees Celsius.
 */
void cooling_controller_set_high_threshold_offset(CoolingController *controller, int8_t offset);

/**
 * @brief Sets the managed primary-window delay offset.
 *
 * Stores values from -120 through 120 seconds after converting the accepted value to milliseconds;
 * an out-of-range request leaves the previous delay unchanged.
 *
 * @param[in,out] controller Thermal controller state to update.
 * @param[in] seconds Signed primary-window delay adjustment in seconds.
 */
void cooling_controller_set_primary_delay_seconds(CoolingController *controller, int8_t seconds);

/**
 * @brief Sets the managed secondary-window delay offset.
 *
 * Stores values from -120 through 120 seconds after converting the accepted value to milliseconds;
 * an out-of-range request leaves the previous delay unchanged.
 *
 * @param[in,out] controller Thermal controller state to update.
 * @param[in] seconds Signed secondary-window delay adjustment in seconds.
 */
void cooling_controller_set_secondary_delay_seconds(CoolingController *controller, int8_t seconds);

/**
 * @brief Sets automatic-control suspension from a service request byte.
 *
 * Sets suspension only when request equals UINT8_MAX and clears it for every other request value.
 *
 * @param[in,out] controller Thermal controller state to update.
 * @param[in] request Service suspension request byte.
 */
void cooling_controller_set_suspend_request(CoolingController *controller, uint8_t request);

/**
 * @brief Applies a service fan and force-output override.
 *
 * A UINT8_MAX request suspends automatic control and clamps all supplied percentages to 100; any
 * other request resumes automatic control without changing the retained outputs.
 *
 * @param[in,out] controller Thermal controller state and output percentages.
 * @param[in] request Service suspension request byte.
 * @param[in] primary_duty_percent Requested primary fan duty percentage.
 * @param[in] secondary_duty_percent Requested secondary fan duty percentage.
 * @param[in] force_scale_percent Requested force-availability scale percentage.
 */
void cooling_controller_apply_service_override(CoolingController *controller, uint8_t request,
                                               uint8_t primary_duty_percent,
                                               uint8_t secondary_duty_percent,
                                               uint8_t force_scale_percent);

/**
 * @brief Advances automatic fan cooling and force derating.
 *
 * Updates the thermal phase, fan duties, managed timing windows, and force scale unless service
 * suspension is active.
 *
 * @param[in,out] controller Thermal controller state and resulting outputs.
 * @param[in] motor_temperature_c Current motor temperature in degrees Celsius.
 * @param[in] managed_motor_present True after a managed motor controller is identified.
 * @param[in] output_inhibited True when force output is inhibited.
 * @param[in] now_ms Current monotonic time in milliseconds.
 */
void cooling_controller_update(CoolingController *controller, float motor_temperature_c,
                               bool managed_motor_present, bool output_inhibited, uint32_t now_ms);

#endif
