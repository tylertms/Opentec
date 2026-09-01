#include "display/controller.h"

#include <stddef.h>
#include <stdint.h>

/**
 * @brief Stores one display-controller bus event.
 *
 * Each event identifies the bus mode and byte emitted in a controller command sequence.
 */
typedef struct {
    DisplayBusMode mode; /**< Command or data mode for the emitted byte. */
    uint8_t value;       /**< Byte emitted on the display bus. */
} DisplayBusEvent;

/**
 * @brief Contains the display-controller initialization event sequence.
 *
 * The events configure the panel before the first framebuffer transfer.
 */
static const DisplayBusEvent initialization[] = {
    {DISPLAY_BUS_COMMAND, 0xfd}, {DISPLAY_BUS_DATA, 0x12},    {DISPLAY_BUS_COMMAND, 0xae},
    {DISPLAY_BUS_COMMAND, 0xb3}, {DISPLAY_BUS_DATA, 0x91},    {DISPLAY_BUS_COMMAND, 0xca},
    {DISPLAY_BUS_DATA, 0x3f},    {DISPLAY_BUS_COMMAND, 0xa2}, {DISPLAY_BUS_DATA, 0x00},
    {DISPLAY_BUS_COMMAND, 0xa1}, {DISPLAY_BUS_DATA, 0x00},    {DISPLAY_BUS_COMMAND, 0x15},
    {DISPLAY_BUS_DATA, 0x1c},    {DISPLAY_BUS_DATA, 0x5b},    {DISPLAY_BUS_COMMAND, 0x75},
    {DISPLAY_BUS_DATA, 0x00},    {DISPLAY_BUS_DATA, 0x3f},    {DISPLAY_BUS_COMMAND, 0xa0},
    {DISPLAY_BUS_DATA, 0x06},    {DISPLAY_BUS_DATA, 0x11},    {DISPLAY_BUS_COMMAND, 0xab},
    {DISPLAY_BUS_DATA, 0x01},    {DISPLAY_BUS_COMMAND, 0xb4}, {DISPLAY_BUS_DATA, 0xa0},
    {DISPLAY_BUS_DATA, 0xfd},    {DISPLAY_BUS_COMMAND, 0xc1}, {DISPLAY_BUS_DATA, 0x70},
    {DISPLAY_BUS_COMMAND, 0xc7}, {DISPLAY_BUS_DATA, 0x0f},    {DISPLAY_BUS_COMMAND, 0xb9},
    {DISPLAY_BUS_COMMAND, 0x00}, {DISPLAY_BUS_COMMAND, 0xb1}, {DISPLAY_BUS_DATA, 0xff},
    {DISPLAY_BUS_COMMAND, 0xd1}, {DISPLAY_BUS_DATA, 0x82},    {DISPLAY_BUS_DATA, 0x20},
    {DISPLAY_BUS_COMMAND, 0xbb}, {DISPLAY_BUS_DATA, 0x17},    {DISPLAY_BUS_COMMAND, 0xb6},
    {DISPLAY_BUS_DATA, 0x01},    {DISPLAY_BUS_COMMAND, 0xbe}, {DISPLAY_BUS_DATA, 0x07},
    {DISPLAY_BUS_COMMAND, 0xa6}, {DISPLAY_BUS_COMMAND, 0xaf}, {DISPLAY_BUS_COMMAND, 0x5c},
};

/**
 * @brief Contains the event sequence that begins one framebuffer transfer.
 *
 * The events select the panel window and enter display-RAM write mode.
 */
static const DisplayBusEvent begin_frame[] = {
    {DISPLAY_BUS_COMMAND, 0x15}, {DISPLAY_BUS_DATA, 0x1c}, {DISPLAY_BUS_DATA, 0x5b},
    {DISPLAY_BUS_COMMAND, 0x75}, {DISPLAY_BUS_DATA, 0x00}, {DISPLAY_BUS_DATA, 0x3f},
    {DISPLAY_BUS_COMMAND, 0x5c},
};

/**
 * @brief Sends an ordered display bus event sequence.
 *
 * Passes every command or data byte to the supplied bus writer without changing the required
 * ordering or mode assignments.
 *
 * @param[in] events Ordered command and data events to send.
 * @param[in] count Number of events in the sequence.
 * @param[in] write Display bus byte writer.
 * @param[in,out] context Opaque state passed to the byte writer.
 */
static void write_events(const DisplayBusEvent *events, size_t count, DisplayBusWrite write,
                         void *context) {
    for (size_t index = 0; index < count; index++) {
        write(context, events[index].mode, events[index].value);
    }
}

/**
 * @brief Sends the controller initialization sequence for the base display.
 *
 * Configures the 64-row grayscale panel, its column window, remapping, contrast, timing, and power
 * state, then selects display RAM for subsequent pixel data.
 *
 * @param[in] write Byte writer for the display command/data bus.
 * @param[in,out] context Opaque state passed to the byte writer.
 */
void display_controller_initialize(DisplayBusWrite write, void *context) {
    write_events(initialization, sizeof(initialization) / sizeof(initialization[0]), write,
                 context);
}

/**
 * @brief Selects the complete display window for a framebuffer transfer.
 *
 * Selects columns 28 through 91 and rows 0 through 63, then enters display-RAM write mode.
 *
 * @param[in] write Byte writer for the display command/data bus.
 * @param[in,out] context Opaque state passed to the byte writer.
 */
void display_controller_begin_frame(DisplayBusWrite write, void *context) {
    write_events(begin_frame, sizeof(begin_frame) / sizeof(begin_frame[0]), write, context);
}
