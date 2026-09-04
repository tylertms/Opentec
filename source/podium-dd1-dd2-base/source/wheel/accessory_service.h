#ifndef OPENTEC_BASE_WHEEL_ACCESSORY_SERVICE_H
#define OPENTEC_BASE_WHEEL_ACCESSORY_SERVICE_H

#include <stdbool.h>
#include <stdint.h>

#include "motor/calibration.h"
#include "motor/status_service.h"
#include "transfer/command.h"
#include "wheel/accessory.h"

/** @brief Tuning values mirrored to an attached wheel accessory. */
typedef struct {
    uint8_t drift_mode;              /**< Drift-mode byte used before an output override. */
    uint8_t sensitivity;             /**< Signed sensitivity, or 0x7e for automatic sensitivity. */
    uint8_t force_feedback_strength; /**< Overall force-feedback strength. */
    uint8_t force_feedback_scale;    /**< Force-feedback scale mode. */
    uint8_t natural_damper;          /**< Natural damper strength in accessory units. */
    uint16_t natural_friction;       /**< Natural friction strength in accessory units. */
    uint8_t natural_inertia;         /**< Natural inertia strength in accessory units. */
    uint8_t interpolation_filter;    /**< Accessory interpolation-filter level. */
    uint8_t force_effect_intensity;  /**< Force-effect intensity. */
    uint8_t force_effect_strength;   /**< Force-effect strength. */
    uint8_t spring_effect_strength;  /**< Spring-effect strength. */
    uint8_t damper_effect_strength;  /**< Damper-effect strength. */
} WheelAccessorySyncParameters;

/** @brief Official composite accessory transaction states. */
typedef enum {
    WHEEL_ACCESSORY_SYNC_READ_NATURAL_DAMPER = 0,
    WHEEL_ACCESSORY_SYNC_READ_MOTOR_TEMPERATURE,
    WHEEL_ACCESSORY_SYNC_READ_DRIVER_TEMPERATURE,
    WHEEL_ACCESSORY_SYNC_READ_RUNTIME,
    WHEEL_ACCESSORY_SYNC_READ_ACCESSORY_TYPE,
    WHEEL_ACCESSORY_SYNC_READ_CALIBRATION_COMMAND,
    WHEEL_ACCESSORY_SYNC_WRITE_CALIBRATION_COMMAND,
    WHEEL_ACCESSORY_SYNC_RESERVED,
    WHEEL_ACCESSORY_SYNC_PREPARE_NATURAL_FRICTION,
    WHEEL_ACCESSORY_SYNC_WRITE_NATURAL_FRICTION,
    WHEEL_ACCESSORY_SYNC_PREPARE_INTERPOLATION_FILTER,
    WHEEL_ACCESSORY_SYNC_WRITE_INTERPOLATION_FILTER,
    WHEEL_ACCESSORY_SYNC_READ_MOTOR_COMMAND,
    WHEEL_ACCESSORY_SYNC_WRITE_MOTOR_START,
    WHEEL_ACCESSORY_SYNC_PREPARE_NATURAL_DAMPER,
    WHEEL_ACCESSORY_SYNC_WRITE_NATURAL_DAMPER,
    WHEEL_ACCESSORY_SYNC_PREPARE_NATURAL_INERTIA,
    WHEEL_ACCESSORY_SYNC_WRITE_NATURAL_INERTIA,
    WHEEL_ACCESSORY_SYNC_PREPARE_FORCE_FEEDBACK_SCALE,
    WHEEL_ACCESSORY_SYNC_WRITE_FORCE_FEEDBACK_SCALE,
    WHEEL_ACCESSORY_SYNC_RESERVED_20,
    WHEEL_ACCESSORY_SYNC_RESERVED_21,
    WHEEL_ACCESSORY_SYNC_RESERVED_22,
    WHEEL_ACCESSORY_SYNC_ROUTE_STATUS,
    WHEEL_ACCESSORY_SYNC_WRITE_STATUS,
    WHEEL_ACCESSORY_SYNC_READ_STATUS,
    WHEEL_ACCESSORY_SYNC_APPLY_STATUS,
    WHEEL_ACCESSORY_SYNC_RESERVED_27,
    WHEEL_ACCESSORY_SYNC_ROUTE_HANDSHAKE,
    WHEEL_ACCESSORY_SYNC_WRITE_HANDSHAKE,
    WHEEL_ACCESSORY_SYNC_WAIT_CYCLE,
    WHEEL_ACCESSORY_SYNC_PREPARE_SENSITIVITY,
    WHEEL_ACCESSORY_SYNC_WRITE_SENSITIVITY,
    WHEEL_ACCESSORY_SYNC_PREPARE_FORCE_FEEDBACK_STRENGTH,
    WHEEL_ACCESSORY_SYNC_WRITE_FORCE_FEEDBACK_STRENGTH,
    WHEEL_ACCESSORY_SYNC_PREPARE_FORCE_EFFECT_INTENSITY,
    WHEEL_ACCESSORY_SYNC_WRITE_FORCE_EFFECT_INTENSITY,
    WHEEL_ACCESSORY_SYNC_PREPARE_FORCE_EFFECT_STRENGTH,
    WHEEL_ACCESSORY_SYNC_WRITE_FORCE_EFFECT_STRENGTH,
    WHEEL_ACCESSORY_SYNC_PREPARE_SPRING_EFFECT_STRENGTH,
    WHEEL_ACCESSORY_SYNC_WRITE_SPRING_EFFECT_STRENGTH,
    WHEEL_ACCESSORY_SYNC_PREPARE_DAMPER_EFFECT_STRENGTH,
    WHEEL_ACCESSORY_SYNC_WRITE_DAMPER_EFFECT_STRENGTH,
} WheelAccessorySyncState;

