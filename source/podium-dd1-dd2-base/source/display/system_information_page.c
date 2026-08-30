#include "display/system_information_page.h"

#include <stdbool.h>
#include <stdint.h>

#include "display/framebuffer.h"
#include "display/text.h"

enum {
    SYSTEM_INFORMATION_COLOR = 15,
    SYSTEM_INFORMATION_MAIN_X = 4,
    SYSTEM_INFORMATION_MOTOR_X = 90,
    SYSTEM_INFORMATION_QUICK_RELEASE_X = 176,
    SYSTEM_INFORMATION_FIRST_SEPARATOR_X = 85,
    SYSTEM_INFORMATION_SECOND_SEPARATOR_X = 171,
    SYSTEM_INFORMATION_HEADER_Y = 2,
    SYSTEM_INFORMATION_FIRMWARE_Y = 14,
    SYSTEM_INFORMATION_HARDWARE_Y = 25,
    SYSTEM_INFORMATION_ACCESSORY_Y = 36,
    SYSTEM_INFORMATION_RUNTIME_Y = 53,
    SYSTEM_INFORMATION_TITLE_Y = 25,
};

/**
 * @brief Appends fixed text to a display line.
 *
 * Copies each character and leaves the destination ready for the next field.
 *
 * @param[in,out] destination First available character in the display line.
 * @param[in] source Null-terminated text to append.
 * @return First available character after the appended text.
 */
static char *append_text(char *destination, const char *source) {
    while (*source != '\0') {
        *destination++ = *source++;
    }
    return destination;
}

/**
 * @brief Appends an unsigned decimal value to a display line.
 *
 * Emits the shortest decimal representation and emits one zero for value zero.
 *
 * @param[in,out] destination First available character in the display line.
 * @param[in] value Value to append.
 * @return First available character after the decimal digits.
 */
static char *append_decimal(char *destination, uint32_t value) {
    uint32_t divisor = 1;
    while (value / divisor >= 10u) {
        divisor *= 10u;
    }
    do {
        *destination++ = (char)('0' + value / divisor % 10u);
        divisor /= 10u;
    } while (divisor != 0);
    return destination;
}

/**
 * @brief Formats a three-component firmware version.
 *
 * Masks the first component to six bits and separates the three decimal components with dots.
 *
 * @param[out] output Null-terminated version text.
 * @param[in] version Three firmware components in display order.
 */
static void format_firmware(char output[16], const uint8_t version[3]) {
    char *cursor = append_text(output, "FW: v");
    cursor = append_decimal(cursor, version[0] & 0x3fu);
    *cursor++ = '.';
    cursor = append_decimal(cursor, version[1]);
    *cursor++ = '.';
    cursor = append_decimal(cursor, version[2]);
    *cursor = '\0';
}

/**
 * @brief Formats a decimal hardware version.
 *
 * Prefixes the hardware value with the local diagnostic label.
 *
 * @param[out] output Null-terminated hardware-version text.
 * @param[in] version Hardware version to display.
 */
static void format_hardware(char output[16], uint8_t version) {
    char *cursor = append_text(output, "HW: v");
    cursor = append_decimal(cursor, version);
    *cursor = '\0';
}

/**
 * @brief Formats elapsed operating time.
 *
 * Converts seconds to total hours, remaining minutes, and remaining seconds with the local
 * diagnostic suffixes.
 *
 * @param[out] output Null-terminated operating-time text.
 * @param[in] seconds Elapsed operating time in seconds.
 */
static void format_runtime(char output[24], uint32_t seconds) {
    char *cursor = append_decimal(output, seconds / 3600u);
    cursor = append_text(cursor, "h ");
    cursor = append_decimal(cursor, seconds / 60u % 60u);
    cursor = append_text(cursor, "m ");
    cursor = append_decimal(cursor, seconds % 60u);
    cursor = append_text(cursor, "s");
    *cursor = '\0';
}

/**
 * @brief Formats the motor accessory-type field.
 *
 * Shows an unsupported marker when no type is available, a dash for type zero, and a decimal
 * version for every nonzero type.
 *
 * @param[out] output Null-terminated accessory-type text.
 * @param[in] available True when the motor supplied an accessory type.
 * @param[in] type Accessory type supplied by the motor.
 */
static void format_accessory_type(char output[16], bool available, uint8_t type) {
    char *cursor = append_text(output, "ACV: ");
    if (!available) {
        cursor = append_text(cursor, "Not sup.");
    } else if (type == 0) {
        *cursor++ = '-';
    } else {
        *cursor++ = 'v';
        cursor = append_decimal(cursor, type);
    }
    *cursor = '\0';
}

/**
 * @brief Draws a vertical diagnostic-column separator.
 *
 * Fills one complete display column with the system-information foreground color.
 *
 * @param[in,out] framebuffer Complete local-display framebuffer.
 * @param[in] x Horizontal separator position.
 */
static void draw_separator(DisplayFramebuffer framebuffer, uint16_t x) {
    for (uint16_t y = 0; y < DISPLAY_FRAMEBUFFER_HEIGHT; y++) {
        display_framebuffer_set_pixel(framebuffer, x, y, SYSTEM_INFORMATION_COLOR);
    }
}

