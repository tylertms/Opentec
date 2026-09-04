#include "wheel/startup_version_page.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "wheel/accessory.h"
#include "wheel/adapter.h"

/** @brief Startup-page mode values with packed axis version data. */
enum {
    WHEEL_STARTUP_ADAPTER_MODE = 1,     /**< Adapter mode that supports text version pages. */
    WHEEL_STARTUP_PULSE_MODE = 0x1b,    /**< Pulse-input wheel mode. */
    WHEEL_STARTUP_EXTENDED_MODE = 0x1c, /**< Extended wheel mode. */
};

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
 * @brief Appends a packed three-component dotted version.
 *
 * Uses the low six bits of the low byte and the next two packed bytes as decimal components.
 *
 * @param[in,out] line Startup version line receiving the version.
 * @param[in] version Packed version with components in its low three bytes.
 */
static void append_packed_version(WheelStartupVersionLine *line, uint32_t version) {
    append_decimal(line, (uint8_t)version & 0x3fu);
    line->text[line->length++] = '.';
    append_decimal(line, (uint8_t)(version >> 8));
    line->text[line->length++] = '.';
    append_decimal(line, (uint8_t)(version >> 16));
}

/**
 * @brief Reports whether the wheel version comes from packed axis values.
 *
 * @param[in] wheel_mode Negotiated attached-wheel mode.
 * @return True for the pulse-input and extended wheel modes.
 */
static bool wheel_mode_uses_packed_axes(uint8_t wheel_mode) {
    return wheel_mode == WHEEL_STARTUP_PULSE_MODE || wheel_mode == WHEEL_STARTUP_EXTENDED_MODE;
}

/**
 * @brief Builds the extended-adapter startup version page.
 *
 * Produces the four lines shown during startup: the base firmware version, supported auxiliary
 * version or NA, the steering-wheel version, and one blank line. Pulse-input and extended wheel
 * modes source the steering-wheel version from the retained pair of axis words.
 *
 * @param[in] accessory Identified auxiliary processor, or null when unavailable.
 * @param[in] adapter Connected extended adapter supplying its three version components.
 * @param[in] wheel_mode Negotiated attached-wheel mode.
 * @param[in] wheel_axis_values Two retained 16-bit axis values for modes 0x1B and 0x1C.
 * @param[out] page Four startup text lines populated in display order.
 * @return True when an extended adapter page was built.
 */
bool wheel_startup_adapter_version_page_build(const WheelAccessory *accessory,
                                              const WheelAdapterInput *adapter, uint8_t wheel_mode,
                                              const uint16_t wheel_axis_values[2],
                                              WheelStartupVersionPage *page) {
    if (adapter == NULL || page == NULL || !adapter->connected ||
        adapter->mode != WHEEL_STARTUP_ADAPTER_MODE ||
        (wheel_mode_uses_packed_axes(wheel_mode) && wheel_axis_values == NULL)) {
        return false;
    }
    *page = (WheelStartupVersionPage){0};

    /** @brief Label prefix for the base firmware version line. */
    static const uint8_t base_label[] = {'B', 'A', 'S', 'E', ':', ' '};
    /** @brief Label prefix for the auxiliary version line. */
    static const uint8_t motor_label[] = {'M', 'O', 'T', 'O', 'R', ':', ' '};
    /** @brief Label prefix for the adapter firmware version line. */
    static const uint8_t wheel_label[] = {'S', 'T', ' ', 'W', 'H', 'E', 'E', 'L', ':', ' '};
    /** @brief Text shown when the auxiliary identity is unavailable. */
    static const uint8_t unavailable[] = {'N', 'A'};
    /** @brief Base firmware version shown on the adapter page. */
    static const uint8_t base_version[] = {3, 9, 1};

    append_text(&page->lines[0], base_label, sizeof(base_label));
    append_version(&page->lines[0], base_version);
    append_text(&page->lines[1], motor_label, sizeof(motor_label));
    if (accessory != NULL && (accessory->kind == WHEEL_ACCESSORY_STANDARD ||
                              accessory->kind == WHEEL_ACCESSORY_EXTENDED)) {
        append_packed_version(&page->lines[1], accessory->version);
    } else {
        append_text(&page->lines[1], unavailable, sizeof(unavailable));
    }
    append_text(&page->lines[2], wheel_label, sizeof(wheel_label));
    if (wheel_mode_uses_packed_axes(wheel_mode)) {
        uint32_t packed_axis_values =
            (uint32_t)wheel_axis_values[0] | (uint32_t)wheel_axis_values[1] << 16;
        append_packed_version(&page->lines[2], packed_axis_values);
    } else {
        append_version(&page->lines[2], adapter->firmware_version);
    }
    page->lines[3].text[0] = ' ';
    page->lines[3].length = 1;
    return true;
}