/**
 * @brief Attached wheel accessory composite transaction state.
 *
 * One command-transport request is active at a time. The state machine retains successful
 * readbacks, retries rejected requests at the same phase, and advances through the official
 * identity, calibration, effect, status, handshake, tuning, telemetry, and override phases. Tuning
 * preparation states queue changed parameter writes in the same pass as the official fall-through
 * branches, while the wait-cycle state remains deadline-gated.
 */
typedef struct {
    WheelAccessory accessory;    /**< Last accepted accessory identity and protocol state. */
    uint8_t version_bytes[4];    /**< Four bytes retained for the little-endian version value. */
    uint8_t status_byte;         /**< Signed status byte retained from the identity probe. */
    uint8_t accessory_type_byte; /**< Type byte retained from an extended accessory read. */
    bool version_stage;          /**< True when the next probe request reads the version. */
    bool accessory_type_stage;   /**< True while the extended type request remains pending. */
    bool request_pending;        /**< True while the transport owns an active request. */
    uint8_t transfer;            /**< Internal transfer kind retained until completion. */

    uint8_t desired_parameters[15];     /**< Desired compact parameter values. */
    uint8_t mirrored_parameters[15];    /**< Last successful compact parameter readback. */
    uint16_t dirty_parameters;          /**< Configurable values awaiting synchronization. */
    uint8_t sync_index;                 /**< Compact parameter selected for a write. */
    uint8_t sync_write_data[2];         /**< Stable payload snapshot for the active tuning write. */
    bool sync_request;                  /**< True while a parameter write is active. */
    bool sync_initialized;              /**< True after a supported probe starts composite sync. */
    WheelAccessorySyncState sync_state; /**< Current official composite state. */
    uint32_t sync_deadline_ms;          /**< Deadline used by the official wait-cycle state. */
    uint8_t natural_damper_response;    /**< Readback buffer for the initial damper request. */

    bool probe_requested;     /**< True while identity status/version probing is required. */
    bool status_read_pending; /**< True when an explicit accessory status read is due. */
    bool status_read_request; /**< True while the synchronized status read is active. */
    bool status_synchronized; /**< True after the status command has been accepted once. */
    bool handshake_requested; /**< True when the protocol-3 handshake token is queued. */
    bool output_inhibited;    /**< Latest decoded accessory force-output inhibition. */
    uint8_t status_response;  /**< Raw accessory status response byte. */

    uint8_t motor_temperature_response[2];  /**< Raw little-endian wheel-motor temperature. */
    int16_t motor_temperature_c;            /**< Latest signed wheel-motor temperature. */
    bool motor_temperature_enabled;         /**< True after a supported probe. */
    bool motor_temperature_request;         /**< True while a wheel-motor read is active. */
    bool motor_temperature_valid;           /**< True after a non-sentinel motor value. */
    uint8_t driver_temperature_response[2]; /**< Raw little-endian driver temperature. */
    int16_t driver_temperature_c;           /**< Latest signed driver temperature. */
    bool driver_temperature_valid;          /**< True after a non-sentinel driver value. */
    uint8_t runtime_response[4];            /**< Raw little-endian runtime counter. */
    uint32_t runtime_seconds;               /**< Latest valid accessory runtime. */
    bool runtime_valid;                     /**< True after a non-sentinel runtime value. */

    uint32_t wheel_travel_limit;   /**< Travel used for automatic sensitivity. */
    uint8_t drift_mode;             /**< Current drift-mode byte, including override state. */
    uint8_t saved_drift_mode;       /**< Drift-mode byte restored when override is disabled. */
    uint8_t requested_sensitivity; /**< Profile sensitivity before automatic encoding. */
    uint8_t wheel_mode;            /**< Current wheel mode used for calibration gates. */
    uint32_t position_modulus; /**< Sticky steering modulus, initialized to the official default. */
    uint8_t mirrored_model;    /**< Model retained by the official accessory parameter mirror. */

    uint8_t calibration_data[2];  /**< Calibration command or response bytes. */
    uint8_t calibration_requests; /**< Pending calibration and erase request bits. */
    MotorCalibrationOperation calibration_operation; /**< Active calibration operation. */
    MotorCalibrationEvent calibration_event;         /**< Pending calibration lifecycle event. */
    bool calibration_command_sent; /**< True after a calibration command write succeeds. */

    uint8_t motor_command_data[2]; /**< Little-endian motor position-sensor command or response. */
    bool motor_start_pending;      /**< True when a position-sensor start is queued. */
    bool motor_command_sent;       /**< True after the position-sensor command is accepted. */
    MotorStatusEvent motor_event;  /**< Pending position-sensor lifecycle event. */

    bool output_override_requested; /**< True while the prioritized full-damper write is requested. */
    bool output_override_active;    /**< True after the prioritized full-scale write succeeds. */
    uint8_t saved_natural_damper;   /**< Damper value exposed to composite sync on disable. */
    bool remote_effects_enabled;    /**< Current force-feedback status bit zero. */
} WheelAccessoryService;

