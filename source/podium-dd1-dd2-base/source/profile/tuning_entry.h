#ifndef OPENTEC_BASE_PROFILE_TUNING_ENTRY_H
#define OPENTEC_BASE_PROFILE_TUNING_ENTRY_H

#include <stdbool.h>
#include <stdint.h>

#include "profile/bank.h"
#include "profile/tuning_interaction.h"
#include "wheel/accessory.h"

/** @brief Logical settings exposed by the local tuning menu. */
typedef enum {
    TUNING_ENTRY_SETUP,                   /**< Selected setup. */
    TUNING_ENTRY_SENSITIVITY,             /**< Steering sensitivity. */
    TUNING_ENTRY_FORCE_FEEDBACK_STRENGTH, /**< Force-feedback strength. */
    TUNING_ENTRY_VIBRATION_STRENGTH,      /**< Vibration strength. */
    TUNING_ENTRY_BRAKE_INDICATOR_LEVEL,   /**< Brake indicator level. */
    TUNING_ENTRY_FORCE_SCALE,             /**< Force-feedback scaling. */
    TUNING_ENTRY_STEERING_DEADZONE,       /**< Steering deadzone. */
    TUNING_ENTRY_DRIFT_COMPENSATION,      /**< Drift compensation. */
    TUNING_ENTRY_FORCE_EFFECT_STRENGTH,   /**< Force-effect strength. */
    TUNING_ENTRY_SPRING_EFFECT_STRENGTH,  /**< Spring-effect strength. */
    TUNING_ENTRY_DAMPER_EFFECT_STRENGTH,  /**< Damper-effect strength. */
    TUNING_ENTRY_NATURAL_DAMPER,          /**< Natural damper. */
    TUNING_ENTRY_NATURAL_FRICTION,        /**< Natural friction. */
    TUNING_ENTRY_BRAKE_FORCE,             /**< Primary brake force. */
    TUNING_ENTRY_ALTERNATE_BRAKE_FORCE,   /**< Alternate brake force. */
    TUNING_ENTRY_FORCE_EFFECT_INTENSITY,  /**< Force-effect intensity. */
    TUNING_ENTRY_MULTI_POSITION_MODE,     /**< Multi-position switch mode. */
    TUNING_ENTRY_PADDLE_MODE,             /**< Analogue paddle mode. */
    TUNING_ENTRY_INTERPOLATION_FILTER,    /**< Interpolation filter. */
    TUNING_ENTRY_NATURAL_INERTIA,         /**< Natural inertia. */
    TUNING_ENTRY_FULL_FORCE,              /**< Full-force flag. */
    TUNING_ENTRY_BUTTON_ILLUMINATION,     /**< Button illumination flag. */
    TUNING_ENTRY_DISPLAY_ROTATION,        /**< Display rotation flag. */
    TUNING_ENTRY_BRAKE_PEDAL_CURVE,       /**< Brake pedal curve. */
    TUNING_ENTRY_CLUTCH_PEDAL_CURVE,      /**< Clutch pedal curve. */
    TUNING_ENTRY_THROTTLE_PEDAL_CURVE,    /**< Throttle pedal curve. */
    TUNING_ENTRY_COUNT,                   /**< Number of logical tuning entries. */
} TuningEntry;

/** @brief Runtime conditions that change local tuning adjustment rules. */
typedef struct {
    bool security_code_active;      /**< True while security code blocks editing. */
    bool automatic_setup_selected;  /**< True when the automatic setup is selected. */
    bool alternate_brake_fine_step; /**< True when alternate brake uses one-percent steps. */
    bool multi_position_automatic_available; /**< True when automatic switch mode is available. */
    bool xbox_mode; /**< True when the Xbox interface limits available entries. */
} TuningEntryAdjustmentContext;

/** @brief Inclusive adjustment interval and increment for one tuning entry. */
typedef struct {
    int16_t minimum; /**< Inclusive minimum value. */
    int16_t maximum; /**< Inclusive maximum value. */
    uint8_t step;    /**< Adjustment increment. */
    bool valid;      /**< True when these limits apply to the requested entry. */
} TuningEntryLimits;

/** @brief Pedal capability levels used by local tuning entry availability. */
typedef enum {
    TUNING_PEDALS_UNAVAILABLE, /**< No supported pedal connection. */
    TUNING_PEDALS_LEGACY,      /**< Legacy pedal connection. */
    TUNING_PEDALS_TRANSFER,    /**< Transfer-capable pedal connection. */
} TuningPedalConnection;

