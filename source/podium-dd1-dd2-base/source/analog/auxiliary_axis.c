#include "analog/auxiliary_axis.h"

#include <stdbool.h>
#include <stdint.h>

#include "analog/axis.h"

/**
 * @brief Auxiliary-axis signal, calibration, and timing constants.
 *
 * These values describe the ADC encoding, endpoint margins, and automatic-learning intervals used
 * by the auxiliary-axis state machine.
 */
enum {
    AUXILIARY_AXIS_SIGNAL_MASK =
        0x0ffe, /**< Bit mask retaining the even-valued inverted 12-bit signal. */
    AUXILIARY_AXIS_PRESENT_SIGNAL_LIMIT =
        0x0fc0, /**< Highest inverted signal considered to indicate a connected input. */
    AUXILIARY_AXIS_SAMPLE_RANGE = 0x1000, /**< One past the maximum normalized 12-bit sample. */
    AUXILIARY_AXIS_DEFAULT_MINIMUM =
        0x0f38, /**< Startup sentinel and minimum-learning candidate. */
    AUXILIARY_AXIS_DEFAULT_MAXIMUM = 0x00c8, /**< Startup sentinel for the maximum endpoint. */
    AUXILIARY_AXIS_FILTER_DEADBAND = 10,     /**< Deadband passed to the analog-axis filter. */
    AUXILIARY_AXIS_ENDPOINT_MARGIN =
        40, /**< Inward margin applied when recording either endpoint. */
    AUXILIARY_AXIS_MINIMUM_TRAVEL =
        200, /**< Minimum usable travel required before accepting a maximum endpoint. */
    AUXILIARY_AXIS_MINIMUM_SAMPLE_COUNT =
        10, /**< Number of active samples used to learn the minimum endpoint. */
    AUXILIARY_AXIS_SETTLE_OFFSET =
        240, /**< Offset above the learned minimum that starts maximum settling. */
    AUXILIARY_AXIS_SETTLE_TIME_MS =
        2000, /**< Duration of the automatic maximum settling interval in milliseconds. */
};

/**
 * @brief Converts one ADC sample to the auxiliary input's normalized signal.
 *
 * Restores the increasing normalized orientation used by endpoint calibration and output scaling.
 * The unused least-significant bit was already cleared by read_signal().
 *
 * @param[in] signal Inverted 12-bit auxiliary input signal.
 * @return Normalized auxiliary sample in the inclusive range 2 through 4096.
 */
static uint16_t normalize_signal(uint16_t signal) {
    return (uint16_t)(AUXILIARY_AXIS_SAMPLE_RANGE - signal);
}

/**
 * @brief Converts one ADC sample to the electrical signal used for detection and filtering.
 *
 * Inverts the converter result and clears its unused least-significant bit.
 *
 * @param[in] adc_sample Raw 12-bit ADC sample.
 * @return Inverted even-valued auxiliary signal.
 */
static uint16_t read_signal(uint16_t adc_sample) {
    return (uint16_t)~adc_sample & AUXILIARY_AXIS_SIGNAL_MASK;
}

/**
 * @brief Reports whether the electrical input indicates a connected local source.
 *
 * Uses the unfiltered inverted signal so source presence changes without waiting for the moving
 * average.
 *
 * @param[in] signal Inverted auxiliary input signal.
 * @return True when a local auxiliary input is present.
 */
static bool signal_present(uint16_t signal) {
    return signal <= AUXILIARY_AXIS_PRESENT_SIGNAL_LIMIT;
}

/**
 * @brief Restores endpoint learning to its startup state.
 *
 * Installs the endpoint sentinels, arms minimum and maximum discovery, cancels settling, and marks
 * the settings snapshot for persistence.
 *
 * @param[in,out] axis Auxiliary input state to reset.
 */
static void reset_limits(AuxiliaryAxis *axis) {
    axis->minimum_candidate = AUXILIARY_AXIS_DEFAULT_MINIMUM;
    axis->settings.minimum = AUXILIARY_AXIS_DEFAULT_MINIMUM;
    axis->settings.maximum = AUXILIARY_AXIS_DEFAULT_MAXIMUM;
    axis->settle_deadline_ms = 0;
    axis->minimum_sample_count = 0;
    axis->limits_uninitialized = true;
    axis->maximum_capture_armed = true;
    axis->learning_minimum = true;
    axis->settling = false;
    axis->minimum_adjustment_pending = false;
    axis->maximum_adjustment_pending = false;
    axis->settings_changed = true;
}

/**
 * @brief Learns the released endpoint from the first ten active samples.
 *
 * Tracks the smallest normalized sample, then adds the 40-count endpoint margin and establishes
 * the threshold used by automatic maximum settling.
 *
 * @param[in,out] axis Auxiliary calibration state to update.
 * @param[in] sample Filtered normalized sample.
 */
