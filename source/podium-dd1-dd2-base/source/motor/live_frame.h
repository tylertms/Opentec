#ifndef OPENTEC_BASE_MOTOR_LIVE_FRAME_H
#define OPENTEC_BASE_MOTOR_LIVE_FRAME_H

#include <stdbool.h>
#include <stdint.h>

#include "force_feedback/output_report.h"

/**
 * @brief Motor live-frame sizes, delimiters, and type values.
 */
enum {
    MOTOR_LIVE_FRAME_SIZE = 13,  /**< Total encoded frame size including delimiters and checksum. */
    MOTOR_LIVE_PAYLOAD_SIZE = 8, /**< Payload bytes in each live frame. */
    MOTOR_LIVE_FRAME_START = 0x7b,   /**< Start delimiter required at the first frame byte. */
    MOTOR_LIVE_FRAME_END = 0x7d,     /**< End delimiter required at the last frame byte. */
    MOTOR_LIVE_POSITION_TYPE = 0x01, /**< Type for a live wheel-position and force frame. */
    MOTOR_LIVE_STATUS_TYPE = 0x02,   /**< Type for a status or command frame. */
    MOTOR_LIVE_REPLAY_FLAG = 0x80,   /**< Flag marking a replay of a retained position frame. */
};

/**
 * @brief Result of validating and decoding a motor live frame.
 */
typedef enum {
    MOTOR_LIVE_FRAME_VALID,            /**< Frame delimiters and checksum are valid. */
    MOTOR_LIVE_FRAME_INVALID_BOUNDARY, /**< Start or end delimiter is invalid. */
    MOTOR_LIVE_FRAME_INVALID_CHECKSUM, /**< Frame checksum does not match its contents. */
} MotorLiveFrameResult;

/**
 * @brief Decoded motor live-frame type and payload.
 */
typedef struct {
    uint8_t type; /**< Frame type byte, including the replay flag when present. */
    uint8_t payload[MOTOR_LIVE_PAYLOAD_SIZE]; /**< Eight-byte frame payload. */
} MotorLiveFrame;

/**
 * @brief Decoded wheel-position report from a position frame.
 */
typedef struct {
    bool replay;             /**< True when the source frame carries the replay flag. */
    int32_t wheel_position;  /**< Signed little-endian wheel-position sample. */
    uint16_t motor_torque;   /**< Unsigned little-endian motor-torque sample. */
    bool auxiliary_negative; /**< True when the auxiliary position direction bit is set. */
    uint16_t
        auxiliary_position; /**< Auxiliary magnitude from the lower 15 bits, shifted left one. */
} MotorPositionReport;

/**
 * @brief Encodes a motor live frame.
 *
 * Writes the start delimiter, type, payload, CRC-16/CCITT checksum, and end delimiter to the
 * fixed-size output buffer. The frame and output pointers must be non-null.
 *
 * @param[in] frame Frame type and payload to encode.
 * @param[out] output Destination for the encoded frame.
 */
void motor_live_frame_encode(const MotorLiveFrame *frame, uint8_t output[MOTOR_LIVE_FRAME_SIZE]);

/**
 * @brief Validates and decodes an encoded motor live frame.
 *
 * Checks both delimiters and the CRC before copying the type and payload into the output frame.
 * The input and frame pointers must be non-null. The output frame is unchanged when validation
 * fails.
 *
 * @param[in] input Encoded frame to validate.
 * @param[out] frame Destination for the decoded type and payload.
 * @return Valid, invalid-boundary, or invalid-checksum result.
 */
MotorLiveFrameResult motor_live_frame_decode(const uint8_t input[MOTOR_LIVE_FRAME_SIZE],
                                             MotorLiveFrame *frame);

/**
 * @brief Decodes a position report from a motor live frame.
 *
 * Accepts position type 0x01 with or without the replay flag and expands its wheel position,
 * torque, and auxiliary fields. Other frame types do not modify the report. The frame and report
 * pointers must be non-null.
 *
 * @param[in] frame Decoded live frame to inspect.
 * @param[out] report Destination for the decoded position report.
 * @return True when frame is a position or replay-position frame; otherwise false.
 */
bool motor_position_report_decode(const MotorLiveFrame *frame, MotorPositionReport *report);

/**
 * @brief Initializes a live force-output frame.
 *
 * Stores the wheel center and encoded force-output report in a position-type frame, clears the
 * unused payload byte, and leaves framing and checksum encoding to motor_live_frame_encode(). The
 * report and frame pointers must be non-null.
 *
 * @param[in] center_position Stored signed wheel-center position.
 * @param[in] report Force direction and primary and secondary magnitudes.
 * @param[out] frame Destination frame to initialize.
 */
void motor_live_force_frame_init(int16_t center_position, const ForceOutputReport *report,
                                 MotorLiveFrame *frame);

#endif
