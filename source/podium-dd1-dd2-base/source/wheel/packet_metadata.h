#ifndef OPENTEC_BASE_WHEEL_PACKET_METADATA_H
#define OPENTEC_BASE_WHEEL_PACKET_METADATA_H

#include <stdbool.h>
#include <stdint.h>

#include "wheel/packet_common.h"

/** @brief Mode value for metadata-only attached-wheel packets. */
enum { WHEEL_PACKET_METADATA_MODE = 0x1e /**< Metadata-only packet mode. */ };

/** @brief Metadata-only input carried by attached-wheel mode 0x1E. */
typedef WheelPacketCommonInput WheelPacketMetadataInput;

/**
 * @brief Reports whether a wheel mode uses metadata-only packets.
 *
 * Selects operating mode 0x1E.
 *
 * @param[in] wheel_mode Negotiated attached-wheel mode.
 * @return True for WHEEL_PACKET_METADATA_MODE; otherwise false.
 */
bool wheel_packet_metadata_applies(uint8_t wheel_mode);

/**
 * @brief Decodes a metadata-only attached-wheel request.
 *
 * Clears the logical input and copies the request's axis values and report metadata without
 * applying normal button or control processing.
 *
 * @param[in] request Thirty-two-byte attached-wheel request.
 * @param[out] input Metadata input state to populate.
 */
void wheel_packet_metadata_decode(const uint8_t request[WHEEL_PACKET_COMMON_REQUEST_SIZE],
                                  WheelPacketMetadataInput *input);

#endif
