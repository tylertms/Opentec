#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "display/framebuffer.h"
#include "display/system_information_page.h"
#include "display/text.h"

enum { DISPLAY_ROW_BYTES = DISPLAY_FRAMEBUFFER_WIDTH / 2 };

static uint8_t pixel(const uint8_t framebuffer[DISPLAY_FRAMEBUFFER_SIZE], uint16_t x, uint16_t y) {
    uint8_t packed = framebuffer[y * DISPLAY_ROW_BYTES + x / 2];
    return (x & 1u) != 0 ? packed & 0x0fu : packed >> 4;
}

static void assert_text(const uint8_t framebuffer[DISPLAY_FRAMEBUFFER_SIZE], const char *text,
                        uint16_t x, uint16_t y, uint8_t scale) {
    uint8_t expected[DISPLAY_FRAMEBUFFER_SIZE] = {0};
    display_text_draw(expected, text, x, y, scale, 15);
    uint16_t width = display_text_width(text, scale);
    for (uint16_t row = y; row < y + 7u * scale; row++) {
        for (uint16_t column = x; column < x + width; column++) {
            assert(pixel(framebuffer, column, row) == pixel(expected, column, row));
        }
    }
}

static DisplaySystemInformation sample_information(void) {
    return (DisplaySystemInformation){
        .main_hardware = 31,
        .main_runtime_seconds = 3661,
        .motor_firmware = {0x43, 9, 10},
        .motor_hardware = 2,
        .motor_accessory_type_available = true,
        .motor_accessory_type = 5,
        .motor_runtime_seconds = 7202,
        .quick_release_firmware = 7,
        .quick_release_hardware = 6,
        .quick_release_runtime_seconds = 59,
    };
}

static void renders_the_opening_title(void) {
    uint8_t framebuffer[DISPLAY_FRAMEBUFFER_SIZE];
    memset(framebuffer, 0xff, sizeof(framebuffer));

    display_system_information_page_render_title(framebuffer);

    uint16_t x = (DISPLAY_FRAMEBUFFER_WIDTH - display_text_width("System Info Screen", 2)) / 2;
    assert_text(framebuffer, "System Info Screen", x, 25, 2);
    assert(pixel(framebuffer, 0, 0) == 0);
    assert(pixel(framebuffer, 85, 0) == 0);
}

static void renders_all_component_values(void) {
    uint8_t framebuffer[DISPLAY_FRAMEBUFFER_SIZE] = {0};
    DisplaySystemInformation information = sample_information();

    display_system_information_page_render(framebuffer, &information);

    assert_text(framebuffer, "Main:", 4, 2, 1);
    assert_text(framebuffer, "FW: v3.9.1", 4, 14, 1);
    assert_text(framebuffer, "HW: v31", 4, 25, 1);
    assert_text(framebuffer, "1h 1m 1s", 4, 53, 1);
    assert_text(framebuffer, "Motor:", 90, 2, 1);
    assert_text(framebuffer, "FW: v3.9.10", 90, 14, 1);
    assert_text(framebuffer, "HW: v2", 90, 25, 1);
    assert_text(framebuffer, "ACV: v5", 90, 36, 1);
    assert_text(framebuffer, "2h 0m 2s", 90, 53, 1);
    assert_text(framebuffer, "WQR:", 176, 2, 1);
    assert_text(framebuffer, "FW: v7", 176, 14, 1);
    assert_text(framebuffer, "HW: v6", 176, 25, 1);
    assert_text(framebuffer, "0h 0m 59s", 176, 53, 1);
    for (uint16_t y = 0; y < DISPLAY_FRAMEBUFFER_HEIGHT - 1; y++) {
        assert(pixel(framebuffer, 85, y) == 15);
        assert(pixel(framebuffer, 171, y) == 15);
    }
}

static void renders_each_accessory_type_form(void) {
    uint8_t framebuffer[DISPLAY_FRAMEBUFFER_SIZE] = {0};
    DisplaySystemInformation information = sample_information();

    information.motor_accessory_type_available = false;
    display_system_information_page_render(framebuffer, &information);
    assert_text(framebuffer, "ACV: Not sup.", 90, 36, 1);

    information.motor_accessory_type_available = true;
    information.motor_accessory_type = 0;
    display_system_information_page_render(framebuffer, &information);
    assert_text(framebuffer, "ACV: -", 90, 36, 1);

    information.motor_accessory_type = 12;
    display_system_information_page_render(framebuffer, &information);
    assert_text(framebuffer, "ACV: v12", 90, 36, 1);
}

int main(void) {
    renders_the_opening_title();
    renders_all_component_values();
    renders_each_accessory_type_form();
    return 0;
}
