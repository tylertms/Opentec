#ifndef OPENTEC_BASE_DISPLAY_SYSTEM_INFORMATION_PAGE_H
#define OPENTEC_BASE_DISPLAY_SYSTEM_INFORMATION_PAGE_H

#include <stdbool.h>
#include <stdint.h>

#include "display/framebuffer.h"

/**
 * @brief Stores component versions and runtimes for the system-information page.
 *
 * The values cover the base, motor, and wheel quick-release hardware shown in the three page
 * columns.
 */
typedef struct {
    uint8_t main_hardware;               /**< Main-board hardware version. */
    uint32_t main_runtime_seconds;       /**< Main-board operating time in seconds. */
    uint8_t motor_firmware[3];           /**< Motor firmware major, minor, and patch components. */
    uint8_t motor_hardware;              /**< Motor hardware version. */
    bool motor_accessory_type_available; /**< Whether the motor supplied an accessory type. */
    uint8_t motor_accessory_type;        /**< Motor accessory type value when available. */
    uint32_t motor_runtime_seconds;      /**< Motor operating time in seconds. */
    uint8_t quick_release_firmware;      /**< Wheel quick-release firmware version. */
    uint8_t quick_release_hardware;      /**< Wheel quick-release hardware version. */
    uint32_t quick_release_runtime_seconds; /**< Wheel quick-release operating time in seconds. */
} DisplaySystemInformation;

/**
 * @brief Renders the system-information title.
 *
 * Clears the framebuffer and centers the title shown before component information appears.
 *
 * @param[in,out] framebuffer Framebuffer receiving the title pixels.
 */
void display_system_information_page_render_title(DisplayFramebuffer framebuffer);

/**
 * @brief Renders component versions and operating times.
 *
 * Clears the framebuffer and draws the main-board, motor, and wheel quick-release information in
 * separate columns.
 *
 * @param[in,out] framebuffer Framebuffer receiving the rendered page.
 * @param[in] information Component information to render.
 */
void display_system_information_page_render(DisplayFramebuffer framebuffer,
                                            const DisplaySystemInformation *information);

#endif