/** @brief Initializes attached wheel accessory composite polling. */
void wheel_accessory_service_init(WheelAccessoryService *service);

/** @brief Advances the composite service using time zero for compatibility callers. */
void wheel_accessory_service_run(WheelAccessoryService *service, CommandTransport *transport);

/** @brief Advances one composite transaction step at the supplied monotonic time. */
void wheel_accessory_service_run_at(WheelAccessoryService *service, CommandTransport *transport,
                                    uint32_t now_ms);

/** @brief Updates desired accessory tuning and schedules changed values. */
void wheel_accessory_service_configure(WheelAccessoryService *service,
                                       const WheelAccessorySyncParameters *parameters);

/** @brief Supplies the travel used to resolve the automatic sensitivity sentinel. */
void wheel_accessory_service_set_wheel_travel(WheelAccessoryService *service,
                                              uint32_t wheel_travel_limit);

/** @brief Supplies the current wheel mode used to gate calibration requests. */
void wheel_accessory_service_set_wheel_mode(WheelAccessoryService *service, uint8_t wheel_mode);

/** @brief Requests one accessory calibration or erase operation. */
void wheel_accessory_service_request_calibration(WheelAccessoryService *service,
                                                 MotorCalibrationOperation operation);

/** @brief Requests the accessory position-sensor start command. */
void wheel_accessory_service_request_motor_start(WheelAccessoryService *service);

