#ifndef PODIUM_DD1_DD2_BASE_PEDAL_V4_TUNING_H
#define PODIUM_DD1_DD2_BASE_PEDAL_V4_TUNING_H

#include <stdbool.h>
#include <stdint.h>

/**
 * @brief Size of the fixed V4 tuning request payload.
 */
enum {
    PEDAL_V4_TUNING_REQUEST_SIZE = 23, /**< V4 tuning request size in bytes. */
};

/**
 * @brief Identifies a V4 pedal tuning setting.
 *
 * The values select the controller channel that receives a tuning update.
 */
typedef enum {
    PEDAL_V4_TUNING_THROTTLE_CURVE = 1, /**< Throttle response curve. */
    PEDAL_V4_TUNING_BRAKE_CURVE = 2,    /**< Brake response curve. */
    PEDAL_V4_TUNING_CLUTCH_CURVE = 3,   /**< Clutch response curve. */
    PEDAL_V4_TUNING_BRAKE_FORCE = 4,    /**< Brake force setting. */
} PedalV4TuningSetting;

/**
 * @brief Stores the current V4 pedal tuning values.
 *
 * Values are raw controller setting bytes and are transmitted only when changed.
 */
typedef struct {
    uint8_t brake_force;    /**< Raw brake-force setting. */
    uint8_t clutch_curve;   /**< Raw clutch-curve setting. */
    uint8_t brake_curve;    /**< Raw brake-curve setting. */
    uint8_t throttle_curve; /**< Raw throttle-curve setting. */
} PedalV4Tuning;

/**
 * @brief Builds a V4 tuning request payload.
 *
 * Fills the fixed request envelope with the selected setting and value and appends its checksum.
 *
 * @param[in] setting Tuning setting to write.
 * @param[in] value Raw one-byte setting value.
 * @param[out] output Destination for the 23-byte request payload.
 * @return True when setting and output are valid; otherwise false and output is unchanged.
 */
bool pedal_v4_tuning_request(PedalV4TuningSetting setting, uint8_t value,
                             uint8_t output[PEDAL_V4_TUNING_REQUEST_SIZE]);

#endif
