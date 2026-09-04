#include "display/notice.h"

#include <stdbool.h>
#include <stdint.h>

#include "display/bitmap.h"
#include "display/framebuffer.h"
#include "display/high_torque_icon.h"
#include "display/text.h"
#include "system/notice.h"

/**
 * @brief Defines notice colors, icon geometry, and text positions.
 *
 * The constants place notice graphics and one- or two-line messages in the local display
 * framebuffer.
 */
enum {
    NOTICE_COLOR = 15,          /**< Foreground grayscale value for notice content. */
    WARNING_ICON_X = 123,       /**< Warning-icon left coordinate. */
    WARNING_ICON_Y = 17,        /**< Warning-icon top coordinate. */
    HIGH_TORQUE_ICON_Y = 16,    /**< High-torque icon top coordinate. */
    WARNING_ICON_WIDTH = 11,    /**< Warning-icon width in pixels. */
    WARNING_ICON_HEIGHT = 10,   /**< Warning-icon height in pixels. */
    OVERLAY_LEFT = 2,           /**< Filled overlay left coordinate. */
    OVERLAY_TOP = 16,           /**< Filled overlay top coordinate. */
    OVERLAY_RIGHT = 253,        /**< Exclusive filled-overlay right endpoint. */
    OVERLAY_BOTTOM = 63,        /**< Exclusive filled-overlay bottom endpoint. */
    NOTICE_CIRCLE_X = 128,      /**< Notice circle center column. */
    NOTICE_CIRCLE_Y = 21,       /**< Notice circle center row. */
    NOTICE_CIRCLE_RADIUS = 8,   /**< Notice circle radius. */
    NOTICE_TEXT_Y = 37,         /**< Vertical coordinate for one-line notice text. */
    NOTICE_PRIMARY_TEXT_Y = 30, /**< Vertical coordinate for the first line of a two-line notice. */
    NOTICE_SECONDARY_TEXT_Y =
        40, /**< Vertical coordinate for the second line of a two-line notice. */
};

/** @brief Text displayed when the power button disables torque. */
static const char torque_disabled_text[] = "Torque disabled by Powerbutton";
/** @brief Text displayed when tuning-menu parameters are reset. */
static const char tuning_menu_reset_text[] = "RESET Tuning Menu Parameters.";
/** @brief Text displayed when standard tuning mode is activated. */
static const char standard_tuning_mode_text[] = "Standard Tuning Menu mode";
/** @brief Text displayed when advanced tuning mode is activated. */
static const char advanced_tuning_mode_text[] = "Advanced Tuning Menu mode";
/** @brief Text displayed below a tuning-mode activation message. */
static const char tuning_mode_activated_text[] = "activated";
/** @brief Text displayed while the wheel base shuts down. */
static const char shutdown_text[] = "Switching off Podium DD Wheel Base";
/** @brief Primary text for an unsupported steering-wheel notice. */
static const char unsupported_wheel_primary_text[] = "WARNING";
/** @brief Secondary text for an unsupported steering-wheel notice. */
static const char unsupported_wheel_secondary_text[] = "Steering Wheel not supported!";
/** @brief Text displayed after wheel-center calibration succeeds. */
static const char wheel_center_calibrated_text[] = "Wheel center calibrated.";
/** @brief Text displayed after a position-sensor test succeeds. */
static const char position_sensor_succeeded_text[] = "Position Sensor Test Successful.";
/** @brief Text displayed when a position-sensor test starts. */
static const char position_sensor_started_text[] = "Position Sensor Test Started.";
/** @brief Text displayed when a position-sensor test fails. */
static const char position_sensor_failed_text[] = "Position Sensor Test Failed!";
/** @brief Primary text for a torque-reduced notice. */
static const char torque_reduced_primary_text[] = "NOTE: Torque is reduced to prevent damage";
/** @brief Secondary text for torque reduction on the simplified quick-release system. */
static const char torque_reduced_secondary_text[] = "  on the simplified quick-release system.";
/** @brief Secondary text for torque reduction on the steering wheel. */
static const char torque_reduced_steering_wheel_text[] = "  on the steering wheel.";
/** @brief Text requesting wheel disconnection before motor calibration. */
static const char motor_calibration_disconnect_text[] =
    "Please disconnect steering wheel to calibrate motor.";
/** @brief Text displayed when motor calibration is unsupported. */
static const char motor_calibration_unsupported_text[] =
    "Motor cal. not supported by current firmware version.";
/** @brief Text displayed while motor calibration is running. */
static const char motor_calibration_ongoing_text[] =
    "Motor calib. ongoing. Do not touch the shaft.";