/** @brief Runtime capabilities that determine visible local tuning entries. */
typedef struct {
    uint8_t interface_mode;                  /**< Active host interface mode. */
    uint8_t wheel_mode;                      /**< Attached-wheel mode identifier. */
    WheelAccessoryKind wheel_accessory_kind; /**< Attached-wheel accessory kind. */
    uint8_t wheel_auxiliary_state;           /**< Attached-wheel auxiliary state byte. */
    TuningPedalConnection pedal_connection;  /**< Attached pedal connection type. */
    bool legacy_pedal_mode;                  /**< True when legacy pedal compatibility is active. */
    bool motor_calibration_active;           /**< True while motor calibration is active. */
    bool primary_pedal_calibration_active;   /**< True while primary pedal calibration is active. */
    bool secondary_pedal_calibration_active; /**< True while secondary pedal calibration is active.
                                              */
    bool multi_position_supported;  /**< True when the wheel supports multi-position input. */
    bool wheel_axis_report_enabled; /**< True when analogue wheel-axis reporting is enabled. */
    bool vibration_mode_compatible; /**< True when vibration control is supported. */
} TuningEntryAvailabilityContext;

/**
 * @brief Returns active adjustment limits for one entry.
 *
 * Applies Standard-mode and runtime adjustment restrictions to the catalog limits.
 *
 * @param[in] entry Logical tuning entry.
 * @param[in] bank Current profile bank and mode.
 * @param[in] context Runtime adjustment conditions.
 * @return Limits with valid true when inputs are valid; zeroed limits with valid false otherwise.
 */
TuningEntryLimits tuning_entry_limits(TuningEntry entry, const TuningProfileBank *bank,
                                      const TuningEntryAdjustmentContext *context);

/**
 * @brief Reports whether automatic setup permits an entry adjustment.
 *
 * Identifies vibration, braking, force-scale, switch, paddle, illumination, display, and pedal
 * curve entries that remain user-adjustable while automatic setup protects generated values.
 *
 * @param[in] entry Logical tuning entry.
 * @return true when entry is adjustable in automatic setup; false otherwise.
 */
bool tuning_entry_adjustable_in_automatic_setup(TuningEntry entry);

/**
 * @brief Reports whether an entry is visible locally.
 *
 * Combines host-interface, attached-hardware, and Standard-mode availability rules. In Standard
 * mode, the restricted entry set applies while the active slot is zero or one.
 *
 * @param[in] entry Logical tuning entry.
 * @param[in] bank Current profile bank and mode.
 * @param[in] context Current interface and hardware capabilities.
 * @return true when entry can be navigated; false when it is unsupported or inputs are invalid.
 */
bool tuning_entry_available(TuningEntry entry, const TuningProfileBank *bank,
                            const TuningEntryAvailabilityContext *context);

/**
 * @brief Finds the next available entry in display order.
 *
 * Moves forward or backward with wraparound and skips entries unavailable in the current context.
 * A current value that is not in the display order starts at the appropriate end of the order.
 *
 * @param[in] current Current entry, or TUNING_ENTRY_COUNT before the first selection.
 * @param[in] direction Previous or next navigation direction.
 * @param[in] bank Current profile bank and mode.
 * @param[in] context Current interface and hardware capabilities.
 * @return Next available entry; current when direction or inputs are invalid or none are available.
 */
TuningEntry tuning_entry_navigate(TuningEntry current, TuningNavigationMode direction,
                                  const TuningProfileBank *bank,
                                  const TuningEntryAvailabilityContext *context);

/**
 * @brief Applies one local tuning adjustment.
 *
 * Enforces security and automatic-setup restrictions, clamps the requested value, and activates a
 * changed setup selection immediately.
 *
 * @param[in,out] bank Profile bank to update.
 * @param[in] entry Logical tuning entry.
 * @param[in] navigation Increase, decrease, or analogue navigation event.
 * @param[in] context Runtime adjustment conditions.
 * @return true when the bank changed; false when the event is rejected or produces no change.
 */
bool tuning_entry_adjust(TuningProfileBank *bank, TuningEntry entry,
                         TuningNavigationEvent navigation,
                         const TuningEntryAdjustmentContext *context);

#endif
