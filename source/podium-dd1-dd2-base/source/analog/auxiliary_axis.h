#ifndef OPENTEC_BASE_ANALOG_AUXILIARY_AXIS_H
#define OPENTEC_BASE_ANALOG_AUXILIARY_AXIS_H

#include <stdbool.h>
#include <stdint.h>

#include "analog/axis.h"

/**
 * @brief Selects how auxiliary-axis endpoints are calibrated.
 *
 * Both modes track startup endpoints while limits are uninitialized. Manual mode accepts explicit
 * endpoint requests; automatic mode additionally settles the maximum after learning the minimum.
 */
typedef enum {
    AUXILIARY_AXIS_MANUAL_CALIBRATION,    /**< Track uninitialized startup endpoints and accept
                                             explicit adjustment requests. */
    AUXILIARY_AXIS_AUTOMATIC_CALIBRATION, /**< Track the minimum and settle the maximum
                                             automatically. */
} AuxiliaryAxisCalibrationMode;

/**
 * @brief Selects which auxiliary-axis endpoint a manual request captures.
 *
 * The selected endpoint is captured from the next active, filtered auxiliary-axis sample; a
 * maximum capture can be rejected when the sample has insufficient travel above the minimum.
 */
typedef enum {
    AUXILIARY_AXIS_ADJUST_MINIMUM, /**< Capture the released or minimum endpoint. */
    AUXILIARY_AXIS_ADJUST_MAXIMUM, /**< Capture the engaged or maximum endpoint. */
} AuxiliaryAxisAdjustment;

/**
 * @brief Persisted auxiliary-axis endpoint settings.
 *
 * The endpoints define the normalized input range and reset_on_start controls whether startup
 * calibration is requested on the next initialization.
 */
typedef struct {
    uint16_t minimum;    /**< Normalized sample mapped to output zero. */
    uint16_t maximum;    /**< Normalized sample mapped to output UINT8_MAX. */
    bool reset_on_start; /**< True to restart endpoint learning during initialization. */
} AuxiliaryAxisSettings;

/**
 * @brief Stateful auxiliary-axis filter, calibrator, and source detector.
 *
 * The structure is updated by auxiliary-axis service calls and contains both persisted endpoint
 * settings and transient state used while learning or settling those endpoints.
 */
typedef struct {
    AnalogAxisFilter filter;        /**< Moving-average state for the electrical input signal. */
    AuxiliaryAxisSettings settings; /**< Current and startup endpoint settings. */
    uint16_t minimum_candidate; /**< Smallest normalized sample observed during minimum learning. */
    uint16_t settle_threshold;  /**< Normalized sample threshold that starts automatic maximum
                                   settling. */
    uint32_t
        settle_deadline_ms; /**< Monotonic deadline for the active automatic settling interval. */
    uint8_t minimum_sample_count; /**< Number of samples collected during minimum learning. */
    uint8_t value;                /**< Most recently scaled auxiliary-axis output byte. */
    bool active; /**< True when the most recent electrical sample indicates a local input. */
    bool limits_uninitialized;  /**< True while startup endpoint calibration is incomplete. */
    bool learning_minimum;      /**< True while the minimum endpoint is being learned. */
    bool maximum_capture_armed; /**< True when automatic maximum capture is armed for a qualifying
                                   sample. */
    bool settling; /**< True while an automatic maximum endpoint hold is in progress. */
    bool minimum_adjustment_pending; /**< True when a manual minimum capture awaits an active
                                        sample. */
    bool maximum_adjustment_pending; /**< True when a manual maximum capture awaits an active
                                        sample. */
    bool settings_changed;           /**< True when settings must be returned by
                                        auxiliary_axis_take_settings(). */
} AuxiliaryAxis;

/**
 * @brief Result of one auxiliary-axis update.
 *
 * active distinguishes a locally driven auxiliary input from a disconnected input; value is the
 * calibrated byte for an active input and zero otherwise.
 */
typedef struct {
    uint8_t value; /**< Calibrated auxiliary-axis value. */
    bool active;   /**< True when a local auxiliary-axis input is present. */
} AuxiliaryAxisReading;

/**
 * @brief Loads the default auxiliary-axis endpoint settings.
 *
 * Installs the endpoint sentinels used for startup learning and requests a reset during the next
 * auxiliary_axis_init() call.
 *
 * @param[out] settings Destination settings record.
 */
void auxiliary_axis_settings_defaults(AuxiliaryAxisSettings *settings);

/**
 * @brief Initializes auxiliary-axis filtering, calibration, and source state.
 *
 * Copies retained settings into a fresh state and starts endpoint learning when reset_on_start is
 * true or when the retained endpoints are not ordered.
 *
 * @param[out] axis Auxiliary-axis state to initialize.
 * @param[in] settings Retained endpoint settings.
 */
void auxiliary_axis_init(AuxiliaryAxis *axis, const AuxiliaryAxisSettings *settings);

/**
 * @brief Restarts auxiliary-axis endpoint discovery.
 *
 * Restores endpoint sentinels and arms both calibration stages while preserving the accumulated
 * input-filter state.
 *
 * @param[in,out] axis Auxiliary-axis state to reset.
 */
void auxiliary_axis_reset(AuxiliaryAxis *axis);

/**
 * @brief Queues one manual auxiliary-axis endpoint capture.
 *
 * Records which endpoint the next active filtered sample must capture and marks settings as
 * changed for persistence. A maximum capture is accepted only when the sample is more than 200
 * counts above the current minimum.
 *
 * @param[in,out] axis Auxiliary-axis calibration state to update.
 * @param[in] adjustment Endpoint to capture.
 */
void auxiliary_axis_request_adjustment(AuxiliaryAxis *axis, AuxiliaryAxisAdjustment adjustment);

/**
 * @brief Samples, calibrates, and scales the local auxiliary-axis input.
 *
 * Detects source presence before filtering, advances the selected calibration mode, and returns the
 * calibrated byte without claiming a disconnected input.
 *
 * @param[in,out] axis Auxiliary-axis state to update.
 * @param[in] adc_sample Raw 12-bit ADC sample.
 * @param[in] mode Endpoint calibration mode.
 * @param[in] now_ms Current monotonic time in milliseconds.
 * @return Source-presence flag and calibrated auxiliary-axis byte.
 */
AuxiliaryAxisReading auxiliary_axis_update(AuxiliaryAxis *axis, uint16_t adc_sample,
                                           AuxiliaryAxisCalibrationMode mode, uint32_t now_ms);

/**
 * @brief Takes the newest persistable auxiliary-axis settings.
 *
 * Copies the current endpoints when the change latch is set, records whether automatic mode must
 * restart learning, and clears the latch only after a snapshot is returned.
 *
 * @param[in,out] axis Auxiliary-axis state and settings-change latch.
 * @param[in] mode Endpoint calibration mode used for the next startup.
 * @param[out] settings Destination for the latest settings snapshot.
 * @return True when a changed snapshot was returned; otherwise false.
 */
bool auxiliary_axis_take_settings(AuxiliaryAxis *axis, AuxiliaryAxisCalibrationMode mode,
                                  AuxiliaryAxisSettings *settings);

#endif
