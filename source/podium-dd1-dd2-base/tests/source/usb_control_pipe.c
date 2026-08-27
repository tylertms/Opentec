#include <assert.h>
#include <stdbool.h>
#include <stdint.h>

#include "usb/control_pipe.h"

static void test_splits_and_toggles_packets(void) {
    uint8_t data[130];
    UsbControlPipe pipe;
    UsbControlPacket packet;
    usb_control_pipe_begin(&pipe, (UsbDescriptorView){.data = data, .length = sizeof(data)},
                           sizeof(data));

    assert(usb_control_pipe_next(&pipe, &packet));
    assert(packet.data.data == data && packet.data.length == 64 && packet.data_one);
    assert(usb_control_pipe_next(&pipe, &packet));
    assert(packet.data.data == &data[64] && packet.data.length == 64 && !packet.data_one);
    assert(usb_control_pipe_next(&pipe, &packet));
    assert(packet.data.data == &data[128] && packet.data.length == 2 && packet.data_one);
    assert(!usb_control_pipe_next(&pipe, &packet));
}

static void test_clips_to_requested_length(void) {
    uint8_t data[64];
    UsbControlPipe pipe;
    UsbControlPacket packet;
    usb_control_pipe_begin(&pipe, (UsbDescriptorView){.data = data, .length = sizeof(data)}, 18);

    assert(usb_control_pipe_next(&pipe, &packet));
    assert(packet.data.length == 18);
    assert(!usb_control_pipe_next(&pipe, &packet));
}

static void test_adds_required_short_packet(void) {
    uint8_t data[64];
    UsbControlPipe pipe;
    UsbControlPacket packet;
    usb_control_pipe_begin(&pipe, (UsbDescriptorView){.data = data, .length = sizeof(data)}, 128);

    assert(usb_control_pipe_next(&pipe, &packet));
    assert(packet.data.length == 64);
    assert(usb_control_pipe_next(&pipe, &packet));
    assert(packet.data.length == 0 && !packet.data_one);
    assert(!usb_control_pipe_next(&pipe, &packet));
}

int main(void) {
    test_splits_and_toggles_packets();
    test_clips_to_requested_length();
    test_adds_required_short_packet();
    return 0;
}