/**
 * @brief Renders the system-information opening title.
 *
 * Clears the previous page and centers the title presented before diagnostic content appears.
 *
 * @param[in,out] framebuffer Complete local-display framebuffer.
 */
void display_system_information_page_render_title(DisplayFramebuffer framebuffer) {
    display_framebuffer_clear(framebuffer);
    display_text_draw_centered(framebuffer, "System Info Screen", SYSTEM_INFORMATION_TITLE_Y, 2,
                               SYSTEM_INFORMATION_COLOR);
}

/**
 * @brief Renders base, motor, and wheel quick-release information.
 *
 * Presents firmware, hardware, accessory type, and elapsed operating times in three independent
 * columns after the opening title interval.
 *
 * @param[in,out] framebuffer Complete local-display framebuffer.
 * @param[in] information Current component versions and operating times.
 */
void display_system_information_page_render(DisplayFramebuffer framebuffer,
                                            const DisplaySystemInformation *information) {
    static const uint8_t main_firmware[3] = {3, 9, 1};
    char text[24];

    display_framebuffer_clear(framebuffer);
    draw_separator(framebuffer, SYSTEM_INFORMATION_FIRST_SEPARATOR_X);
    draw_separator(framebuffer, SYSTEM_INFORMATION_SECOND_SEPARATOR_X);
    display_text_draw(framebuffer, "Main:", SYSTEM_INFORMATION_MAIN_X, SYSTEM_INFORMATION_HEADER_Y,
                      1, SYSTEM_INFORMATION_COLOR);
    display_text_draw(framebuffer, "Motor:", SYSTEM_INFORMATION_MOTOR_X,
                      SYSTEM_INFORMATION_HEADER_Y, 1, SYSTEM_INFORMATION_COLOR);
    display_text_draw(framebuffer, "WQR:", SYSTEM_INFORMATION_QUICK_RELEASE_X,
                      SYSTEM_INFORMATION_HEADER_Y, 1, SYSTEM_INFORMATION_COLOR);

    format_firmware(text, main_firmware);
    display_text_draw(framebuffer, text, SYSTEM_INFORMATION_MAIN_X, SYSTEM_INFORMATION_FIRMWARE_Y,
                      1, SYSTEM_INFORMATION_COLOR);
    format_hardware(text, information->main_hardware);
    display_text_draw(framebuffer, text, SYSTEM_INFORMATION_MAIN_X, SYSTEM_INFORMATION_HARDWARE_Y,
                      1, SYSTEM_INFORMATION_COLOR);
    format_runtime(text, information->main_runtime_seconds);
    display_text_draw(framebuffer, text, SYSTEM_INFORMATION_MAIN_X, SYSTEM_INFORMATION_RUNTIME_Y, 1,
                      SYSTEM_INFORMATION_COLOR);

    format_firmware(text, information->motor_firmware);
    display_text_draw(framebuffer, text, SYSTEM_INFORMATION_MOTOR_X, SYSTEM_INFORMATION_FIRMWARE_Y,
                      1, SYSTEM_INFORMATION_COLOR);
    format_hardware(text, information->motor_hardware);
    display_text_draw(framebuffer, text, SYSTEM_INFORMATION_MOTOR_X, SYSTEM_INFORMATION_HARDWARE_Y,
                      1, SYSTEM_INFORMATION_COLOR);
    format_accessory_type(text, information->motor_accessory_type_available,
                          information->motor_accessory_type);
    display_text_draw(framebuffer, text, SYSTEM_INFORMATION_MOTOR_X, SYSTEM_INFORMATION_ACCESSORY_Y,
                      1, SYSTEM_INFORMATION_COLOR);
    format_runtime(text, information->motor_runtime_seconds);
    display_text_draw(framebuffer, text, SYSTEM_INFORMATION_MOTOR_X, SYSTEM_INFORMATION_RUNTIME_Y,
                      1, SYSTEM_INFORMATION_COLOR);

    text[0] = 'F';
    text[1] = 'W';
    text[2] = ':';
    text[3] = ' ';
    text[4] = 'v';
    char *cursor = append_decimal(text + 5, information->quick_release_firmware);
    *cursor = '\0';
    display_text_draw(framebuffer, text, SYSTEM_INFORMATION_QUICK_RELEASE_X,
                      SYSTEM_INFORMATION_FIRMWARE_Y, 1, SYSTEM_INFORMATION_COLOR);
    format_hardware(text, information->quick_release_hardware);
    display_text_draw(framebuffer, text, SYSTEM_INFORMATION_QUICK_RELEASE_X,
                      SYSTEM_INFORMATION_HARDWARE_Y, 1, SYSTEM_INFORMATION_COLOR);
    format_runtime(text, information->quick_release_runtime_seconds);
    display_text_draw(framebuffer, text, SYSTEM_INFORMATION_QUICK_RELEASE_X,
                      SYSTEM_INFORMATION_RUNTIME_Y, 1, SYSTEM_INFORMATION_COLOR);
}
