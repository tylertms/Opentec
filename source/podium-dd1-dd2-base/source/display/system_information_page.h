#ifndef OPENTEC_BASE_DISPLAY_SYSTEM_INFORMATION_PAGE_H
#define OPENTEC_BASE_DISPLAY_SYSTEM_INFORMATION_PAGE_H

#include <stdbool.h>
#include <stdint.h>

#include "display/framebuffer.h"

/** @brief Base, motor, and wheel quick-release values shown on the system-information page. */
typedef struct {
    uint8_t main_hardware;
    uint32_t main_runtime_seconds;
    uint8_t motor_firmware[3];
    uint8_t motor_hardware;
    bool motor_accessory_type_available;
    uint8_t motor_accessory_type;
    uint32_t motor_runtime_seconds;
    uint8_t quick_release_firmware;
    uint8_t quick_release_hardware;
    uint32_t quick_release_runtime_seconds;
} DisplaySystemInformation;

void display_system_information_page_render_title(DisplayFramebuffer framebuffer);
void display_system_information_page_render(DisplayFramebuffer framebuffer,
                                            const DisplaySystemInformation *information);

#endif
