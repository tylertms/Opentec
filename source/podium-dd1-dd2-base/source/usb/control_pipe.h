#ifndef OPENTEC_BASE_USB_CONTROL_PIPE_H
#define OPENTEC_BASE_USB_CONTROL_PIPE_H

#include <stdbool.h>
#include <stdint.h>

#include "usb/device_control.h"

enum { USB_CONTROL_PACKET_SIZE = 64 };

typedef struct {
    UsbDescriptorView remaining;
    bool data_one;
    bool zero_pending;
} UsbControlPipe;

typedef struct {
    UsbDescriptorView data;
    bool data_one;
} UsbControlPacket;

void usb_control_pipe_begin(UsbControlPipe *pipe, UsbDescriptorView data,
                            uint16_t requested_length);
bool usb_control_pipe_next(UsbControlPipe *pipe, UsbControlPacket *packet);

#endif
