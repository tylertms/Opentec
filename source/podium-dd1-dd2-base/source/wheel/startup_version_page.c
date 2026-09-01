#include "wheel/startup_version_page.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "motor/identity.h"
#include "wheel/adapter.h"

/**
 * @brief Appends fixed text to one startup version line.
 *
 * Copies the supplied bytes after the current line content and advances its retained length.
 *
 * @param[in,out] line Startup version line receiving the bytes.
 * @param[in] text Fixed text bytes to append.
 * @param[in] length Number of bytes to append.
 */
static void append_text(WheelStartupVersionLine *line, const uint8_t *text, uint8_t length) {
    for (uint8_t index = 0; index < length; index++) {
        line->text[line->length++] = text[index];
    }
}

/**
 * @brief Appends one unsigned decimal component.
 *
 * Writes one, two, or three ASCII digits without leading zeroes.
 *
 * @param[in,out] line Startup version line receiving the digits.
 * @param[in] value Component value from zero through 255.
 */
static void append_decimal(WheelStartupVersionLine *line, uint8_t value) {
    if (value >= 100) {
        line->text[line->length++] = (uint8_t)('0' + value / 100u);
    }
    if (value >= 10) {
        line->text[line->length++] = (uint8_t)('0' + value / 10u % 10u);
    }
    line->text[line->length++] = (uint8_t)('0' + value % 10u);
}

/**
 * @brief Appends a three-component dotted version.
 *
 * Masks the first component to six bits, then separates all three decimal components with dots.
 *
 * @param[in,out] line Startup version line receiving the version.
 * @param[in] version Three version components in display order.
 */
static void append_version(WheelStartupVersionLine *line, const uint8_t version[3]) {
    append_decimal(line, version[0] & 0x3fu);
    line->text[line->length++] = '.';
    append_decimal(line, version[1]);
    line->text[line->length++] = '.';
    append_decimal(line, version[2]);
}

/**
 * @brief Builds the extended-adapter startup version page.
 *
 * Produces the four lines shown during startup: the base firmware version, managed motor version or
 * NA, the extended adapter version, and one blank line.
 *
 * @param[in] motor_identity Identified motor controller, or null when unavailable.
 * @param[in] adapter Connected extended adapter supplying its three version components.
 * @param[out] page Four startup text lines populated in display order.
 * @return True when an extended adapter page was built.
 */
bool wheel_startup_adapter_version_page_build(const MotorIdentity *motor_identity,
                                              const WheelAdapterInput *adapter,
                                              WheelStartupVersionPage *page) {
    if (adapter == NULL || page == NULL || !adapter->connected || adapter->mode != 1) {
        return false;
    }
    *page = (WheelStartupVersionPage){0};

    /** @brief Label prefix for the base firmware version line. */
    static const uint8_t base_label[] = {'B', 'A', 'S', 'E', ':', ' '};
    /** @brief Label prefix for the motor-controller version line. */
    static const uint8_t motor_label[] = {'M', 'O', 'T', 'O', 'R', ':', ' '};
    /** @brief Label prefix for the adapter firmware version line. */
    static const uint8_t wheel_label[] = {'S', 'T', ' ', 'W', 'H', 'E', 'E', 'L', ':', ' '};
    /** @brief Text shown when motor identity is unavailable. */
    static const uint8_t unavailable[] = {'N', 'A'};
    /** @brief Base firmware version shown on the adapter page. */
    static const uint8_t base_version[] = {3, 9, 1};

    append_text(&page->lines[0], base_label, sizeof(base_label));
    append_version(&page->lines[0], base_version);
    append_text(&page->lines[1], motor_label, sizeof(motor_label));
    if (motor_identity != NULL && motor_identity->protocol != MOTOR_PROTOCOL_LEGACY) {
        append_version(&page->lines[1], motor_identity->version);
    } else {
        append_text(&page->lines[1], unavailable, sizeof(unavailable));
    }
    append_text(&page->lines[2], wheel_label, sizeof(wheel_label));
    append_version(&page->lines[2], adapter->firmware_version);
    page->lines[3].text[0] = ' ';
    page->lines[3].length = 1;
    return true;
}
