#ifndef OPENTEC_MOTOR_ENCODER_CALIBRATION_H
#define OPENTEC_MOTOR_ENCODER_CALIBRATION_H

#include <stdbool.h>
#include <stdint.h>

#include "motor/motion.h"

/** @brief Number of directional encoder-correction samples in each table. */
#define MOTOR_ENCODER_CORRECTION_CAPACITY 0x952U

/** @brief Persistent header, correction tables, and checksum for encoder calibration. */
typedef struct {
    uint32_t magic; /**< Record-format magic value. */
    uint32_t version; /**< Record-format version. */
    uint32_t correction_scale; /**< Fixed-point scale applied when reading corrections. */
    uint32_t sample_offset; /**< Directional table sample offset. */
    int16_t forward[MOTOR_ENCODER_CORRECTION_CAPACITY]; /**< Forward correction samples. */
    int16_t reverse[MOTOR_ENCODER_CORRECTION_CAPACITY]; /**< Reverse correction samples. */
    uint32_t checksum; /**< CRC-32 over all preceding record bytes. */
} MotorEncoderCalibrationRecord;

/** @brief Phases of the encoder correction calibration state machine. */
typedef enum {
    kMotorEncoderCalibrationInitialize, /**< Prepare the next calibration run. */
    kMotorEncoderCalibrationStartForward, /**< Set the forward sweep target and limits. */
    kMotorEncoderCalibrationSettleForward, /**< Wait for stable forward velocity. */
    kMotorEncoderCalibrationCaptureForward, /**< Capture one forward revolution. */
    kMotorEncoderCalibrationSettleReverse, /**< Wait for stable reverse velocity. */
    kMotorEncoderCalibrationCaptureReverse, /**< Capture one reverse revolution. */
    kMotorEncoderCalibrationCenter, /**< Return the shaft to center before completion. */
} MotorEncoderCalibrationPhase;

/** @brief Results reported by the encoder correction calibration state machine. */
typedef enum {
    kMotorEncoderCalibrationPending, /**< Calibration remains in progress. */
    kMotorEncoderCalibrationComplete, /**< Calibration completed and the record header is ready. */
} MotorEncoderCalibrationResult;

/** @brief Motion and correction inputs consumed by one calibration step. */
typedef struct {
    int16_t velocity; /**< Signed filtered velocity sample. */
    int16_t correction; /**< Signed velocity-controller correction sample. */
    int32_t position; /**< Current extended encoder position. */
    uint16_t relative_position; /**< Current position within one encoder revolution. */
    uint16_t encoder_period; /**< Encoder counts per revolution. */
    bool revolution_complete; /**< True when the armed revolution completed. */
} MotorEncoderCalibrationInput;

/** @brief Actions and result produced by one encoder calibration step. */
typedef struct {
    MotorEncoderCalibrationResult result; /**< Calibration result. */
    int16_t target_velocity; /**< Velocity target for the calibration controller. */
    bool reset_controller; /**< True when velocity PI integral and previous-error history reset. */
    bool arm_revolution; /**< True when revolution completion tracking must be armed. */
    bool clear_revolution; /**< True when revolution completion tracking must be cleared. */
} MotorEncoderCalibrationStep;

/** @brief Persistent state for encoder correction calibration. */
typedef struct {
    MotorEncoderCalibrationPhase phase; /**< Current calibration phase. */
    uint32_t settle_count; /**< Number of service steps spent in the active settle phase. */
    int16_t velocity_lower; /**< Inclusive lower velocity-settle bound. */
    int16_t velocity_upper; /**< Inclusive upper velocity-settle bound. */
    uint16_t sweep_start_position; /**< Relative position at the start of the active sweep. */
    int16_t target_velocity; /**< Current calibration velocity target. */
    MotorMotionFilter correction_filter; /**< Filter applied to captured correction samples. */
    MotorEncoderCalibrationRecord record; /**< Captured corrections and persistent record fields. */
} MotorEncoderCalibrationState;

/**
 * @brief Initializes encoder correction calibration state.
 *
 * The record and runtime state are cleared, and the correction filter is configured for capture.
 *
 * @param[out] state Calibration state to initialize.
 */
void motor_encoder_calibration_initialize(MotorEncoderCalibrationState *state);

/**
 * @brief Advances encoder correction calibration by one input sample.
 *
 * The state machine settles, captures forward and reverse tables, and returns the shaft to center.
 *
 * @param[in,out] state Persistent calibration state to update.
 * @param[in] input Current velocity, correction, position, encoder phase, and revolution event.
 * @return Calibration actions and completion result.
 */
MotorEncoderCalibrationStep
motor_encoder_calibration_step(MotorEncoderCalibrationState *state,
                               const MotorEncoderCalibrationInput *input);

/**
 * @brief Reads and scales a directional encoder correction sample.
 *
 * The persisted sample offset is applied before wrapping the selected table index.
 *
 * @param[in] record Valid persisted encoder correction record.
 * @param[in] reverse True to read the reverse table.
 * @param[in] relative_position Encoder position within one revolution.
 * @param[in] table_length Active correction-table length.
 * @return Signed scaled encoder correction.
 */
int16_t motor_encoder_correction_read(const MotorEncoderCalibrationRecord *record, bool reverse,
                                      uint16_t relative_position, uint16_t table_length);

/**
 * @brief Updates the selected encoder correction-table direction.
 *
 * Direction changes when the filtered position delta reaches the configured threshold.
 *
 * @param[in] reverse Previously selected reverse-table flag.
 * @param[in] filtered_position_delta Current filtered position delta.
 * @return True when the reverse table is selected.
 */
bool motor_encoder_correction_direction_update(bool reverse, int16_t filtered_position_delta);

/**
 * @brief Calculates and stores the encoder calibration record checksum.
 *
 * The CRC covers every record byte except the checksum field itself.
 *
 * @param[in,out] record Completed calibration record to finalize.
 */
void motor_encoder_calibration_record_finalize(MotorEncoderCalibrationRecord *record);

/**
 * @brief Validates an encoder calibration record.
 *
 * Magic, version, and checksum must all match the supported record format.
 *
 * @param[in] record Calibration record to validate.
 * @return True when the record is valid.
 */
bool motor_encoder_calibration_record_is_valid(const MotorEncoderCalibrationRecord *record);

#endif
