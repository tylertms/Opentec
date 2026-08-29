#include "display/notice.h"

#include <stdbool.h>
#include <stdint.h>

#include "display/framebuffer.h"
#include "display/text.h"
#include "system/notice.h"

enum {
    NOTICE_COLOR = 15,
    WARNING_ICON_X = 123,
    WARNING_ICON_Y = 17,
    WARNING_ICON_WIDTH = 11,
    WARNING_ICON_HEIGHT = 10,
    NOTICE_TEXT_Y = 37,
    NOTICE_PRIMARY_TEXT_Y = 30,
    NOTICE_SECONDARY_TEXT_Y = 40,
};

static const char torque_disabled_text[] = "Torque disabled by Powerbutton";
static const char position_sensor_started_text[] = "Position Sensor Test Started.";
static const char position_sensor_failed_text[] = "Position Sensor Test Failed!";
static const char torque_reduced_primary_text[] = "NOTE: Torque is reduced to prevent damage";
static const char torque_reduced_secondary_text[] = "  on the simplified quick-release system.";

/**
 * @brief Draws the warning icon used by persistent display notices.
 *
 * Renders an eleven-by-ten triangular warning mark at the notice icon position.
 *
 * @param[in,out] framebuffer Complete local-display framebuffer.
 */
static void draw_warning_icon(uint8_t framebuffer[DISPLAY_FRAMEBUFFER_SIZE]) {
    uint16_t center = WARNING_ICON_X + WARNING_ICON_WIDTH / 2;
    for (uint16_t row = 0; row < WARNING_ICON_HEIGHT - 1; row++) {
        uint16_t half_width = (row + 1) / 2;
        display_framebuffer_set_pixel(framebuffer, (uint16_t)(center - half_width),
                                      (uint16_t)(WARNING_ICON_Y + row), NOTICE_COLOR);
        display_framebuffer_set_pixel(framebuffer, (uint16_t)(center + half_width),
                                      (uint16_t)(WARNING_ICON_Y + row), NOTICE_COLOR);
    }
    for (uint16_t column = 0; column < WARNING_ICON_WIDTH; column++) {
        display_framebuffer_set_pixel(framebuffer, (uint16_t)(WARNING_ICON_X + column),
                                      WARNING_ICON_Y + WARNING_ICON_HEIGHT - 1, NOTICE_COLOR);
    }
    for (uint16_t row = 3; row < 7; row++) {
        display_framebuffer_set_pixel(framebuffer, center, (uint16_t)(WARNING_ICON_Y + row),
                                      NOTICE_COLOR);
    }
    display_framebuffer_set_pixel(framebuffer, center, WARNING_ICON_Y + 8, NOTICE_COLOR);
}

/**
 * @brief Draws the error icon used by persistent failure notices.
 *
 * Renders an eleven-by-ten outlined mark with crossing diagonals at the shared notice position.
 *
 * @param[in,out] framebuffer Complete local-display framebuffer.
 */
static void draw_error_icon(uint8_t framebuffer[DISPLAY_FRAMEBUFFER_SIZE]) {
    for (uint16_t column = 0; column < WARNING_ICON_WIDTH; column++) {
        display_framebuffer_set_pixel(framebuffer, (uint16_t)(WARNING_ICON_X + column),
                                      WARNING_ICON_Y, NOTICE_COLOR);
        display_framebuffer_set_pixel(framebuffer, (uint16_t)(WARNING_ICON_X + column),
                                      WARNING_ICON_Y + WARNING_ICON_HEIGHT - 1, NOTICE_COLOR);
    }
    for (uint16_t row = 0; row < WARNING_ICON_HEIGHT; row++) {
        display_framebuffer_set_pixel(framebuffer, WARNING_ICON_X, (uint16_t)(WARNING_ICON_Y + row),
                                      NOTICE_COLOR);
        display_framebuffer_set_pixel(framebuffer, WARNING_ICON_X + WARNING_ICON_WIDTH - 1,
                                      (uint16_t)(WARNING_ICON_Y + row), NOTICE_COLOR);
        display_framebuffer_set_pixel(framebuffer, (uint16_t)(WARNING_ICON_X + row + 1),
                                      (uint16_t)(WARNING_ICON_Y + row), NOTICE_COLOR);
        display_framebuffer_set_pixel(framebuffer,
                                      (uint16_t)(WARNING_ICON_X + WARNING_ICON_WIDTH - row - 2),
                                      (uint16_t)(WARNING_ICON_Y + row), NOTICE_COLOR);
    }
}

/**
 * @brief Renders the persistent power-button torque-disabled notice.
 *
 * Clears the display, then shows the centered warning icon and exact torque-disabled message while
 * the notice is visible.
 *
 * @param[out] framebuffer Complete local-display framebuffer.
 * @param[in] visible True while the torque-disabled notice owns the display.
 */
void display_notice_render_torque_disabled(uint8_t framebuffer[DISPLAY_FRAMEBUFFER_SIZE],
                                           bool visible) {
    display_framebuffer_clear(framebuffer);
    if (!visible) {
        return;
    }
    draw_warning_icon(framebuffer);
    display_text_draw_centered(framebuffer, torque_disabled_text, NOTICE_TEXT_Y, 1, NOTICE_COLOR);
}

/**
 * @brief Renders a motor-originated system notice.
 *
 * Clears the display, selects the warning or error icon, and lays out the exact one-line or
 * two-line operator message associated with the notice.
 *
 * @param[out] framebuffer Complete local-display framebuffer.
 * @param[in] kind Active system notice kind.
 */
void display_notice_render_system(uint8_t framebuffer[DISPLAY_FRAMEBUFFER_SIZE],
                                  SystemNoticeKind kind) {
    display_framebuffer_clear(framebuffer);
    if (kind == SYSTEM_NOTICE_NONE) {
        return;
    }

    if (kind == SYSTEM_NOTICE_POSITION_SENSOR_TEST_FAILED) {
        draw_error_icon(framebuffer);
        display_text_draw_centered(framebuffer, position_sensor_failed_text, NOTICE_TEXT_Y, 1,
                                   NOTICE_COLOR);
    } else if (kind == SYSTEM_NOTICE_POSITION_SENSOR_TEST_STARTED) {
        draw_warning_icon(framebuffer);
        display_text_draw_centered(framebuffer, position_sensor_started_text, NOTICE_TEXT_Y, 1,
                                   NOTICE_COLOR);
    } else if (kind == SYSTEM_NOTICE_TORQUE_REDUCED) {
        draw_warning_icon(framebuffer);
        display_text_draw_centered(framebuffer, torque_reduced_primary_text, NOTICE_PRIMARY_TEXT_Y,
                                   1, NOTICE_COLOR);
        display_text_draw_centered(framebuffer, torque_reduced_secondary_text,
                                   NOTICE_SECONDARY_TEXT_Y, 1, NOTICE_COLOR);
    }
}
