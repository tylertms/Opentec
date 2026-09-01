#ifndef OPENTEC_BASE_PEDAL_ADJUSTMENT_PROBE_H
#define OPENTEC_BASE_PEDAL_ADJUSTMENT_PROBE_H

#include <stdbool.h>
#include <stdint.h>

/**
 * @brief Size of the fixed V4 adjustment-probe request payload.
 */
enum {
    PEDAL_ADJUSTMENT_PROBE_REQUEST_SIZE = 25, /**< V4 adjustment-probe request size in bytes. */
};

/**
 * @brief Identifies the V4 pedal-adjustment display result.
 *
 * Values describe the current adjustment status reported by the pedal controller.
 */
typedef enum {
    PEDAL_ADJUSTMENT_DISPLAY_IDLE = 0,        /**< No new adjustment display value is available. */
    PEDAL_ADJUSTMENT_DISPLAY_HOLD = 0x80,     /**< Hold the adjustment display while probing. */
    PEDAL_ADJUSTMENT_DISPLAY_NONE = 0x90,     /**< Neither pedal requires adjustment. */
    PEDAL_ADJUSTMENT_DISPLAY_BOTH = 0x91,     /**< Both throttle and clutch require adjustment. */
    PEDAL_ADJUSTMENT_DISPLAY_THROTTLE = 0x93, /**< The throttle requires adjustment. */
    PEDAL_ADJUSTMENT_DISPLAY_CLUTCH = 0x95,   /**< The clutch requires adjustment. */
} PedalAdjustmentDisplay;

/**
 * @brief Returns the fixed V4 pedal-adjustment probe request.
 *
 * The returned payload remains owned by the module and must not be modified by the caller.
 *
 * @return Read-only 25-byte adjustment-probe request payload.
 */
const uint8_t *pedal_adjustment_probe_request(void);

/**
 * @brief Classifies a V4 pedal-adjustment probe response.
 *
 * Selects the display status from the response tail and pedal markers without modifying the
 * response buffer.
 *
 * @param[in] response Complete adjustment-probe response bytes.
 * @param[in] length Number of response bytes.
 * @param[out] display Destination for the classified display result.
 * @return True when response and display are valid and response is long enough to classify.
 */
bool pedal_adjustment_probe_classify(const uint8_t *response, uint8_t length,
                                     PedalAdjustmentDisplay *display);

#endif