/** @brief Text displayed after motor calibration succeeds. */
static const char motor_calibration_completed_text[] = "Motor calib. successfully completed.";
/** @brief Text displayed after motor calibration data is erased. */
static const char motor_calibration_erased_text[] = "Motor calib. data erased.";
/** @brief Primary text for a maximum-rotations notice. */
static const char maximum_rotations_exceeded_text[] = "Exceeded maximum rotations,";
/** @brief Secondary text requesting a wheel-base restart. */
static const char restart_wheel_base_text[] = "please restart the wheel base";
/** @brief Text displayed when alternative shifter mode is enabled. */
static const char alternative_shifter_enabled_text[] = "Alternative Shifter Mode Enabled";
/** @brief Text displayed when alternative shifter mode is disabled. */
static const char alternative_shifter_disabled_text[] = "Alternative Shifter Mode Disabled";

/** @brief Official warning bitmap at binary address 0xa9a8. */
static const uint8_t warning_icon[] = {
    0x00, 0x04, 0xbf, 0xb4, 0x00, 0x00, 0x00, 0x8f, 0x00, 0x0f, 0x80, 0x00, 0x04, 0xf0, 0x0f,
    0x00, 0xf4, 0x00, 0x0b, 0x00, 0x00, 0x00, 0x0b, 0x00, 0x0f, 0x00, 0x0f, 0x00, 0x0f, 0x00,
    0x0b, 0x00, 0x0f, 0x00, 0x0b, 0x00, 0x04, 0xf0, 0x0f, 0x00, 0xf4, 0x00, 0x00, 0x8f, 0x00,
    0x0f, 0x80, 0x00, 0x00, 0x04, 0xbf, 0xb4, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};

/** @brief Official error bitmap at binary address 0xa96c. */
static const uint8_t error_icon[] = {
    0x00, 0x04, 0xbf, 0xb4, 0x00, 0x00, 0x00, 0x8f, 0xff, 0xff, 0x80, 0x00, 0x04, 0xf0, 0xff,
    0xf0, 0xf4, 0x00, 0x0b, 0xff, 0x0f, 0x0f, 0xfb, 0x00, 0x0f, 0xff, 0xf0, 0xff, 0xff, 0x00,
    0x0b, 0xff, 0x0f, 0x0f, 0xfb, 0x00, 0x04, 0xf0, 0xff, 0xf0, 0xf4, 0x00, 0x00, 0x8f, 0xff,
    0xff, 0x80, 0x00, 0x00, 0x04, 0xbf, 0xb4, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};

static void draw_filled_overlay(DisplayFramebuffer framebuffer) {
    for (uint16_t row = OVERLAY_TOP; row < OVERLAY_BOTTOM; row++) {
        for (uint16_t column = OVERLAY_LEFT; column < OVERLAY_RIGHT; column++) {
            display_framebuffer_set_pixel(framebuffer, column, row, NOTICE_COLOR);
        }
    }
}

static void draw_notice_text(DisplayFramebuffer framebuffer, const char *text, uint16_t y,
                             bool primary, bool inverted) {
    const DisplayFont *font = &display_font_10_00c988;
    uint16_t width = display_text_width_for_font(font, text);
    if (width > DISPLAY_FRAMEBUFFER_WIDTH - 2u) {
        return;
    }
    uint16_t x = primary ? DISPLAY_FRAMEBUFFER_WIDTH / 2u - width / 2u
                         : (DISPLAY_FRAMEBUFFER_WIDTH - width) / 2u;
    display_text_draw_with_font(framebuffer, font, text, x, y, inverted);
}

static bool notice_is_outlined(SystemNoticeKind kind) {
    return kind == SYSTEM_NOTICE_UNSUPPORTED_WHEEL_OUTLINED;
}

/**
 * @brief Draws the official one-pixel unfilled notice ring.
 *
 * @param[in,out] framebuffer Framebuffer receiving the ring.
 */
static void draw_notice_ring(DisplayFramebuffer framebuffer) {
    int16_t x = 0;
    int16_t y = NOTICE_CIRCLE_RADIUS;
    int16_t error = 1 - y;

    for (;;) {
        display_framebuffer_set_pixel(framebuffer, NOTICE_CIRCLE_X + x, NOTICE_CIRCLE_Y + y,
                                      NOTICE_COLOR);
        display_framebuffer_set_pixel(framebuffer, NOTICE_CIRCLE_X + x, NOTICE_CIRCLE_Y - y,
                                      NOTICE_COLOR);
        display_framebuffer_set_pixel(framebuffer, NOTICE_CIRCLE_X - x, NOTICE_CIRCLE_Y + y,
                                      NOTICE_COLOR);
        display_framebuffer_set_pixel(framebuffer, NOTICE_CIRCLE_X - x, NOTICE_CIRCLE_Y - y,
                                      NOTICE_COLOR);
        display_framebuffer_set_pixel(framebuffer, NOTICE_CIRCLE_X + y, NOTICE_CIRCLE_Y + x,
                                      NOTICE_COLOR);
        display_framebuffer_set_pixel(framebuffer, NOTICE_CIRCLE_X + y, NOTICE_CIRCLE_Y - x,
                                      NOTICE_COLOR);
        display_framebuffer_set_pixel(framebuffer, NOTICE_CIRCLE_X - y, NOTICE_CIRCLE_Y + x,
                                      NOTICE_COLOR);
        display_framebuffer_set_pixel(framebuffer, NOTICE_CIRCLE_X - y, NOTICE_CIRCLE_Y - x,
                                      NOTICE_COLOR);
        if (y <= x) {
            return;
        }
        if (error < 0) {
            error += 2 * x + 3;
        } else {
            error += 2 * (x - y) + 5;
            y--;
        }
        x++;
    }
}

