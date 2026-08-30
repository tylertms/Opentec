#include "pedal/v4_tuning.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

enum {
    PEDAL_V4_TUNING_PREFIX_SIZE = 19,
    PEDAL_V4_TUNING_CRC_POLYNOMIAL = 0x8408,
};

static const uint8_t tuning_prefix[PEDAL_V4_TUNING_PREFIX_SIZE] = {
    0x14, 0x0a, 0x02, 0x08, 0x02, 0x18, 0x01, 0x20, 0x08, 0xaa,
    0x01, 0x09, 0xaa, 0x01, 0x06, 0x5a, 0x04, 0x12, 0x02,
};

/**
 * @brief Maps a V4 tuning setting to its request offset.
 *
 * Selects the four supported eight-byte channel offsets and rejects all other setting values.
 *
 * @param[in] setting Tuning setting to map.
 * @return Request offset, or zero when the setting is unsupported.
 */
static uint8_t setting_offset(PedalV4TuningSetting setting) {
    switch (setting) {
    case PEDAL_V4_TUNING_THROTTLE_CURVE:
        return 8;
    case PEDAL_V4_TUNING_BRAKE_CURVE:
        return 16;
    case PEDAL_V4_TUNING_CLUTCH_CURVE:
        return 24;
    case PEDAL_V4_TUNING_BRAKE_FORCE:
        return 32;
    }
    return 0;
}

/**
 * @brief Calculates the V4 tuning payload checksum.
 *
 * Applies the reflected 0x8408 polynomial from an all-zero initial value.
 *
 * @param[in] data Payload bytes to checksum.
 * @param[in] length Number of payload bytes.
 * @return Calculated 16-bit checksum.
 */
static uint16_t calculate_crc(const uint8_t *data, uint8_t length) {
    uint16_t crc = 0;
    for (uint8_t index = 0; index < length; index++) {
        crc ^= data[index];
        for (uint8_t bit = 0; bit < 8; bit++) {
            crc = (crc & 1u) != 0 ? (uint16_t)((crc >> 1) ^ PEDAL_V4_TUNING_CRC_POLYNOMIAL)
                                  : (uint16_t)(crc >> 1);
        }
    }
    return crc;
}

/**
 * @brief Builds a V4 tuning write with its setting offset, raw value, and CRC-16.
 *
 * Copies the fixed request envelope, inserts the selected channel offset and value, and appends the
 * checksum over the first 21 bytes.
 *
 * @param[in] setting Throttle, brake, clutch, or brake-force setting to write.
 * @param[in] value Raw one-byte setting value.
 * @param[out] output Destination for the 23-byte transfer payload.
 * @return True for a supported setting; false without changing output otherwise.
 */
bool pedal_v4_tuning_request(PedalV4TuningSetting setting, uint8_t value,
                             uint8_t output[PEDAL_V4_TUNING_REQUEST_SIZE]) {
    uint8_t offset = setting_offset(setting);
    if (offset == 0 || output == NULL) {
        return false;
    }

    for (uint8_t index = 0; index < PEDAL_V4_TUNING_PREFIX_SIZE; index++) {
        output[index] = tuning_prefix[index];
    }
    output[19] = offset;
    output[20] = value;
    uint16_t crc = calculate_crc(output, 21);
    output[21] = (uint8_t)crc;
    output[22] = (uint8_t)(crc >> 8);
    return true;
}
