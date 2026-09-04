#ifndef OPENTEC_BASE_USB_CONTROL_PIPE_H
#define OPENTEC_BASE_USB_CONTROL_PIPE_H

#include <stdbool.h>
#include <stdint.h>

#include "usb/device_control.h"

/** @brief Maximum endpoint-zero control-pipe packet size in bytes. */
enum { USB_CONTROL_PACKET_SIZE = 64 /**< Maximum endpoint-zero packet size in bytes. */ };

/** @brief Endpoint-zero input packetizer phase. */
typedef enum {
    USB_CONTROL_PIPE_PHASE_DATA,     /**< Response data remains to be sent. */
    USB_CONTROL_PIPE_PHASE_ZERO,     /**< A zero-length data-stage packet is next. */
    USB_CONTROL_PIPE_PHASE_STALL,    /**< The terminal input STALL is ready to arm. */
    USB_CONTROL_PIPE_PHASE_COMPLETE, /**< The terminal indication was consumed. */
} UsbControlPipePhase;

/** @brief Remaining endpoint-zero response data, data-toggle, and completion state. */
typedef struct {
    UsbDescriptorView remaining; /**< Unsent response bytes. */
    bool data_one;               /**< Data toggle for the next packet. */
    UsbControlPipePhase phase;   /**< Packet or terminal action to produce next. */
} UsbControlPipe;

/** @brief One endpoint-zero response packet and its USB data toggle. */
typedef struct {
    UsbDescriptorView data; /**< Response bytes in this packet. */
    bool data_one;          /**< True when this packet uses DATA1. */
} UsbControlPacket;

/**
 * @brief Starts an endpoint-zero input data stage.
 *
 * Clips the available data to the host request, starts with DATA1, and schedules a terminating
 * zero-length packet whenever the response ends on a packet boundary, including an empty response.
 *
 * @param[out] pipe Control input packetizer to initialize.
 * @param[in] data Complete response data.
 * @param[in] requested_length Maximum response length requested by the host.
 */
void usb_control_pipe_begin(UsbControlPipe *pipe, UsbDescriptorView data,
                            uint16_t requested_length);

/**
 * @brief Produces the next endpoint-zero input packet.
 *
 * Returns consecutive chunks of at most 64 bytes, alternates DATA1 and DATA0, and emits one
 * terminating zero-length packet after an exact packet boundary, including an empty response.
 *
 * @param[in,out] pipe Remaining response and data-toggle state.
 * @param[out] packet Next response chunk and its data toggle.
 * @return True when a packet is available; otherwise false.
 */
bool usb_control_pipe_next(UsbControlPipe *pipe, UsbControlPacket *packet);

/**
 * @brief Takes the endpoint-zero terminal input STALL indication.
 *
 * Returns true once after the final short packet, including the terminating zero-length packet for
 * an empty or packet-aligned response.
 *
 * @param[in,out] pipe Control input packetizer whose terminal indication is consumed.
 * @return True when the terminal STALL should be armed; otherwise false.
 */
bool usb_control_pipe_take_terminal_stall(UsbControlPipe *pipe);

#endif
