#include "display/icon_bitmaps.h"

#include <stdint.h>

#include "display/bitmap.h"

enum {
    DISPLAY_ICON_SEGMENT_TOP = 1u << 0,
    DISPLAY_ICON_SEGMENT_UPPER_RIGHT = 1u << 1,
    DISPLAY_ICON_SEGMENT_LOWER_RIGHT = 1u << 2,
    DISPLAY_ICON_SEGMENT_BOTTOM = 1u << 3,
    DISPLAY_ICON_SEGMENT_LOWER_LEFT = 1u << 4,
    DISPLAY_ICON_SEGMENT_UPPER_LEFT = 1u << 5,
    DISPLAY_ICON_SEGMENT_MIDDLE = 1u << 6,
    DISPLAY_ICON_DECIMAL_POINT = 1u << 7,
    DISPLAY_ICON_COLOR = 15,
};

static const uint8_t horizontal_marker_pixels[] = {
    0x00, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x00,
    0x0f, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xf0,
    0x00, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x00,
};

static const uint8_t vertical_marker_pixels[] = {
    0x00, 0xf0, 0x0f, 0xff, 0x0f, 0xff, 0x0f, 0xff, 0x0f, 0xff, 0x0f, 0xff, 0x0f, 0xff,
    0x0f, 0xff, 0x0f, 0xff, 0x0f, 0xff, 0x0f, 0xff, 0x0f, 0xff, 0x0f, 0xff, 0x00, 0xf0,
};

static const uint8_t square_marker_pixels[] = {
    0x0f, 0xf0, 0xff, 0xff, 0xff, 0xff, 0x0f, 0xf0,
};

static void draw_bitmap(DisplayBitmapQueue *queue, const uint8_t *pixels, uint16_t x, uint16_t y,
                        uint16_t width, uint16_t height) {
    display_bitmap_queue_add(queue, pixels, x, y, width, height, false, DISPLAY_ICON_COLOR);
}

void display_icon_group_draw_queued(DisplayBitmapQueue *queue, uint16_t x, uint16_t y,
                                    uint8_t segments) {
    if ((segments & DISPLAY_ICON_SEGMENT_MIDDLE) != 0) {
        draw_bitmap(queue, horizontal_marker_pixels, (uint16_t)(x + 3), (uint16_t)(y + 17),
                    16, 3);
    }
    if ((segments & DISPLAY_ICON_SEGMENT_TOP) != 0) {
        draw_bitmap(queue, horizontal_marker_pixels, (uint16_t)(x + 3), y, 16, 3);
    }
    if ((segments & DISPLAY_ICON_SEGMENT_UPPER_RIGHT) != 0) {
        draw_bitmap(queue, vertical_marker_pixels, (uint16_t)(x + 16), (uint16_t)(y + 3),
                    4, 14);
    }
    if ((segments & DISPLAY_ICON_SEGMENT_LOWER_RIGHT) != 0) {
        draw_bitmap(queue, vertical_marker_pixels, (uint16_t)(x + 16), (uint16_t)(y + 20),
                    4, 14);
    }
    if ((segments & DISPLAY_ICON_SEGMENT_BOTTOM) != 0) {
        draw_bitmap(queue, horizontal_marker_pixels, (uint16_t)(x + 3), (uint16_t)(y + 34),
                    16, 3);
    }
    if ((segments & DISPLAY_ICON_SEGMENT_LOWER_LEFT) != 0) {
        draw_bitmap(queue, vertical_marker_pixels, (uint16_t)(x + 1), (uint16_t)(y + 20),
                    4, 14);
    }
    if ((segments & DISPLAY_ICON_SEGMENT_UPPER_LEFT) != 0) {
        draw_bitmap(queue, vertical_marker_pixels, (uint16_t)(x + 1), (uint16_t)(y + 3), 4,
                    14);
    }
    if ((segments & DISPLAY_ICON_DECIMAL_POINT) != 0) {
        draw_bitmap(queue, square_marker_pixels, (uint16_t)(x + 21), (uint16_t)(y + 33), 4,
                    4);
    }
}