static void learn_minimum(AuxiliaryAxis *axis, uint16_t sample) {
    if (axis->minimum_sample_count >= AUXILIARY_AXIS_MINIMUM_SAMPLE_COUNT) {
        return;
    }

    axis->minimum_sample_count++;
    if (sample < axis->minimum_candidate) {
        axis->minimum_candidate = sample;
    }
    if (axis->minimum_sample_count == AUXILIARY_AXIS_MINIMUM_SAMPLE_COUNT) {
        axis->settings.minimum = axis->minimum_candidate + AUXILIARY_AXIS_ENDPOINT_MARGIN;
        axis->settle_threshold = axis->minimum_candidate + AUXILIARY_AXIS_SETTLE_OFFSET;
        axis->learning_minimum = false;
        axis->minimum_sample_count = 0;
    }
}

/**
 * @brief Tracks the initial maximum endpoint while calibration is incomplete.
 *
 * Requires more than 200 counts of usable travel. Manual mode follows every new maximum and arms a
 * future automatic capture; automatic mode captures only the first qualifying maximum before its
 * settle timer takes ownership.
 *
 * @param[in,out] axis Auxiliary calibration state to update.
 * @param[in] sample Filtered normalized sample.
 * @param[in] mode Current endpoint calibration mode.
 */
static void track_maximum(AuxiliaryAxis *axis, uint16_t sample, AuxiliaryAxisCalibrationMode mode) {
    if (!axis->limits_uninitialized) {
        return;
    }
    if (axis->learning_minimum) {
        learn_minimum(axis, sample);
    }
    if (sample <= axis->settings.maximum ||
        sample <= axis->settings.minimum + AUXILIARY_AXIS_MINIMUM_TRAVEL) {
        return;
    }

    if (mode == AUXILIARY_AXIS_AUTOMATIC_CALIBRATION) {
        if (!axis->maximum_capture_armed) {
            return;
        }
        axis->maximum_capture_armed = false;
    } else {
        axis->maximum_capture_armed = true;
    }
    axis->settings.maximum = sample;
}

/**
 * @brief Applies pending manual endpoint captures to the current sample.
 *
 * The minimum capture adds 40 counts. The maximum capture subtracts 40 counts and is accepted only
 * after the sample exceeds the current minimum by more than 200 counts.
 *
 * @param[in,out] axis Auxiliary calibration state to update.
 * @param[in] sample Filtered normalized sample.
 */
static void apply_manual_adjustments(AuxiliaryAxis *axis, uint16_t sample) {
    if (axis->maximum_adjustment_pending) {
        if (sample > axis->settings.minimum + AUXILIARY_AXIS_MINIMUM_TRAVEL) {
            axis->settings.maximum = sample - AUXILIARY_AXIS_ENDPOINT_MARGIN;
            axis->limits_uninitialized = false;
        }
        axis->maximum_adjustment_pending = false;
    }
    if (axis->minimum_adjustment_pending) {
        axis->settings.minimum = sample + AUXILIARY_AXIS_ENDPOINT_MARGIN;
        axis->minimum_adjustment_pending = false;
    }
}

/**
 * @brief Advances automatic maximum-endpoint settling.
 *
 * Starts a two-second hold when the input stays beyond the learned threshold. Completion subtracts
 * the 40-count margin, finalizes the calibration, and schedules the resulting settings snapshot.
 * Moving back to or below the threshold cancels the hold.
 *
 * @param[in,out] axis Auxiliary calibration state to update.
 * @param[in] sample Filtered normalized sample.
 * @param[in] now_ms Current monotonic time in milliseconds.
 */
static void settle_maximum(AuxiliaryAxis *axis, uint16_t sample, uint32_t now_ms) {
    if (axis->learning_minimum || sample <= axis->settle_threshold) {
        axis->settling = false;
        return;
    }
    if (!axis->settling) {
        axis->settle_deadline_ms = now_ms + AUXILIARY_AXIS_SETTLE_TIME_MS;
        axis->settling = true;
    }
    if ((int32_t)(now_ms - axis->settle_deadline_ms) < 0) {
        return;
    }

    axis->settings.maximum = sample - AUXILIARY_AXIS_ENDPOINT_MARGIN;
    axis->settle_threshold = axis->settings.maximum;
    axis->settling = false;
    axis->limits_uninitialized = false;
    axis->settings_changed = true;
}

/**
 * @brief Scales one calibrated auxiliary sample to its published byte.
 *
 * Clamps values at or below the minimum to zero and values at or above the maximum to 255, with
 * linear integer scaling between valid endpoints.
 *
 * @param[in] axis Auxiliary calibration state.
 * @param[in] sample Filtered normalized sample.
 * @return Calibrated auxiliary byte.
 */
static uint8_t scale_sample(const AuxiliaryAxis *axis, uint16_t sample) {
    uint16_t minimum = axis->settings.minimum;
    uint16_t maximum = axis->settings.maximum;
    if (sample <= minimum || maximum <= minimum) {
        return 0;
    }
    if (sample >= maximum) {
        return UINT8_MAX;
    }
    return (uint8_t)((uint32_t)(sample - minimum) * UINT8_MAX / (maximum - minimum));
}

