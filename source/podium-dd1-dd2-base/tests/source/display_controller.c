#include <assert.h>
#include <stddef.h>
#include <stdint.h>

#include "display/controller.h"

typedef struct {
    DisplayBusMode mode;
    uint8_t value;
} Event;

enum {
    EVENT_CAPACITY = 64,
};

typedef struct {
    Event events[EVENT_CAPACITY];
    size_t count;
} Capture;

static void capture(void *context, DisplayBusMode mode, uint8_t value) {
    Capture *output = context;
    assert(output->count < EVENT_CAPACITY);
    output->events[output->count++] = (Event){mode, value};
}

static void expect(const Capture *capture_state, const Event *expected, size_t count) {
    assert(capture_state->count == count);
    for (size_t index = 0; index < count; index++) {
        assert(capture_state->events[index].mode == expected[index].mode);
        assert(capture_state->events[index].value == expected[index].value);
    }
}

static void test_initialization_sequence(void) {
    static const Event expected[] = {
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
    Capture capture_state = {0};

    display_controller_initialize(capture, &capture_state);

    expect(&capture_state, expected, sizeof(expected) / sizeof(expected[0]));
}

static void test_frame_window_sequence(void) {
    static const Event expected[] = {
        {DISPLAY_BUS_COMMAND, 0x15}, {DISPLAY_BUS_DATA, 0x1c}, {DISPLAY_BUS_DATA, 0x5b},
        {DISPLAY_BUS_COMMAND, 0x75}, {DISPLAY_BUS_DATA, 0x00}, {DISPLAY_BUS_DATA, 0x3f},
        {DISPLAY_BUS_COMMAND, 0x5c},
    };
    Capture capture_state = {0};

    display_controller_begin_frame(capture, &capture_state);

    expect(&capture_state, expected, sizeof(expected) / sizeof(expected[0]));
}

int main(void) {
    test_initialization_sequence();
    test_frame_window_sequence();
    return 0;
}
