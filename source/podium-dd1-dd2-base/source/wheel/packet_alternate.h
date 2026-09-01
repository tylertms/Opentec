#ifndef OPENTEC_BASE_WHEEL_PACKET_ALTERNATE_H
#define OPENTEC_BASE_WHEEL_PACKET_ALTERNATE_H

#include <stdbool.h>
#include <stdint.h>

#include "wheel/packet_common.h"

/** @brief Alternate-packet history and payload dimensions. */
enum {
    WHEEL_PACKET_ALTERNATE_HISTORY_DEPTH = 3,  /**< Button-history sample count. */
    WHEEL_PACKET_ALTERNATE_PAYLOAD_SIZE = 30,  /**< Transfer payload size in bytes. */
    WHEEL_PACKET_ALTERNATE_RESPONSE_SIZE = 33, /**< Complete response size in bytes. */
};

/** @brief Logical input fields carried by an alternate mode packet. */
typedef WheelPacketCommonInput WheelPacketAlternateInput;

/** @brief Three-sample button filter used by alternate mode input. */
typedef struct {
    uint8_t samples[WHEEL_PACKET_ALTERNATE_HISTORY_DEPTH]
                   [WHEEL_PACKET_COMMON_BUTTON_COUNT]; /**< Recent button samples. */
    uint8_t next_sample;                               /**< Index receiving the next sample. */
} WheelPacketAlternateFilter;

/** @brief Retained display, command, and transfer state for alternate mode output. */
typedef struct {
    WheelDisplayOutput display;                           /**< Current display output. */
    uint8_t payload[WHEEL_PACKET_ALTERNATE_PAYLOAD_SIZE]; /**< Retained transfer payload. */
    uint8_t sequence;                                     /**< Current transfer sequence number. */
    uint8_t report_state;                                 /**< Current alternate report state. */
    uint8_t auxiliary_link_option;                        /**< Auxiliary-link display option. */
    bool auxiliary_status;                                /**< Current auxiliary-link status. */
    bool suppress_auxiliary_display; /**< Whether auxiliary display output is suppressed. */
    bool command_restart_pending;    /**< Whether the command response must restart. */
    bool payload_pending;            /**< Whether a payload awaits transfer. */
    bool payload_suppressed;         /**< Whether payload transfer is temporarily suppressed. */
    bool transfer_active;            /**< Whether a payload transfer is in progress. */
} WheelPacketAlternateOutput;

/**
 * @brief Reports whether a wheel mode uses alternate packets.
 *
 * Selects the alternate operating mode 0x12.
 *
 * @param[in] wheel_mode Negotiated attached-wheel mode.
 * @return True for mode 0x12; otherwise false.
 */
bool wheel_packet_alternate_applies(uint8_t wheel_mode);

/**
 * @brief Initializes alternate-packet button filtering.
 *
 * Clears all retained button samples and resets the insertion index.
 *
 * @param[out] filter Filter state to initialize.
 */
void wheel_packet_alternate_filter_init(WheelPacketAlternateFilter *filter);

/**
 * @brief Remaps and filters one alternate-packet input.
 *
 * Applies interface-specific button remapping, retains only bits present in all recent samples,
 * and clears controls two through five.
 *
 * @param[in,out] filter Button-history state to update.
 * @param[in,out] input Input to remap and filter in place.
 * @param[in] interface_mode Active host interface mode.
 */
void wheel_packet_alternate_filter(WheelPacketAlternateFilter *filter,
                                   WheelPacketAlternateInput *input, uint8_t interface_mode);

/**
 * @brief Queues an alternate-packet transfer payload.
 *
 * Retains the payload and marks a new transfer pending when no earlier payload is pending.
 *
 * @param[in,out] output Alternate-packet output state to update.
 * @param[in] payload Complete thirty-byte transfer payload.
 * @return True when the payload was retained; otherwise false.
 */
bool wheel_packet_alternate_queue_payload(
    WheelPacketAlternateOutput *output, const uint8_t payload[WHEEL_PACKET_ALTERNATE_PAYLOAD_SIZE]);

/**
 * @brief Reports whether an alternate transfer payload is pending.
 *
 * Reads the pending state without consuming transfer data.
 *
 * @param[in] output Alternate-packet output state to inspect.
 * @return True while a payload awaits transfer; otherwise false.
 */
bool wheel_packet_alternate_payload_pending(const WheelPacketAlternateOutput *output);

/**
 * @brief Encodes the next alternate-packet response.
 *
 * Emits a pending transfer segment when due or the current default display response otherwise.
 *
 * @param[in,out] output Alternate-packet output and transfer state to advance.
 * @param[out] response Thirty-three-byte response destination.
 */
void wheel_packet_alternate_encode(WheelPacketAlternateOutput *output,
                                   uint8_t response[WHEEL_PACKET_ALTERNATE_RESPONSE_SIZE]);

#endif
