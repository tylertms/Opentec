#include "wheel/packet_metadata.h"

#include <stdbool.h>
#include <stdint.h>

/** @brief Internal metadata-packet payload offsets. */
enum {
    REQUEST_PAYLOAD_OFFSET = 2,              /**< Request payload offset. */
    PAYLOAD_AXIS_VALUES_OFFSET = 16,         /**< Axis-value offset within the payload. */
    PAYLOAD_REPORT_MODE_OFFSET = 26,         /**< Report-mode offset within the payload. */
    PAYLOAD_REPORT_CAPABILITIES_OFFSET = 28, /**< Report-capability offset. */
    PAYLOAD_AXIS_LIMIT_OFFSET = 29,          /**< Axis-limit offset. */
};

/**
 * @brief Reads one little-endian 16-bit packet value.
 *
 * Combines adjacent low and high bytes without relying on host alignment or byte order.
 *
 * @param[in] data First byte of the encoded value.
 * @return Decoded unsigned value.
 */
static uint16_t read_little_endian_u16(const uint8_t data[2]) {
    return (uint16_t)data[0] | (uint16_t)data[1] << 8;
}

/**
 * @brief Reports whether a wheel mode uses metadata-only input.
 *
 * Selects authenticated mode 0x1E, whose request updates report state without publishing buttons,
 * motion, controls, or normalized output axes.
 *
 * @param[in] wheel_mode Selected attached-wheel mode.
 * @return True for mode 0x1E.
 */
bool wheel_packet_metadata_applies(uint8_t wheel_mode) {
    return wheel_mode == WHEEL_PACKET_METADATA_MODE;
}

/**
 * @brief Decodes a metadata-only attached-wheel request.
 *
 * Clears the shared logical input, then retains the two 16-bit axis values, report mode,
 * capability byte, and axis limit from their common-payload positions.
 *
 * @param[in] request Complete attached-wheel request.
 * @param[out] input Cleared logical input receiving report metadata.
 */
void wheel_packet_metadata_decode(const uint8_t request[WHEEL_PACKET_COMMON_REQUEST_SIZE],
                                  WheelPacketMetadataInput *input) {
    const uint8_t *payload = &request[REQUEST_PAYLOAD_OFFSET];
    *input = (WheelPacketMetadataInput){0};
    input->axis_values[0] = read_little_endian_u16(&payload[PAYLOAD_AXIS_VALUES_OFFSET]);
    input->axis_values[1] = read_little_endian_u16(&payload[PAYLOAD_AXIS_VALUES_OFFSET + 2]);
    input->report_mode = payload[PAYLOAD_REPORT_MODE_OFFSET];
    input->report_capabilities = payload[PAYLOAD_REPORT_CAPABILITIES_OFFSET];
    input->axis_limit = payload[PAYLOAD_AXIS_LIMIT_OFFSET];
}
