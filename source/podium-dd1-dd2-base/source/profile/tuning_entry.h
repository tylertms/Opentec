#ifndef OPENTEC_BASE_PROFILE_TUNING_ENTRY_H
#define OPENTEC_BASE_PROFILE_TUNING_ENTRY_H

#include <stdbool.h>
#include <stdint.h>

#include "profile/bank.h"
#include "profile/tuning_interaction.h"

/** @brief Logical settings exposed by the local tuning menu. */
typedef enum {
    TUNING_ENTRY_SETUP,
    TUNING_ENTRY_SENSITIVITY,
    TUNING_ENTRY_FORCE_FEEDBACK_STRENGTH,
    TUNING_ENTRY_VIBRATION_STRENGTH,
    TUNING_ENTRY_BRAKE_INDICATOR_LEVEL,
    TUNING_ENTRY_FORCE_SCALE,
    TUNING_ENTRY_STEERING_DEADZONE,
    TUNING_ENTRY_DRIFT_COMPENSATION,
    TUNING_ENTRY_FORCE_EFFECT_STRENGTH,
    TUNING_ENTRY_SPRING_EFFECT_STRENGTH,
    TUNING_ENTRY_DAMPER_EFFECT_STRENGTH,
    TUNING_ENTRY_NATURAL_DAMPER,
    TUNING_ENTRY_NATURAL_FRICTION,
    TUNING_ENTRY_BRAKE_FORCE,
    TUNING_ENTRY_ALTERNATE_BRAKE_FORCE,
    TUNING_ENTRY_FORCE_EFFECT_INTENSITY,
    TUNING_ENTRY_MULTI_POSITION_MODE,
    TUNING_ENTRY_PADDLE_MODE,
    TUNING_ENTRY_INTERPOLATION_FILTER,
    TUNING_ENTRY_NATURAL_INERTIA,
    TUNING_ENTRY_FULL_FORCE,
    TUNING_ENTRY_BUTTON_ILLUMINATION,
    TUNING_ENTRY_DISPLAY_ROTATION,
    TUNING_ENTRY_BRAKE_PEDAL_CURVE,
    TUNING_ENTRY_CLUTCH_PEDAL_CURVE,
    TUNING_ENTRY_THROTTLE_PEDAL_CURVE,
    TUNING_ENTRY_COUNT,
} TuningEntry;

/** @brief Runtime conditions that change local tuning adjustment rules. */
typedef struct {
    bool security_code_active;
    bool automatic_setup_selected;
    bool alternate_brake_fine_step;
    bool multi_position_automatic_available;
} TuningEntryAdjustmentContext;

/** @brief Inclusive adjustment interval and increment for one tuning entry. */
typedef struct {
    int16_t minimum;
    int16_t maximum;
    uint8_t step;
    bool valid;
} TuningEntryLimits;

/** @brief Pedal connection kinds used by local tuning entry availability. */
typedef enum {
    TUNING_PEDALS_UNAVAILABLE,
    TUNING_PEDALS_LEGACY,
    TUNING_PEDALS_TRANSFER,
} TuningPedalConnection;

/** @brief Runtime capabilities that determine visible local tuning entries. */
typedef struct {
    uint8_t interface_mode;
    uint8_t wheel_mode;
    uint8_t wheel_aux_state;
    TuningPedalConnection pedal_connection;
    bool wheel_calibration_active;
    bool legacy_pedals;
    bool primary_pedal_calibration_active;
    bool secondary_pedal_calibration_active;
    bool wheel_status_mode_active;
    bool wheel_axis_report_enabled;
    bool vibration_mode_compatible;
} TuningEntryAvailabilityContext;

TuningEntryLimits tuning_entry_limits(TuningEntry entry, const TuningProfileBank *bank,
                                      const TuningEntryAdjustmentContext *context);
bool tuning_entry_adjustable_in_automatic_setup(TuningEntry entry);
bool tuning_entry_available(TuningEntry entry, const TuningProfileBank *bank,
                            const TuningEntryAvailabilityContext *context);
TuningEntry tuning_entry_navigate(TuningEntry current, TuningNavigationMode direction,
                                  const TuningProfileBank *bank,
                                  const TuningEntryAvailabilityContext *context);
bool tuning_entry_adjust(TuningProfileBank *bank, TuningEntry entry,
                         TuningNavigationEvent navigation,
                         const TuningEntryAdjustmentContext *context);

#endif