/**
 * @brief Draws the warning icon used by operator notices.
 *
 * Renders the official eleven-by-ten warning bitmap at the notice icon position.
 *
 * @param[in,out] framebuffer Complete local-display framebuffer.
 * @param[in] inverted True to draw a light field with a dark warning mark.
 */
static void draw_warning_icon(DisplayFramebuffer framebuffer, bool inverted) {
    display_bitmap_draw(framebuffer, warning_icon, WARNING_ICON_X, WARNING_ICON_Y,
                        WARNING_ICON_WIDTH, WARNING_ICON_HEIGHT, inverted, NOTICE_COLOR);
}

/**
 * @brief Draws the error icon used by rejection and failure notices.
 *
 * Renders the official eleven-by-ten error bitmap at the shared notice position.
 *
 * @param[in,out] framebuffer Complete local-display framebuffer.
 */
static void draw_error_icon(DisplayFramebuffer framebuffer) {
    display_bitmap_draw(framebuffer, error_icon, WARNING_ICON_X, WARNING_ICON_Y, WARNING_ICON_WIDTH,
                        WARNING_ICON_HEIGHT, true, NOTICE_COLOR);
}

/**
 * @brief Renders the persistent power-button torque-disabled notice.
 *
 * Clears the display, then shows the official filled white panel, inverted warning bitmap, and
 * centered Font10 torque-disabled message while the notice is visible.
 *
 * @param[in,out] framebuffer Complete local-display framebuffer.
 * @param[in] visible True while the torque-disabled notice owns the display.
 */
void display_notice_render_torque_disabled(DisplayFramebuffer framebuffer, bool visible) {
    display_framebuffer_clear(framebuffer);
    if (!visible) {
        return;
    }
    draw_filled_overlay(framebuffer);
    draw_warning_icon(framebuffer, true);
    draw_notice_text(framebuffer, torque_disabled_text, NOTICE_TEXT_Y, true, true);
}

/**
 * @brief Renders a tuning, wheel-position, or motor-originated system notice.
 *
 * Clears the display, selects the official filled or outlined overlay and icon, and lays out the
 * exact inverted or normal Font10 one-line or two-line operator message associated with the notice.
 *
 * @param[in,out] framebuffer Complete local-display framebuffer.
 * @param[in] kind Active system notice kind.
 */
