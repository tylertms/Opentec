#include "usb/podium_report_descriptor.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/** @brief Cursor and destination storage for HID descriptor encoding. */
typedef struct {
    uint8_t *output; /**< Caller-owned descriptor destination. */
    size_t length;   /**< Number of descriptor bytes currently written. */
} HidDescriptor;

/**
 * @brief Appends an HID item with an eight-bit value.
 *
 * Writes the item prefix followed by its one-byte payload and advances the descriptor length.
 *
 * @param[in,out] descriptor Descriptor under construction.
 * @param[in] prefix HID short-item prefix.
 * @param[in] value Item payload.
 */
static void append_u8(HidDescriptor *descriptor, uint8_t prefix, uint8_t value) {
    descriptor->output[descriptor->length++] = prefix;
    descriptor->output[descriptor->length++] = value;
}

/**
 * @brief Appends an HID item with a sixteen-bit value.
 *
 * Writes the item prefix followed by its little-endian payload and advances the descriptor length.
 *
 * @param[in,out] descriptor Descriptor under construction.
 * @param[in] prefix HID short-item prefix.
 * @param[in] value Item payload.
 */
static void append_u16(HidDescriptor *descriptor, uint8_t prefix, uint16_t value) {
    descriptor->output[descriptor->length++] = prefix;
    descriptor->output[descriptor->length++] = (uint8_t)value;
    descriptor->output[descriptor->length++] = (uint8_t)(value >> 8);
}

/**
 * @brief Appends an HID item with a thirty-two-bit value.
 *
 * Writes the item prefix followed by its little-endian payload and advances the descriptor length.
 *
 * @param[in,out] descriptor Descriptor under construction.
 * @param[in] prefix HID short-item prefix.
 * @param[in] value Item payload.
 */
static void append_u32(HidDescriptor *descriptor, uint8_t prefix, uint32_t value) {
    descriptor->output[descriptor->length++] = prefix;
    descriptor->output[descriptor->length++] = (uint8_t)value;
    descriptor->output[descriptor->length++] = (uint8_t)(value >> 8);
    descriptor->output[descriptor->length++] = (uint8_t)(value >> 16);
    descriptor->output[descriptor->length++] = (uint8_t)(value >> 24);
}

/**
 * @brief Appends a usage-page item.
 *
 * Selects the compact eight-bit item when the page identifier fits in one byte and the sixteen-bit
 * item otherwise.
 *
 * @param[in,out] descriptor Descriptor under construction.
 * @param[in] page HID usage-page identifier.
 */
static void usage_page(HidDescriptor *descriptor, uint16_t page) {
    if (page <= UINT8_MAX) {
        append_u8(descriptor, 0x05, (uint8_t)page);
    } else {
        append_u16(descriptor, 0x06, page);
    }
}

/**
 * @brief Appends an eight-bit usage item.
 *
 * Selects one usage within the current usage page.
 *
 * @param[in,out] descriptor Descriptor under construction.
 * @param[in] value HID usage identifier.
 */
static void usage(HidDescriptor *descriptor, uint8_t value) { append_u8(descriptor, 0x09, value); }

/**
 * @brief Opens an HID collection.
 *
 * Appends the requested collection type to the descriptor.
 *
 * @param[in,out] descriptor Descriptor under construction.
 * @param[in] type HID collection type.
 */
static void collection(HidDescriptor *descriptor, uint8_t type) {
    append_u8(descriptor, 0xa1, type);
}

/**
 * @brief Closes the current HID collection.
 *
 * Appends the single-byte end-collection item.
 *
 * @param[in,out] descriptor Descriptor under construction.
 */
static void end_collection(HidDescriptor *descriptor) {
    descriptor->output[descriptor->length++] = 0xc0;
}

/**
 * @brief Selects an HID report identifier.
 *
 * Appends the identifier used by subsequent main items.
 *
 * @param[in,out] descriptor Descriptor under construction.
 * @param[in] id Report identifier.
 */
static void report_id(HidDescriptor *descriptor, uint8_t id) { append_u8(descriptor, 0x85, id); }

/**
 * @brief Sets an eight-bit logical minimum.
 *
 * Appends the lower logical bound for subsequent fields.
 *
 * @param[in,out] descriptor Descriptor under construction.
 * @param[in] value Signed logical minimum.
 */
static void logical_minimum(HidDescriptor *descriptor, int8_t value) {
    append_u8(descriptor, 0x15, (uint8_t)value);
}

