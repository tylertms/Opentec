#ifndef OPENTEC_MOTOR_LINK_FRAME_H
#define OPENTEC_MOTOR_LINK_FRAME_H

#include <stdbool.h>
#include <stdint.h>

/** @brief Total byte count of one motor-link frame. */
#define MOTOR_LINK_FRAME_SIZE 13U
/** @brief Payload byte count in one motor-link frame. */
#define MOTOR_LINK_PAYLOAD_SIZE 8U
/** @brief Byte count covered when calculating a motor-link frame checksum. */
#define MOTOR_LINK_CHECKSUM_INPUT_SIZE 9U

/**
 * @brief Result of validating a motor-link frame.
 *
 * Separates boundary and checksum failures from a frame that is ready for payload decoding.
 */
typedef enum {
    MOTOR_LINK_FRAME_VALID,             /**< Frame boundaries and checksum are valid. */
    MOTOR_LINK_FRAME_INVALID_BOUNDARY,  /**< Frame start or end marker is invalid. */
    MOTOR_LINK_FRAME_INVALID_CHECKSUM,  /**< Stored frame checksum does not match. */
} MotorLinkFrameResult;

/**
 * @brief Identifies the payload type in a motor-link frame.
 *
 * The values match the type byte used by force and status frames on the motor link.
 */
typedef enum {
    MOTOR_LINK_FORCE_TYPE = 1,  /**< Live-force command or position response. */
    MOTOR_LINK_STATUS_TYPE = 2, /**< Status and local-effect command. */
} MotorLinkFrameType;

/**
 * @brief Decoded motor-link frame payload.
 *
 * The type byte is retained separately from the eight payload bytes so callers can select the
 * corresponding command decoder.
 */
typedef struct {
    uint8_t type;                            /**< Wire frame type. */
    uint8_t payload[MOTOR_LINK_PAYLOAD_SIZE]; /**< Eight decoded payload bytes. */
} MotorLinkFrame;

/**
 * @brief Live-force command decoded from a type-one frame.
 *
 * Contains the commanded center, primary direction and magnitude, and signed secondary force.
 */
typedef struct {
    int16_t center;    /**< Commanded encoder center. */
    bool positive;     /**< Primary-force direction flag. */
    uint16_t primary;  /**< Unsigned primary-force magnitude. */
    int16_t secondary; /**< Signed secondary-force command. */
} MotorLinkForceCommand;

/**
 * @brief Status and local-effect command decoded from a type-two frame.
 *
 * The status byte selects protocol gates and the seven-byte command configures or controls an
 * effect slot.
 */
typedef struct {
    uint8_t status;    /**< Motor status bit field. */
    uint8_t command[7]; /**< Seven-byte force-feedback command. */
} MotorLinkStatusCommand;

/**
 * @brief Values encoded into one motor position response frame.
 *
 * The response reports extended position, torque, drive-current direction and magnitude, and the
 * replay indicator for the most recent received frame.
 */
typedef struct {
    int32_t position;      /**< Signed extended encoder position. */
    uint16_t torque;       /**< Unsigned motor torque value. */
    int16_t drive_current; /**< Signed drive-current value. */
    bool positive;         /**< Drive-current direction flag. */
    bool replay;           /**< Whether the next response reports a received-frame failure. */
} MotorLinkPositionReport;

/**
 * @brief Checks the fixed boundary markers of one motor-link frame.
 *
 * A complete frame must start with 0x7b and end with 0x7d before checksum validation.
 *
 * @param[in] input Complete received motor-link frame.
 * @return True when both boundary bytes are valid.
 */
bool motor_link_frame_boundaries_valid(const uint8_t input[MOTOR_LINK_FRAME_SIZE]);

/**
 * @brief Validates and decodes one motor-link frame.
 *
 * Checks the boundary markers and supplied CRC result before copying the type and payload into the
 * decoded frame.
 *
 * @param[in] input Complete received frame.
 * @param[in] checksum CRC peripheral result for the nine checksum input bytes.
 * @param[out] frame Destination for the decoded type and payload when validation succeeds.
 * @return Frame validation result.
 */
MotorLinkFrameResult motor_link_frame_decode_checked(const uint8_t input[MOTOR_LINK_FRAME_SIZE],
                                                     uint16_t checksum, MotorLinkFrame *frame);

/**
 * @brief Decodes a live-force command from a type-one motor-link frame.
 *
 * Copies the center, direction, primary magnitude, and signed secondary force from the frame
 * payload.
 *
 * @param[in] frame Decoded motor-link frame.
 * @param[out] command Destination for the decoded live-force command.
 * @return True when the frame is a live-force command.
 */
bool motor_link_force_command_decode(const MotorLinkFrame *frame, MotorLinkForceCommand *command);

/**
 * @brief Decodes a status and local-effect command from a type-two frame.
 *
 * Copies the status byte and seven-byte force-feedback command from the frame payload.
 *
 * @param[in] frame Decoded motor-link frame.
 * @param[out] command Destination for the decoded status command.
 * @return True when the frame is a status command.
 */
bool motor_link_status_command_decode(const MotorLinkFrame *frame, MotorLinkStatusCommand *command);

/**
 * @brief Prepares a motor-link position response frame before checksum insertion.
 *
 * Encodes position, torque, current magnitude and direction, and replay state into the fixed
 * thirteen-byte response layout.
 *
 * @param[in] report Current motor response values and replay flag.
 * @param[out] output Destination for the prepared frame.
 */
void motor_link_position_frame_prepare(const MotorLinkPositionReport *report,
                                       uint8_t output[MOTOR_LINK_FRAME_SIZE]);

/**
 * @brief Writes a checksum into a prepared motor-link frame.
 *
 * Stores the supplied little-endian CRC in the two checksum bytes between the response body and
 * its end marker.
 *
 * @param[in,out] frame Prepared motor-link frame.
 * @param[in] checksum CRC peripheral result for the checksum input bytes.
 */
void motor_link_frame_checksum_write(uint8_t frame[MOTOR_LINK_FRAME_SIZE], uint16_t checksum);

#endif
