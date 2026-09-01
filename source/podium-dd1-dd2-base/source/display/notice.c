#include "display/notice.h"

#include <stdbool.h>
#include <stdint.h>

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
    WARNING_ICON_WIDTH = 11,    /**< Warning-icon width in pixels. */
    WARNING_ICON_HEIGHT = 10,   /**< Warning-icon height in pixels. */
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

/**
 * @brief Draws the warning icon used by operator notices.
 *
 * Renders an eleven-by-ten triangular warning mark at the notice icon position.
 *
 * @param[in,out] framebuffer Complete local-display framebuffer.
 * @param[in] inverted True to draw a light field with a dark warning mark.
 */
static void draw_warning_icon(DisplayFramebuffer framebuffer, bool inverted) {
    uint16_t center = WARNING_ICON_X + WARNING_ICON_WIDTH / 2;
    uint8_t color = inverted ? 0 : NOTICE_COLOR;
    if (inverted) {
        for (uint16_t row = 0; row < WARNING_ICON_HEIGHT; row++) {
            for (uint16_t column = 0; column < WARNING_ICON_WIDTH; column++) {
                display_framebuffer_set_pixel(framebuffer, (uint16_t)(WARNING_ICON_X + column),
                                              (uint16_t)(WARNING_ICON_Y + row), NOTICE_COLOR);
            }
        }
    }
    for (uint16_t row = 0; row < WARNING_ICON_HEIGHT - 1; row++) {
        uint16_t half_width = (row + 1) / 2;
        display_framebuffer_set_pixel(framebuffer, (uint16_t)(center - half_width),
                                      (uint16_t)(WARNING_ICON_Y + row), color);
        display_framebuffer_set_pixel(framebuffer, (uint16_t)(center + half_width),
                                      (uint16_t)(WARNING_ICON_Y + row), color);
    }
    for (uint16_t column = 0; column < WARNING_ICON_WIDTH; column++) {
        display_framebuffer_set_pixel(framebuffer, (uint16_t)(WARNING_ICON_X + column),
                                      WARNING_ICON_Y + WARNING_ICON_HEIGHT - 1, color);
    }
    for (uint16_t row = 3; row < 7; row++) {
        display_framebuffer_set_pixel(framebuffer, center, (uint16_t)(WARNING_ICON_Y + row), color);
    }
    display_framebuffer_set_pixel(framebuffer, center, WARNING_ICON_Y + 8, color);
}

/**
 * @brief Draws the error icon used by rejection and failure notices.
 *
 * Renders an eleven-by-ten outlined mark with crossing diagonals at the shared notice position.
 *
 * @param[in,out] framebuffer Complete local-display framebuffer.
 */