/**
 * @brief Installs the startup auxiliary-axis settings.
 *
 * Uses the endpoint sentinels and requests startup learning on the first initialization.
 *
 * @param[out] settings Settings record to initialize.
 */
void auxiliary_axis_settings_defaults(AuxiliaryAxisSettings *settings) {
    *settings = (AuxiliaryAxisSettings){
        .minimum = AUXILIARY_AXIS_DEFAULT_MINIMUM,
        .maximum = AUXILIARY_AXIS_DEFAULT_MAXIMUM,
        .reset_on_start = true,
    };
}

/**
 * @brief Initializes auxiliary input filtering, calibration, and source state.
 *
 * Loads a valid retained endpoint pair or starts endpoint learning when requested or when the pair
 * is not ordered.
 *
 * @param[out] axis Auxiliary input state to initialize.
 * @param[in] settings Retained endpoint settings.
 */
void auxiliary_axis_init(AuxiliaryAxis *axis, const AuxiliaryAxisSettings *settings) {
    *axis = (AuxiliaryAxis){.settings = *settings};
    if (settings->reset_on_start || settings->minimum >= settings->maximum) {
        reset_limits(axis);
    }
}

/**
 * @brief Restarts auxiliary endpoint discovery.
 *
 * Restores the endpoint sentinels and arms both stages of calibration without disturbing the
 * accumulated input filter.
 *
 * @param[in,out] axis Auxiliary input state to reset.
 */
void auxiliary_axis_reset(AuxiliaryAxis *axis) { reset_limits(axis); }

/**
 * @brief Queues one manual endpoint capture.
 *
 * Records the requested endpoint for the next active sample and schedules a settings snapshot. A
 * maximum capture is ignored unless the sample exceeds the current minimum by more than 200 counts.
 *
 * @param[in,out] axis Auxiliary calibration state to update.
 * @param[in] adjustment Minimum or maximum endpoint to capture.
 */
void auxiliary_axis_request_adjustment(AuxiliaryAxis *axis, AuxiliaryAxisAdjustment adjustment) {
    if (adjustment == AUXILIARY_AXIS_ADJUST_MINIMUM) {
        axis->minimum_adjustment_pending = true;
    } else {
        axis->maximum_adjustment_pending = true;
    }
    axis->settings_changed = true;
}

/**
 * @brief Samples, calibrates, and scales the local auxiliary input.
 *
 * Detects the source before filtering, advances manual or automatic calibration for active input,
 * tracks startup endpoints, and publishes the resulting byte without affecting remote sources.
 *
 * @param[in,out] axis Auxiliary input state to update.
 * @param[in] adc_sample Raw 12-bit ADC sample.
 * @param[in] mode Current endpoint calibration mode.
 * @param[in] now_ms Current monotonic time in milliseconds.
 * @return Local-source activity and its calibrated byte.
 */
AuxiliaryAxisReading auxiliary_axis_update(AuxiliaryAxis *axis, uint16_t adc_sample,
                                           AuxiliaryAxisCalibrationMode mode, uint32_t now_ms) {
    uint16_t signal = read_signal(adc_sample);
    axis->active = signal_present(signal);
    if (!axis->active) {
        axis->value = 0;
        return (AuxiliaryAxisReading){0};
    }

    uint16_t filtered = analog_axis_filter(&axis->filter, signal, AUXILIARY_AXIS_FILTER_DEADBAND);
    uint16_t sample = normalize_signal(filtered);
    if (mode == AUXILIARY_AXIS_AUTOMATIC_CALIBRATION) {
        settle_maximum(axis, sample, now_ms);
    } else {
        apply_manual_adjustments(axis, sample);
    }
    track_maximum(axis, sample, mode);
    axis->value = scale_sample(axis, sample);
    return (AuxiliaryAxisReading){.value = axis->value, .active = true};
}

/**
 * @brief Takes the newest persistable auxiliary-axis settings.
 *
 * Copies the current endpoints and records whether the next startup must relearn them. The change
 * latch is cleared only when a snapshot is returned.
 *
 * @param[in,out] axis Auxiliary input state and change latch.
 * @param[in] mode Current endpoint calibration mode.
 * @param[out] settings Destination for the latest settings.
 * @return True when a changed snapshot was returned.
 */
bool auxiliary_axis_take_settings(AuxiliaryAxis *axis, AuxiliaryAxisCalibrationMode mode,
                                  AuxiliaryAxisSettings *settings) {
    if (!axis->settings_changed) {
        return false;
    }
    axis->settings.reset_on_start = mode == AUXILIARY_AXIS_AUTOMATIC_CALIBRATION;
    *settings = axis->settings;
    axis->settings_changed = false;
    return true;
}
