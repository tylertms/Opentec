#include "usb/podium_report_descriptor.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct {
    uint8_t *output;
    size_t length;
} HidDescriptor;

static void append_u8(HidDescriptor *descriptor, uint8_t prefix, uint8_t value) {
    descriptor->output[descriptor->length++] = prefix;
    descriptor->output[descriptor->length++] = value;
}

static void append_u16(HidDescriptor *descriptor, uint8_t prefix, uint16_t value) {
    descriptor->output[descriptor->length++] = prefix;
    descriptor->output[descriptor->length++] = (uint8_t)value;
    descriptor->output[descriptor->length++] = (uint8_t)(value >> 8);
}

static void append_u32(HidDescriptor *descriptor, uint8_t prefix, uint32_t value) {
    descriptor->output[descriptor->length++] = prefix;
    descriptor->output[descriptor->length++] = (uint8_t)value;
    descriptor->output[descriptor->length++] = (uint8_t)(value >> 8);
    descriptor->output[descriptor->length++] = (uint8_t)(value >> 16);
    descriptor->output[descriptor->length++] = (uint8_t)(value >> 24);
}

static void usage_page(HidDescriptor *descriptor, uint16_t page) {
    if (page <= UINT8_MAX) {
        append_u8(descriptor, 0x05, (uint8_t)page);
    } else {
        append_u16(descriptor, 0x06, page);
    }
}

static void usage(HidDescriptor *descriptor, uint8_t value) { append_u8(descriptor, 0x09, value); }

static void collection(HidDescriptor *descriptor, uint8_t type) {
    append_u8(descriptor, 0xa1, type);
}

static void end_collection(HidDescriptor *descriptor) {
    descriptor->output[descriptor->length++] = 0xc0;
}

static void report_id(HidDescriptor *descriptor, uint8_t id) { append_u8(descriptor, 0x85, id); }

static void logical_minimum(HidDescriptor *descriptor, int8_t value) {
    append_u8(descriptor, 0x15, (uint8_t)value);
}

static void logical_maximum_8(HidDescriptor *descriptor, int8_t value) {
    append_u8(descriptor, 0x25, (uint8_t)value);
}

static void logical_maximum_16(HidDescriptor *descriptor, uint16_t value) {
    append_u16(descriptor, 0x26, value);
}

static void logical_maximum_32(HidDescriptor *descriptor, uint32_t value) {
    append_u32(descriptor, 0x27, value);
}

static void physical_minimum(HidDescriptor *descriptor, int8_t value) {
    append_u8(descriptor, 0x35, (uint8_t)value);
}

static void physical_maximum_8(HidDescriptor *descriptor, int8_t value) {
    append_u8(descriptor, 0x45, (uint8_t)value);
}

static void physical_maximum_16(HidDescriptor *descriptor, uint16_t value) {
    append_u16(descriptor, 0x46, value);
}

static void physical_maximum_32(HidDescriptor *descriptor, uint32_t value) {
    append_u32(descriptor, 0x47, value);
}

static void unit(HidDescriptor *descriptor, uint8_t value) { append_u8(descriptor, 0x65, value); }

static void report_size(HidDescriptor *descriptor, uint8_t bits) {
    append_u8(descriptor, 0x75, bits);
}

static void report_count(HidDescriptor *descriptor, uint8_t count) {
    append_u8(descriptor, 0x95, count);
}

static void input(HidDescriptor *descriptor) { append_u8(descriptor, 0x81, 0x02); }

static void output(HidDescriptor *descriptor) { append_u8(descriptor, 0x91, 0x02); }

static void usage_minimum(HidDescriptor *descriptor, uint8_t value) {
    append_u8(descriptor, 0x19, value);
}

static void usage_maximum(HidDescriptor *descriptor, uint8_t value) {
    append_u8(descriptor, 0x29, value);
}

static void encode_hat(HidDescriptor *descriptor, uint8_t count) {
    for (uint8_t index = 0; index < count; index++) {
        usage(descriptor, 0x39);
    }
    logical_minimum(descriptor, 0);
    logical_maximum_8(descriptor, 7);
    physical_minimum(descriptor, 0);
    physical_maximum_16(descriptor, 315);
    unit(descriptor, 0x14);
    report_size(descriptor, 4);
    report_count(descriptor, count);
    input(descriptor);
    unit(descriptor, 0);
    logical_maximum_8(descriptor, 1);
    physical_maximum_8(descriptor, 1);
}