/** @brief Takes the oldest pending accessory position-sensor lifecycle event. */
MotorStatusEvent wheel_accessory_service_take_motor_event(WheelAccessoryService *service);

/** @brief Requests the official accessory handshake write. */
void wheel_accessory_service_request_handshake(WheelAccessoryService *service);

/**
 * @brief Requests or releases the full-damper accessory output override.
 *
 * Requests are accepted only for standard and extended accessory states. Enabling preserves the
 * configured drift and damper values, selects local drift mode 0xfb and tuning damper value 0x64,
 * clears force-feedback status bit zero, and schedules the prioritized wire write of damper byte
 * 0xff. Disabling restores the preserved values and clears status bit zero again; the normal
 * composite state machine then writes the restored damper value.
 *
 * @param[in,out] service Attached wheel accessory service.
 * @param[in] enabled True to apply the override, or false to restore the saved tuning.
 */
void wheel_accessory_service_set_output_override(WheelAccessoryService *service, bool enabled);

/** @brief Reports whether calibration work or an active calibration exchange remains. */
bool wheel_accessory_service_calibration_pending(const WheelAccessoryService *service);

/** @brief Reports whether an accepted calibration exchange is active. */
bool wheel_accessory_service_calibration_active(const WheelAccessoryService *service);

/** @brief Reports whether calibration currently owns the command transport. */
bool wheel_accessory_service_calibration_owns_transport(const WheelAccessoryService *service);

/** @brief Takes one pending calibration lifecycle event. */
MotorCalibrationEvent
wheel_accessory_service_take_calibration_event(WheelAccessoryService *service);

/** @brief Reports whether the output override is active. */
bool wheel_accessory_service_output_override_active(const WheelAccessoryService *service);

/**
 * @brief Returns the sticky steering-position modulus.
 *
 * The default is 0x5d2b. Only an accepted position-capable probe changes the value, and the
 * value remains available while the accessory is in any other protocol state.
 *
 * @param[in] service Attached wheel accessory service.
 * @return Sticky steering-position modulus, or zero when service is null.
 */
uint32_t wheel_accessory_service_position_modulus(const WheelAccessoryService *service);

/** @brief Reports whether the latest accessory status inhibits force output. */
bool wheel_accessory_service_output_inhibited(const WheelAccessoryService *service);

/** @brief Returns the latest valid signed wheel-motor temperature. */
bool wheel_accessory_service_motor_temperature(const WheelAccessoryService *service,
                                               int16_t *temperature_c);

/** @brief Returns the latest valid signed accessory driver temperature. */
bool wheel_accessory_service_driver_temperature(const WheelAccessoryService *service,
                                                int16_t *temperature_c);

/** @brief Returns the latest valid accessory runtime counter. */
bool wheel_accessory_service_runtime(const WheelAccessoryService *service,
                                     uint32_t *runtime_seconds);

/** @brief Returns the latest attached wheel accessory identity. */
const WheelAccessory *wheel_accessory_service_identity(const WheelAccessoryService *service);

/**
 * @brief Reports the force-feedback status bit cleared by an accessory output override.
 *
 * The official override clears status bit zero for both enable and restore operations.
 *
 * @param[in] service Attached wheel accessory service.
 * @return True while the status bit is set, or false for a null service.
 */
bool wheel_accessory_service_remote_effects_enabled(const WheelAccessoryService *service);

/**
 * @brief Returns the auxiliary transfer code used by native wheel input reports.
 *
 * Selects the low six bits of the retained auxiliary version only while the auxiliary state is
 * standard or extended. Disconnected and legacy auxiliary states map to zero.
 *
 * @param[in] service Attached wheel accessory service.
 * @return The six-bit auxiliary transfer code, or zero otherwise.
 */
uint8_t wheel_accessory_service_input_transfer_code(const WheelAccessoryService *service);

#endif
