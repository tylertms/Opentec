#include "usb/control_pipe.h"

#include <stdbool.h>
#include <stdint.h>

void usb_control_pipe_begin(UsbControlPipe *pipe, UsbDescriptorView data,
                            uint16_t requested_length) {
    if (data.length > requested_length) {
        data.length = requested_length;
    }
    pipe->remaining = data;
    pipe->data_one = true;
    pipe->zero_pending = data.length == 0 || data.length % USB_CONTROL_PACKET_SIZE == 0;
}

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
