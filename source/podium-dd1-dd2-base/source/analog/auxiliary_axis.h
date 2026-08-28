#ifndef OPENTEC_BASE_ANALOG_AUXILIARY_AXIS_H
#define OPENTEC_BASE_ANALOG_AUXILIARY_AXIS_H

#include <stdbool.h>
#include <stdint.h>

#include "analog/axis.h"

typedef enum {
    AUXILIARY_AXIS_MANUAL_CALIBRATION,
    AUXILIARY_AXIS_AUTOMATIC_CALIBRATION,
} AuxiliaryAxisCalibrationMode;

typedef enum {
    AUXILIARY_AXIS_ADJUST_MINIMUM,
    AUXILIARY_AXIS_ADJUST_MAXIMUM,
} AuxiliaryAxisAdjustment;

typedef struct {
    uint16_t minimum;
    uint16_t maximum;
    bool reset_on_start;
} AuxiliaryAxisSettings;

typedef struct {
    AnalogAxisFilter filter;
    AuxiliaryAxisSettings settings;
    uint16_t minimum_candidate;
    uint16_t settle_threshold;
    uint32_t settle_deadline_ms;
    uint8_t minimum_sample_count;
    uint8_t value;
    bool active;
    bool limits_uninitialized;
    bool learning_minimum;
    bool maximum_capture_armed;
    bool settling;
    bool minimum_adjustment_pending;
    bool maximum_adjustment_pending;
    bool settings_changed;
} AuxiliaryAxis;

typedef struct {
    uint8_t value;
    bool active;
} AuxiliaryAxisReading;

void auxiliary_axis_settings_defaults(AuxiliaryAxisSettings *settings);
void auxiliary_axis_init(AuxiliaryAxis *axis, const AuxiliaryAxisSettings *settings);
void auxiliary_axis_reset(AuxiliaryAxis *axis);
void auxiliary_axis_request_adjustment(AuxiliaryAxis *axis, AuxiliaryAxisAdjustment adjustment);
AuxiliaryAxisReading auxiliary_axis_update(AuxiliaryAxis *axis, uint16_t adc_sample,
                                           AuxiliaryAxisCalibrationMode mode, uint32_t now_ms);
bool auxiliary_axis_take_settings(AuxiliaryAxis *axis, AuxiliaryAxisCalibrationMode mode,
                                  AuxiliaryAxisSettings *settings);

#endif