void display_notice_render_system(DisplayFramebuffer framebuffer, SystemNoticeKind kind) {
    display_framebuffer_clear(framebuffer);
    if (kind == SYSTEM_NOTICE_NONE) {
        return;
    }

    bool outlined = notice_is_outlined(kind);
    bool inverted = !outlined;
    if (inverted) {
        draw_filled_overlay(framebuffer);
    } else {
        draw_notice_ring(framebuffer);
    }

    if (kind == SYSTEM_NOTICE_POSITION_SENSOR_TEST_FAILED ||
        kind == SYSTEM_NOTICE_MOTOR_CALIBRATION_DISCONNECT_WHEEL ||
        kind == SYSTEM_NOTICE_MOTOR_CALIBRATION_UNSUPPORTED) {
        draw_error_icon(framebuffer);
    } else if (kind == SYSTEM_NOTICE_MAXIMUM_ROTATIONS_EXCEEDED) {
        display_high_torque_icon_draw(framebuffer, WARNING_ICON_X, HIGH_TORQUE_ICON_Y, true);
    } else {
        draw_warning_icon(framebuffer, inverted);
    }

    if (kind == SYSTEM_NOTICE_TUNING_MENU_RESET) {
        draw_notice_text(framebuffer, tuning_menu_reset_text, NOTICE_TEXT_Y, true, inverted);
    } else if (kind == SYSTEM_NOTICE_WHEEL_CENTER_CALIBRATED) {
        draw_notice_text(framebuffer, wheel_center_calibrated_text, NOTICE_TEXT_Y, true, inverted);
    } else if (kind == SYSTEM_NOTICE_POSITION_SENSOR_TEST_SUCCEEDED) {
        draw_notice_text(framebuffer, position_sensor_succeeded_text, NOTICE_TEXT_Y, true,
                         inverted);
    } else if (kind == SYSTEM_NOTICE_POSITION_SENSOR_TEST_STARTED) {
        draw_notice_text(framebuffer, position_sensor_started_text, NOTICE_TEXT_Y, true, inverted);
    } else if (kind == SYSTEM_NOTICE_POSITION_SENSOR_TEST_FAILED) {
        draw_notice_text(framebuffer, position_sensor_failed_text, NOTICE_TEXT_Y, true, inverted);
    } else if (kind == SYSTEM_NOTICE_TORQUE_REDUCED ||
               kind == SYSTEM_NOTICE_TORQUE_REDUCED_STEERING_WHEEL) {
        draw_notice_text(framebuffer, torque_reduced_primary_text, NOTICE_PRIMARY_TEXT_Y, true,
                         inverted);
        draw_notice_text(framebuffer,
                         kind == SYSTEM_NOTICE_TORQUE_REDUCED ? torque_reduced_secondary_text
                                                              : torque_reduced_steering_wheel_text,
                         NOTICE_SECONDARY_TEXT_Y, false, inverted);
    } else if (kind == SYSTEM_NOTICE_MOTOR_CALIBRATION_DISCONNECT_WHEEL) {
        draw_notice_text(framebuffer, motor_calibration_disconnect_text, NOTICE_TEXT_Y, true,
                         inverted);
    } else if (kind == SYSTEM_NOTICE_MOTOR_CALIBRATION_UNSUPPORTED) {
        draw_notice_text(framebuffer, motor_calibration_unsupported_text, NOTICE_TEXT_Y, true,
                         inverted);
    } else if (kind == SYSTEM_NOTICE_MOTOR_CALIBRATION_ONGOING) {
        draw_notice_text(framebuffer, motor_calibration_ongoing_text, NOTICE_TEXT_Y, true,
                         inverted);
    } else if (kind == SYSTEM_NOTICE_MOTOR_CALIBRATION_COMPLETED) {
        draw_notice_text(framebuffer, motor_calibration_completed_text, NOTICE_TEXT_Y, true,
                         inverted);
    } else if (kind == SYSTEM_NOTICE_MOTOR_CALIBRATION_ERASED) {
        draw_notice_text(framebuffer, motor_calibration_erased_text, NOTICE_TEXT_Y, true, inverted);
    } else if (kind == SYSTEM_NOTICE_MAXIMUM_ROTATIONS_EXCEEDED) {
        draw_notice_text(framebuffer, maximum_rotations_exceeded_text, NOTICE_PRIMARY_TEXT_Y, true,
                         inverted);
        draw_notice_text(framebuffer, restart_wheel_base_text, NOTICE_SECONDARY_TEXT_Y, false,
                         inverted);
    } else if (kind == SYSTEM_NOTICE_STANDARD_TUNING_MODE ||
               kind == SYSTEM_NOTICE_ADVANCED_TUNING_MODE) {
        const char *mode_text = kind == SYSTEM_NOTICE_STANDARD_TUNING_MODE
                                    ? standard_tuning_mode_text
                                    : advanced_tuning_mode_text;
        draw_notice_text(framebuffer, mode_text, NOTICE_PRIMARY_TEXT_Y, true, inverted);
        draw_notice_text(framebuffer, tuning_mode_activated_text, NOTICE_SECONDARY_TEXT_Y, false,
                         inverted);
    } else if (kind == SYSTEM_NOTICE_SHUTDOWN) {
        draw_notice_text(framebuffer, shutdown_text, NOTICE_TEXT_Y, true, inverted);
    } else if (kind == SYSTEM_NOTICE_UNSUPPORTED_WHEEL_INVERTED ||
               kind == SYSTEM_NOTICE_UNSUPPORTED_WHEEL_OUTLINED) {
        draw_notice_text(framebuffer, unsupported_wheel_primary_text, NOTICE_PRIMARY_TEXT_Y, true,
                         inverted);
        draw_notice_text(framebuffer, unsupported_wheel_secondary_text, NOTICE_SECONDARY_TEXT_Y,
                         false, inverted);
    } else if (kind == SYSTEM_NOTICE_ALTERNATIVE_SHIFTER_ENABLED ||
               kind == SYSTEM_NOTICE_ALTERNATIVE_SHIFTER_DISABLED) {
        const char *text = kind == SYSTEM_NOTICE_ALTERNATIVE_SHIFTER_ENABLED
                               ? alternative_shifter_enabled_text
                               : alternative_shifter_disabled_text;
        draw_notice_text(framebuffer, text, NOTICE_TEXT_Y, true, inverted);
    }
}
