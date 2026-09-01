#ifndef PODIUM_DD1_DD2_BASE_PEDAL_V4_STATUS_H
#define PODIUM_DD1_DD2_BASE_PEDAL_V4_STATUS_H

#include <stdint.h>

/**
 * @brief Sizes used by the V4 status request and response parser.
 */
enum {
    PEDAL_V4_STATUS_AXIS_COUNT = 3,     /**< Number of pedal axes in a V4 status response. */
    PEDAL_V4_STATUS_ENVELOPE_SIZE = 25, /**< Bytes before the first V4 status record. */
    PEDAL_V4_STATUS_REQUEST_SIZE = 21,  /**< V4 status request size in bytes. */
};

/**
 * @brief Returns the fixed V4 pedal-status request.
 *
 * The returned payload remains owned by the module and must not be modified by the caller.
 *
 * @return Read-only 21-byte V4 status request payload.
 */
const uint8_t *pedal_v4_status_request(void);

/**
 * @brief Parses V4 status records into pedal-axis values.
 *
 * Reads bounded selector/value records after the response envelope and publishes axes in primary,
 * secondary, and tertiary order.
 *
 * @param[in] data Complete V4 status response bytes.
 * @param[in] length Number of response bytes.
 * @param[out] axes Destination for the three decoded pedal axes; missing selectors are written as
 * zero, while the destination is unchanged when data is null or no bytes follow the envelope.
 */
void pedal_v4_status_parse(const uint8_t *data, uint16_t length,
                           uint16_t axes[PEDAL_V4_STATUS_AXIS_COUNT]);

#endif