/**
 * @brief Sets an eight-bit logical maximum.
 *
 * Appends the upper logical bound for subsequent fields.
 *
 * @param[in,out] descriptor Descriptor under construction.
 * @param[in] value Signed logical maximum.
 */
static void logical_maximum_8(HidDescriptor *descriptor, int8_t value) {
    append_u8(descriptor, 0x25, (uint8_t)value);
}

/**
 * @brief Sets a sixteen-bit logical maximum.
 *
 * Appends the upper logical bound for subsequent fields.
 *
 * @param[in,out] descriptor Descriptor under construction.
 * @param[in] value Unsigned logical maximum.
 */
static void logical_maximum_16(HidDescriptor *descriptor, uint16_t value) {
    append_u16(descriptor, 0x26, value);
}

/**
 * @brief Sets a thirty-two-bit logical maximum.
 *
 * Appends the upper logical bound for subsequent fields.
 *
 * @param[in,out] descriptor Descriptor under construction.
 * @param[in] value Unsigned logical maximum.
 */
static void logical_maximum_32(HidDescriptor *descriptor, uint32_t value) {
    append_u32(descriptor, 0x27, value);
}

/**
 * @brief Sets an eight-bit physical minimum.
 *
 * Appends the lower physical bound for subsequent fields.
 *
 * @param[in,out] descriptor Descriptor under construction.
 * @param[in] value Signed physical minimum.
 */
static void physical_minimum(HidDescriptor *descriptor, int8_t value) {
    append_u8(descriptor, 0x35, (uint8_t)value);
}

/**
 * @brief Sets an eight-bit physical maximum.
 *
 * Appends the upper physical bound for subsequent fields.
 *
 * @param[in,out] descriptor Descriptor under construction.
 * @param[in] value Signed physical maximum.
 */
static void physical_maximum_8(HidDescriptor *descriptor, int8_t value) {
    append_u8(descriptor, 0x45, (uint8_t)value);
}

/**
 * @brief Sets a sixteen-bit physical maximum.
 *
 * Appends the upper physical bound for subsequent fields.
 *
 * @param[in,out] descriptor Descriptor under construction.
 * @param[in] value Unsigned physical maximum.
 */
static void physical_maximum_16(HidDescriptor *descriptor, uint16_t value) {
    append_u16(descriptor, 0x46, value);
}

/**
 * @brief Sets a thirty-two-bit physical maximum.
 *
 * Appends the upper physical bound for subsequent fields.
 *
 * @param[in,out] descriptor Descriptor under construction.
 * @param[in] value Unsigned physical maximum.
 */
static void physical_maximum_32(HidDescriptor *descriptor, uint32_t value) {
    append_u32(descriptor, 0x47, value);
}

/**
 * @brief Selects the unit for subsequent fields.
 *
 * Appends the compact HID unit item.
 *
 * @param[in,out] descriptor Descriptor under construction.
 * @param[in] value Encoded HID unit.
 */
static void unit(HidDescriptor *descriptor, uint8_t value) { append_u8(descriptor, 0x65, value); }

/**
 * @brief Sets the width of subsequent report fields.
 *
 * Appends the field width in bits.
 *
 * @param[in,out] descriptor Descriptor under construction.
 * @param[in] bits Bits in each field.
 */
static void report_size(HidDescriptor *descriptor, uint8_t bits) {
    append_u8(descriptor, 0x75, bits);
}

/**
 * @brief Sets the number of subsequent report fields.
 *
 * Appends the number of fields consumed by the next main item.
 *
 * @param[in,out] descriptor Descriptor under construction.
 * @param[in] count Number of fields.
 */
static void report_count(HidDescriptor *descriptor, uint8_t count) {
    append_u8(descriptor, 0x95, count);
}

/**
 * @brief Appends a data input item.
 *
 * Declares the current fields as variable, absolute input data.
 *
 * @param[in,out] descriptor Descriptor under construction.
 */
static void input(HidDescriptor *descriptor) { append_u8(descriptor, 0x81, 0x02); }

/**
 * @brief Appends a data output item.
 *
 * Declares the current fields as variable, absolute output data.
 *
 * @param[in,out] descriptor Descriptor under construction.
 */
static void output(HidDescriptor *descriptor) { append_u8(descriptor, 0x91, 0x02); }

/**
 * @brief Sets the first usage in a range.
 *
 * Appends an eight-bit usage minimum.
 *
 * @param[in,out] descriptor Descriptor under construction.
 * @param[in] value First usage identifier.
 */
