#ifndef OPENTEC_MOTOR_FORCE_FEEDBACK_H
#define OPENTEC_MOTOR_FORCE_FEEDBACK_H

#include <stdbool.h>
#include <stdint.h>

#define MOTOR_FORCE_FEEDBACK_FILTER_CAPACITY 40U

enum {
    MOTOR_FORCE_FEEDBACK_POSITION_SLOT = 16,
    MOTOR_FORCE_FEEDBACK_DAMPER_SLOT = 18,
};

typedef struct {
    int32_t magnitude;
} MotorConstantEffect;

typedef struct {
    int32_t lower_position;
    int32_t upper_position;
    uint8_t lower_coefficient;
    uint8_t upper_coefficient;
    int8_t lower_direction;
    int8_t upper_direction;
    uint16_t saturation;
    uint16_t reserved;
} MotorWindowEffect;

typedef struct {
    uint16_t positive_coefficient;
    uint16_t negative_coefficient;
    int8_t positive_direction;
    int8_t negative_direction;
    uint16_t saturation;
    bool steering_scaled;
} MotorDirectionalEffect;

typedef struct {
    int32_t position_half_range;
    uint8_t overall_gain_percent;
    uint8_t filter_setting;
    uint8_t constant_gain_tenths;
    uint8_t window_gain_tenths;
    uint8_t directional_gain_tenths;
    uint8_t window_multiplier;
} MotorForceFeedbackSettings;

typedef struct {
    int32_t samples[MOTOR_FORCE_FEEDBACK_FILTER_CAPACITY];
    int32_t sum;
    uint8_t length;
    uint8_t index;
    uint8_t setting;
    bool initialized;
} MotorForceFeedbackFilter;

typedef struct {
    bool positive;
    uint16_t magnitude;
} MotorForceFeedbackOutput;

MotorForceFeedbackSettings motor_force_feedback_settings_default(void);
void motor_force_feedback_settings_apply(MotorForceFeedbackSettings *settings,
                                         int8_t steering_range, uint8_t overall_gain_percent,
                                         uint8_t filter_setting, uint8_t constant_gain_tenths,
                                         uint8_t window_gain_tenths,
                                         uint8_t directional_gain_tenths);
MotorConstantEffect motor_force_feedback_constant_decode(const uint8_t payload[5]);
MotorWindowEffect motor_force_feedback_window_decode(const uint8_t payload[5],
                                                     int32_t position_half_range);
MotorDirectionalEffect motor_force_feedback_directional_decode(const uint8_t payload[5]);
int32_t motor_force_feedback_constant_evaluate(const MotorConstantEffect *effect,
                                               uint8_t gain_tenths);
int32_t motor_force_feedback_directional_evaluate(const MotorDirectionalEffect *effect,
                                                  int32_t velocity, uint8_t gain_tenths,
                                                  uint8_t overall_gain_percent, uint8_t slot);
int32_t motor_force_feedback_window_evaluate(const MotorWindowEffect *effect, int32_t position,
                                             int32_t velocity, uint8_t multiplier,
                                             uint8_t gain_tenths, uint8_t slot,
                                             const MotorDirectionalEffect *internal_effect,
                                             uint8_t directional_gain_tenths,
                                             uint8_t overall_gain_percent);
uint8_t motor_force_feedback_filter_length(uint8_t setting);
void motor_force_feedback_filter_configure(MotorForceFeedbackFilter *filter, uint8_t setting);
int32_t motor_force_feedback_filter_apply(MotorForceFeedbackFilter *filter, int32_t force);
MotorForceFeedbackOutput motor_force_feedback_output_resolve(int32_t force);

#endif
