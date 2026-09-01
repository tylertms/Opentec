#ifndef OPENTEC_BASE_WHEEL_STARTUP_VERSION_PAGE_H
#define OPENTEC_BASE_WHEEL_STARTUP_VERSION_PAGE_H

#include <stdbool.h>
#include <stdint.h>

#include "motor/identity.h"
#include "wheel/adapter.h"

/** @brief Dimensions of the extended-adapter startup version page. */
enum {
    WHEEL_STARTUP_VERSION_LINE_COUNT = 4,     /**< Number of text lines in the page. */
    WHEEL_STARTUP_VERSION_LINE_CAPACITY = 20, /**< Maximum bytes retained in one text line. */
};

/** @brief One bounded text line in an extended-adapter startup page. */
typedef struct {
    uint8_t
        text[WHEEL_STARTUP_VERSION_LINE_CAPACITY]; /**< Retained line bytes without a terminator. */
    uint8_t length;                                /**< Number of valid bytes in text. */
} WheelStartupVersionLine;

/** @brief Four-line startup version page for an extended adapter display. */
typedef struct {
    WheelStartupVersionLine lines[WHEEL_STARTUP_VERSION_LINE_COUNT]; /**< Lines in display order. */
} WheelStartupVersionPage;

/**
 * @brief Builds an extended-adapter startup version page.
 *
 * Populates the base, motor, adapter, and blank lines when the connected adapter exposes mode-one
 * version-page support.
 *
 * @param[in] motor_identity Motor-controller identity, or null when unavailable.
 * @param[in] adapter Adapter input supplying connection, mode, and firmware version.
 * @param[out] page Version page to populate.
 * @return True when adapter and page are valid and the adapter supports the version page; otherwise
 * false.
 */
bool wheel_startup_adapter_version_page_build(const MotorIdentity *motor_identity,
                                              const WheelAdapterInput *adapter,
                                              WheelStartupVersionPage *page);

#endif