static void draw_error_icon(DisplayFramebuffer framebuffer) {
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
 * @param[in,out] framebuffer Complete local-display framebuffer.
 * @param[in] visible True while the torque-disabled notice owns the display.
 */
void display_notice_render_torque_disabled(DisplayFramebuffer framebuffer, bool visible) {
    display_framebuffer_clear(framebuffer);
    if (!visible) {
        return;
    }
    draw_warning_icon(framebuffer, false);
    display_text_draw_centered(framebuffer, torque_disabled_text, NOTICE_TEXT_Y, 1, NOTICE_COLOR);
}

/**
 * @brief Renders a tuning, wheel-position, or motor-originated system notice.
 *
 * Clears the display, selects the warning or error icon, and lays out the exact one-line or
 * two-line operator message associated with the notice.
 *
 * @param[in,out] framebuffer Complete local-display framebuffer.
 * @param[in] kind Active system notice kind.
 */
void display_notice_render_system(DisplayFramebuffer framebuffer, SystemNoticeKind kind) {
    display_framebuffer_clear(framebuffer);
    if (kind == SYSTEM_NOTICE_NONE) {
        return;
    }

    if (kind == SYSTEM_NOTICE_POSITION_SENSOR_TEST_FAILED ||
        kind == SYSTEM_NOTICE_MOTOR_CALIBRATION_DISCONNECT_WHEEL ||
        kind == SYSTEM_NOTICE_MOTOR_CALIBRATION_UNSUPPORTED) {
        draw_error_icon(framebuffer);
    } else if (kind == SYSTEM_NOTICE_MAXIMUM_ROTATIONS_EXCEEDED) {
        display_high_torque_icon_draw(framebuffer, WARNING_ICON_X, WARNING_ICON_Y);
    } else {
        draw_warning_icon(framebuffer, kind == SYSTEM_NOTICE_UNSUPPORTED_WHEEL_INVERTED);
    }

    if (kind == SYSTEM_NOTICE_TUNING_MENU_RESET) {
        display_text_draw_centered(framebuffer, tuning_menu_reset_text, NOTICE_TEXT_Y, 1,
                                   NOTICE_COLOR);
    } else if (kind == SYSTEM_NOTICE_WHEEL_CENTER_CALIBRATED) {
        display_text_draw_centered(framebuffer, wheel_center_calibrated_text, NOTICE_TEXT_Y, 1,
                                   NOTICE_COLOR);
    } else if (kind == SYSTEM_NOTICE_POSITION_SENSOR_TEST_SUCCEEDED) {
        display_text_draw_centered(framebuffer, position_sensor_succeeded_text, NOTICE_TEXT_Y, 1,
                                   NOTICE_COLOR);
    } else if (kind == SYSTEM_NOTICE_POSITION_SENSOR_TEST_STARTED) {
        display_text_draw_centered(framebuffer, position_sensor_started_text, NOTICE_TEXT_Y, 1,
                                   NOTICE_COLOR);
    } else if (kind == SYSTEM_NOTICE_POSITION_SENSOR_TEST_FAILED) {
        display_text_draw_centered(framebuffer, position_sensor_failed_text, NOTICE_TEXT_Y, 1,
                                   NOTICE_COLOR);
    } else if (kind == SYSTEM_NOTICE_TORQUE_REDUCED ||
               kind == SYSTEM_NOTICE_TORQUE_REDUCED_STEERING_WHEEL) {
        display_text_draw_centered(framebuffer, torque_reduced_primary_text, NOTICE_PRIMARY_TEXT_Y,
                                   1, NOTICE_COLOR);
        display_text_draw_centered(framebuffer,
                                   kind == SYSTEM_NOTICE_TORQUE_REDUCED
                                       ? torque_reduced_secondary_text
                                       : torque_reduced_steering_wheel_text,
                                   NOTICE_SECONDARY_TEXT_Y, 1, NOTICE_COLOR);
    } else if (kind == SYSTEM_NOTICE_MOTOR_CALIBRATION_DISCONNECT_WHEEL) {
        display_text_draw_centered(framebuffer, motor_calibration_disconnect_text, NOTICE_TEXT_Y, 1,
                                   NOTICE_COLOR);
    } else if (kind == SYSTEM_NOTICE_MOTOR_CALIBRATION_UNSUPPORTED) {
        display_text_draw_centered(framebuffer, motor_calibration_unsupported_text, NOTICE_TEXT_Y,
                                   1, NOTICE_COLOR);
    } else if (kind == SYSTEM_NOTICE_MOTOR_CALIBRATION_ONGOING) {
        display_text_draw_centered(framebuffer, motor_calibration_ongoing_text, NOTICE_TEXT_Y, 1,
                                   NOTICE_COLOR);
    } else if (kind == SYSTEM_NOTICE_MOTOR_CALIBRATION_COMPLETED) {
        display_text_draw_centered(framebuffer, motor_calibration_completed_text, NOTICE_TEXT_Y, 1,
                                   NOTICE_COLOR);
    } else if (kind == SYSTEM_NOTICE_MOTOR_CALIBRATION_ERASED) {
        display_text_draw_centered(framebuffer, motor_calibration_erased_text, NOTICE_TEXT_Y, 1,
                                   NOTICE_COLOR);
    } else if (kind == SYSTEM_NOTICE_MAXIMUM_ROTATIONS_EXCEEDED) {
        display_text_draw_centered(framebuffer, maximum_rotations_exceeded_text,
                                   NOTICE_PRIMARY_TEXT_Y, 1, NOTICE_COLOR);
        display_text_draw_centered(framebuffer, restart_wheel_base_text, NOTICE_SECONDARY_TEXT_Y, 1,
                                   NOTICE_COLOR);
    } else if (kind == SYSTEM_NOTICE_STANDARD_TUNING_MODE ||
               kind == SYSTEM_NOTICE_ADVANCED_TUNING_MODE) {
        const char *mode_text = kind == SYSTEM_NOTICE_STANDARD_TUNING_MODE
                                    ? standard_tuning_mode_text
                                    : advanced_tuning_mode_text;
        display_text_draw_centered(framebuffer, mode_text, NOTICE_PRIMARY_TEXT_Y, 1, NOTICE_COLOR);
        display_text_draw_centered(framebuffer, tuning_mode_activated_text, NOTICE_SECONDARY_TEXT_Y,
                                   1, NOTICE_COLOR);
    } else if (kind == SYSTEM_NOTICE_SHUTDOWN) {
        display_text_draw_centered(framebuffer, shutdown_text, NOTICE_TEXT_Y, 1, NOTICE_COLOR);
    } else if (kind == SYSTEM_NOTICE_UNSUPPORTED_WHEEL_INVERTED ||
               kind == SYSTEM_NOTICE_UNSUPPORTED_WHEEL_OUTLINED) {
        display_text_draw_centered(framebuffer, unsupported_wheel_primary_text,
                                   NOTICE_PRIMARY_TEXT_Y, 1, NOTICE_COLOR);
        display_text_draw_centered(framebuffer, unsupported_wheel_secondary_text,
                                   NOTICE_SECONDARY_TEXT_Y, 1, NOTICE_COLOR);
    } else if (kind == SYSTEM_NOTICE_ALTERNATIVE_SHIFTER_ENABLED ||
               kind == SYSTEM_NOTICE_ALTERNATIVE_SHIFTER_DISABLED) {
        const char *text = kind == SYSTEM_NOTICE_ALTERNATIVE_SHIFTER_ENABLED
                               ? alternative_shifter_enabled_text
                               : alternative_shifter_disabled_text;
        display_text_draw_centered(framebuffer, text, NOTICE_TEXT_Y, 1, NOTICE_COLOR);
    }
}