static void usage_minimum(HidDescriptor *descriptor, uint8_t value) {
    append_u8(descriptor, 0x19, value);
}

/**
 * @brief Sets the final usage in a range.
 *
 * Appends an eight-bit usage maximum.
 *
 * @param[in,out] descriptor Descriptor under construction.
 * @param[in] value Final usage identifier.
 */
static void usage_maximum(HidDescriptor *descriptor, uint8_t value) {
    append_u8(descriptor, 0x29, value);
}

/**
 * @brief Encodes one or more directional hats.
 *
 * Declares four-bit hats with eight directions, a 315-degree physical maximum, and angular units,
 * then restores neutral unit and one-valued bounds for subsequent fields.
 *
 * @param[in,out] descriptor Descriptor under construction.
 * @param[in] count Number of hat fields.
 */
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

/**
 * @brief Encodes a packed button field.
 *
 * Selects the button usage page and emits the requested one-bit field count and usage range.
 *
 * @param[in,out] descriptor Descriptor under construction.
 * @param[in] count Number of button bits.
 * @param[in] maximum_usage Final advertised button usage.
 */
static void encode_buttons(HidDescriptor *descriptor, uint8_t count, uint8_t maximum_usage) {
    usage_page(descriptor, 0x09);
    usage_minimum(descriptor, 1);
    usage_maximum(descriptor, maximum_usage);
    report_size(descriptor, 1);
    report_count(descriptor, count);
    input(descriptor);
}

/**
 * @brief Encodes unsigned sixteen-bit axes.
 *
 * Selects the Generic Desktop page and declares the requested usages across the full unsigned
 * sixteen-bit logical and physical range.
 *
 * @param[in,out] descriptor Descriptor under construction.
 * @param[in] usages Axis usage identifiers.
 * @param[in] count Number of axes.
 */
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

/**
 * @brief Encodes signed eight-bit axes.
 *
 * Declares the requested usages from minus 128 through 127 and optionally sets the report width to
 * eight bits before emitting the input item.
 *
 * @param[in,out] descriptor Descriptor under construction.
 * @param[in] usages Axis usage identifiers.
 * @param[in] count Number of axes.
 * @param[in] set_report_size True to set an eight-bit field width.
 */
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

/**
 * @brief Encodes vendor-defined report payloads.
 *
 * Declares usage one as input and usage two as output with independent byte counts.
 *
 * @param[in,out] descriptor Descriptor under construction.
 * @param[in] input_count Number of vendor input bytes.
 * @param[in] output_count Number of vendor output bytes.
 */
static void encode_vendor_io(HidDescriptor *descriptor, uint8_t input_count, uint8_t output_count) {
    usage_page(descriptor, 0xff00);
    usage(descriptor, 1);
    report_count(descriptor, input_count);
    input(descriptor);
    usage(descriptor, 2);
    report_count(descriptor, output_count);
    output(descriptor);
}

/**
 * @brief Encodes the native primary wheel report.
 *
 * Builds report one with one hat, 124 button bits, four unsigned axes, three signed axes, one
 * unsigned byte, five vendor input bytes, and seven vendor output bytes.
 *
 * @param[in,out] descriptor Descriptor under construction.
 */
static void encode_primary_report(HidDescriptor *descriptor) {
    /** @brief Generic Desktop usages for primary unsigned axes. */
    static const uint8_t axes[] = {0x30, 0x32, 0x35, 0x31};
    /** @brief Generic Desktop usages for primary signed axes. */
    static const uint8_t signed_axes[] = {0x33, 0x34};
    /** @brief Generic Desktop usage for the primary encoder axis. */
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

/**
 * @brief Encodes the native extended wheel report.
 *
 * Builds report two in its own application collection with four hats, 64 button bits, eight
 * unsigned axes, four signed axes, three vendor input bytes, and seven vendor output bytes.
 *
 * @param[in,out] descriptor Descriptor under construction.
 */
static void encode_extended_report(HidDescriptor *descriptor) {
    /** @brief Generic Desktop usages for extended unsigned axes. */
    static const uint8_t axes[] = {0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x36};
    /** @brief Generic Desktop usages for extended signed axes. */
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

/**
 * @brief Encodes the bidirectional transfer report.
 *
 * Builds report 255 in its own application collection with 63 input bytes and 63 output bytes.
 *
 * @param[in,out] descriptor Descriptor under construction.
 */
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
    /** @brief Reusable descriptor cursor storage. */
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
