#ifndef OPENTEC_BASE_WHEEL_PACKET_METADATA_H
#define OPENTEC_BASE_WHEEL_PACKET_METADATA_H

#include <stdbool.h>
#include <stdint.h>

#include "wheel/packet_common.h"

enum { WHEEL_PACKET_METADATA_MODE = 0x1e };

/** @brief Metadata-only input carried by attached-wheel mode 0x1E. */
typedef WheelPacketCommonInput WheelPacketMetadataInput;

bool wheel_packet_metadata_applies(uint8_t wheel_mode);
void wheel_packet_metadata_decode(const uint8_t request[WHEEL_PACKET_COMMON_REQUEST_SIZE],
                                  WheelPacketMetadataInput *input);

#endif
