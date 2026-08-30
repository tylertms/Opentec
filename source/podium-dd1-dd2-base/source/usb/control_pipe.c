#include "usb/control_pipe.h"

#include <stdbool.h>
#include <stdint.h>

/**
 * @brief Starts an endpoint-zero input data stage.
 *
 * Clips the available data to the host request, starts with DATA1, and schedules a terminating
 * zero-length packet only when a shorter response ends on a 64-byte boundary.
 *
 * @param[out] pipe Control input packetizer to initialize.
 * @param[in] data Complete response data.
 * @param[in] requested_length Maximum response length requested by the host.
 */
void usb_control_pipe_begin(UsbControlPipe *pipe, UsbDescriptorView data,
                            uint16_t requested_length) {
    if (data.length > requested_length) {
        data.length = requested_length;
    }
    pipe->remaining = data;
    pipe->data_one = true;
    pipe->zero_pending =
        data.length < requested_length && data.length % USB_CONTROL_PACKET_SIZE == 0;
}

/**
 * @brief Produces the next endpoint-zero input packet.
 *
 * Returns consecutive chunks of at most 64 bytes, alternates DATA1 and DATA0, and emits a pending
 * terminating zero-length packet once.
 *
 * @param[in,out] pipe Remaining response and data-toggle state.
 * @param[out] packet Next response chunk and its data toggle.
 * @return True when a packet is available; otherwise false.
 */
bool usb_control_pipe_next(UsbControlPipe *pipe, UsbControlPacket *packet) {
    if (pipe->remaining.length == 0 && !pipe->zero_pending) {
        return false;
    }

    uint16_t length = pipe->remaining.length;
    if (length > USB_CONTROL_PACKET_SIZE) {
        length = USB_CONTROL_PACKET_SIZE;
    }
    packet->data.data = pipe->remaining.data;
    packet->data.length = length;
    packet->data_one = pipe->data_one;
    pipe->data_one = !pipe->data_one;
    if (length != 0) {
        pipe->remaining.data += length;
    }
    pipe->remaining.length -= length;
    if (length == 0) {
        pipe->zero_pending = false;
    }
    return true;
}
