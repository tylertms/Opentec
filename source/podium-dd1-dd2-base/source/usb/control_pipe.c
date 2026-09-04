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
    pipe->phase = data.length == 0 ? USB_CONTROL_PIPE_PHASE_ZERO : USB_CONTROL_PIPE_PHASE_DATA;
}

bool usb_control_pipe_next(UsbControlPipe *pipe, UsbControlPacket *packet) {
    if (pipe->phase == USB_CONTROL_PIPE_PHASE_STALL ||
        pipe->phase == USB_CONTROL_PIPE_PHASE_COMPLETE) {
        return false;
    }

    if (pipe->phase == USB_CONTROL_PIPE_PHASE_ZERO) {
        packet->data = (UsbDescriptorView){0};
        packet->data_one = pipe->data_one;
        pipe->data_one = !pipe->data_one;
        pipe->phase = USB_CONTROL_PIPE_PHASE_STALL;
        return true;
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
    if (length < USB_CONTROL_PACKET_SIZE) {
        pipe->phase = USB_CONTROL_PIPE_PHASE_STALL;
    } else if (pipe->remaining.length == 0) {
        pipe->phase = USB_CONTROL_PIPE_PHASE_ZERO;
    }
    return true;
}

bool usb_control_pipe_take_terminal_stall(UsbControlPipe *pipe) {
    if (pipe->phase != USB_CONTROL_PIPE_PHASE_STALL) {
        return false;
    }
    pipe->phase = USB_CONTROL_PIPE_PHASE_COMPLETE;
    return true;
}
