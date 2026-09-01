#include "pedal/adjustment_probe.h"

#include <stdbool.h>
#include <stdint.h>

/**
 * @brief V4 adjustment-probe response classification constants.
 */
enum {
    MINIMUM_RESPONSE_SIZE = 5,     /**< Minimum response length accepted for classification. */
    THROTTLE_MARKER_OFFSET = 0x15, /**< Throttle marker byte offset. */
    CLUTCH_MARKER_OFFSET = 0x1f,   /**< Clutch marker byte offset. */
    ADJUSTED_MARKER = 'X',         /**< Marker indicating an adjusted pedal. */
    UNADJUSTED_TAIL_FIRST = 0xbf,  /**< First byte of the unadjusted response tail. */
    UNADJUSTED_TAIL_SECOND = 0x77, /**< Second byte of the unadjusted response tail. */
};

/**
 * @brief Fixed V4 adjustment-probe request payload.
 */
static const uint8_t request[PEDAL_ADJUSTMENT_PROBE_REQUEST_SIZE] = {
    0x16, 0x0a, 0x02, 0x08, 0x02, 0x20, 0x08, 0xaa, 0x01, 0x0d, 0xba, 0x01, 0x0a,
    0x5a, 0x08, 0x62, 0x06, 0x0a, 0x02, 0x00, 0x02, 0x10, 0x01, 0xb6, 0xf8,
};

/**
 * @brief Returns the V4 pedal-adjustment query payload.
 *
 * Provides the fixed 25-byte group-zero request submitted before an end-stop adjustment response.
 *
 * @return Read-only adjustment query payload.
 */
const uint8_t *pedal_adjustment_probe_request(void) { return request; }

/**
 * @brief Classifies a V4 pedal-adjustment query response.
 *
 * Gives the unadjusted tail signature priority, then checks the clutch and throttle markers in
 * that order. A valid response without either marker reports that both pedals require adjustment.
 *
 * @param[in] response Complete adjustment query response.
 * @param[in] length Number of response bytes.
 * @param[out] display Display command selected from the response.
 * @return True when response and display are non-null and response is long enough to classify.
 */
bool pedal_adjustment_probe_classify(const uint8_t *response, uint8_t length,
                                     PedalAdjustmentDisplay *display) {
    if (response == 0 || display == 0 || length < MINIMUM_RESPONSE_SIZE) {
        return false;
    }

    if (response[length - 2u] == UNADJUSTED_TAIL_FIRST &&
        response[length - 1u] == UNADJUSTED_TAIL_SECOND) {
        *display = PEDAL_ADJUSTMENT_DISPLAY_NONE;
    } else if (length > CLUTCH_MARKER_OFFSET && response[CLUTCH_MARKER_OFFSET] == ADJUSTED_MARKER) {
        *display = PEDAL_ADJUSTMENT_DISPLAY_CLUTCH;
    } else if (length > THROTTLE_MARKER_OFFSET &&
               response[THROTTLE_MARKER_OFFSET] == ADJUSTED_MARKER) {
        *display = PEDAL_ADJUSTMENT_DISPLAY_THROTTLE;
    } else {
        *display = PEDAL_ADJUSTMENT_DISPLAY_BOTH;
    }
    return true;
}