static void encode_buttons(HidDescriptor *descriptor, uint8_t count, uint8_t maximum_usage) {
    usage_page(descriptor, 0x09);
    usage_minimum(descriptor, 1);
    usage_maximum(descriptor, maximum_usage);
    report_size(descriptor, 1);
    report_count(descriptor, count);
    input(descriptor);
}

static void encode_unsigned_axes(HidDescriptor *descriptor, const uint8_t *usages, uint8_t count) {
    usage_page(descriptor, 0x01);
    for (uint8_t index = 0; index < count; index++) {
        usage(descriptor, usages[index]);
    }
    logical_maximum_32(descriptor, UINT16_MAX);
    physical_maximum_32(descriptor, UINT16_MAX);
    report_size(descriptor, 16);
    report_count(descriptor, count);
    input(descriptor);
}

static void encode_signed_axes(HidDescriptor *descriptor, const uint8_t *usages, uint8_t count,
                               bool set_report_size) {
    for (uint8_t index = 0; index < count; index++) {
        usage(descriptor, usages[index]);
    }
    logical_minimum(descriptor, INT8_MIN);
    logical_maximum_8(descriptor, INT8_MAX);
    physical_minimum(descriptor, INT8_MIN);
    physical_maximum_8(descriptor, INT8_MAX);
    if (set_report_size) {
        report_size(descriptor, 8);
    }
    report_count(descriptor, count);
    input(descriptor);
}

static void encode_vendor_io(HidDescriptor *descriptor, uint8_t input_count, uint8_t output_count) {
    usage_page(descriptor, 0xff00);
    usage(descriptor, 1);
    report_count(descriptor, input_count);
    input(descriptor);
    usage(descriptor, 2);
    report_count(descriptor, output_count);
    output(descriptor);
}

static void encode_primary_report(HidDescriptor *descriptor) {
    static const uint8_t axes[] = {0x30, 0x32, 0x35, 0x31};
    static const uint8_t signed_axes[] = {0x33, 0x34};
    static const uint8_t encoder_axis[] = {0x37};
    report_id(descriptor, 1);
    encode_hat(descriptor, 1);
    encode_buttons(descriptor, 124, 108);
    encode_unsigned_axes(descriptor, axes, 4);
    encode_signed_axes(descriptor, signed_axes, 2, true);
    usage(descriptor, 0x36);
    logical_minimum(descriptor, 0);
    logical_maximum_16(descriptor, UINT8_MAX);
    physical_minimum(descriptor, 0);
    physical_maximum_16(descriptor, UINT8_MAX);
    report_count(descriptor, 1);
    input(descriptor);
    encode_signed_axes(descriptor, encoder_axis, 1, false);
    encode_vendor_io(descriptor, 5, 7);
}

static void encode_extended_report(HidDescriptor *descriptor) {
    static const uint8_t axes[] = {0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x36};
    static const uint8_t signed_axes[] = {0x37, 0x37, 0x37, 0x37};
    usage_page(descriptor, 0x01);
    usage(descriptor, 0x04);
    collection(descriptor, 1);
    report_id(descriptor, 2);
    encode_hat(descriptor, 4);
    encode_buttons(descriptor, 64, 63);
    encode_unsigned_axes(descriptor, axes, 8);
    encode_signed_axes(descriptor, signed_axes, 4, true);
    encode_vendor_io(descriptor, 3, 7);
    end_collection(descriptor);
}

static void encode_transfer_report(HidDescriptor *descriptor) {
    usage_page(descriptor, 0x01);
    usage(descriptor, 0x3a);
    collection(descriptor, 1);
    report_id(descriptor, 0xff);
    usage_page(descriptor, 0x01);
    usage(descriptor, 0x3b);
    logical_minimum(descriptor, 0);
    logical_maximum_16(descriptor, UINT8_MAX);
    physical_minimum(descriptor, 0);
    physical_maximum_16(descriptor, UINT8_MAX);
    report_size(descriptor, 8);
    report_count(descriptor, 63);
    input(descriptor);
    usage_page(descriptor, 0xff00);
    usage(descriptor, 1);
    report_count(descriptor, 63);
    output(descriptor);
    end_collection(descriptor);
}

size_t
usb_podium_report_descriptor_encode(uint8_t output_buffer[USB_PODIUM_REPORT_DESCRIPTOR_SIZE]) {
    static HidDescriptor descriptor;
    descriptor = (HidDescriptor){.output = output_buffer, .length = 0};
    usage_page(&descriptor, 0x01);
    usage(&descriptor, 0x04);
    collection(&descriptor, 1);
    encode_primary_report(&descriptor);
    end_collection(&descriptor);
    encode_extended_report(&descriptor);
    encode_transfer_report(&descriptor);
    return descriptor.length;
}
