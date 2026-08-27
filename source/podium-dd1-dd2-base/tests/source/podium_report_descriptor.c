#include "usb/podium_report_descriptor.h"

#include <assert.h>
#include <stddef.h>
#include <stdint.h>

enum {
    HID_ITEM_INPUT = 8,
    HID_ITEM_OUTPUT = 9,
    HID_ITEM_REPORT_SIZE = 7,
    HID_ITEM_REPORT_ID = 8,
    HID_ITEM_REPORT_COUNT = 9,
    HID_ITEM_MAIN = 0,
    HID_ITEM_GLOBAL = 1,
};

static uint32_t item_value(const uint8_t *data, size_t size) {
    uint32_t value = 0;
    for (size_t index = 0; index < size; index++) {
        value |= (uint32_t)data[index] << (index * 8);
    }
    return value;
}

static uint32_t report_bits(const uint8_t *descriptor, size_t length, uint8_t report,
                            uint8_t main_tag) {
    uint8_t report_id = 0;
    uint32_t report_size = 0;
    uint32_t report_count = 0;
    uint32_t bits = 0;

    for (size_t offset = 0; offset < length;) {
        uint8_t prefix = descriptor[offset++];
        size_t size = prefix & 3;
        if (size == 3) {
            size = 4;
        }
        uint8_t type = (prefix >> 2) & 3;
        uint8_t tag = prefix >> 4;
        uint32_t value = item_value(&descriptor[offset], size);
        offset += size;

        if (type == HID_ITEM_GLOBAL && tag == HID_ITEM_REPORT_SIZE) {
            report_size = value;
        } else if (type == HID_ITEM_GLOBAL && tag == HID_ITEM_REPORT_ID) {
            report_id = (uint8_t)value;
        } else if (type == HID_ITEM_GLOBAL && tag == HID_ITEM_REPORT_COUNT) {
            report_count = value;
        } else if (type == HID_ITEM_MAIN && tag == main_tag && report_id == report) {
            bits += report_size * report_count;
        }
    }
    return bits;
}

static void test_report_layouts(void) {
    uint8_t descriptor[USB_PODIUM_REPORT_DESCRIPTOR_SIZE];
    size_t length = usb_podium_report_descriptor_encode(descriptor);

    assert(length == USB_PODIUM_REPORT_DESCRIPTOR_SIZE);
    assert(report_bits(descriptor, length, 1, HID_ITEM_INPUT) == 264);
    assert(report_bits(descriptor, length, 1, HID_ITEM_OUTPUT) == 56);
    assert(report_bits(descriptor, length, 2, HID_ITEM_INPUT) == 264);
    assert(report_bits(descriptor, length, 2, HID_ITEM_OUTPUT) == 56);
    assert(report_bits(descriptor, length, 0xff, HID_ITEM_INPUT) == 504);
    assert(report_bits(descriptor, length, 0xff, HID_ITEM_OUTPUT) == 504);
}

static void test_top_level_collections(void) {
    uint8_t descriptor[USB_PODIUM_REPORT_DESCRIPTOR_SIZE];
    size_t length = usb_podium_report_descriptor_encode(descriptor);

    assert(descriptor[0] == 0x05 && descriptor[1] == 0x01);
    assert(descriptor[2] == 0x09 && descriptor[3] == 0x04);
    assert(descriptor[4] == 0xa1 && descriptor[5] == 0x01);
    assert(descriptor[length - 1] == 0xc0);
}

int main(void) {
    test_report_layouts();
    test_top_level_collections();
    return 0;
}
