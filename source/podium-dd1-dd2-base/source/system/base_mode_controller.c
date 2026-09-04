#include "system/base_mode_controller.h"

#include <stdbool.h>
#include <stdint.h>

enum {
    BASE_MODE_CONTROLLER_MEMORY_TIMEOUT_MS = 2000,
    BASE_MODE_CONTROLLER_USB_DELAY_MS = 100,
    BASE_MODE_CONTROLLER_STATUS_TIMEOUT_MS = 2000,
};

static bool time_reached(uint32_t now_ms, uint32_t deadline_ms) {
    return (int32_t)(now_ms - deadline_ms) >= 0;
}

static bool after_deadline(uint32_t now_ms, uint32_t deadline_ms) {
    return time_reached(now_ms, deadline_ms + 1u);
}

static void enter_memory_timeout(BaseModeController *controller, uint32_t now_ms) {
    controller->transport_deadline_ms = now_ms + BASE_MODE_CONTROLLER_STATUS_TIMEOUT_MS;
    controller->phase = BASE_MODE_CONTROLLER_MEMORY_TIMEOUT;
}

static uint8_t begin_transport_wait(BaseModeController *controller,
                                    BaseModeControllerPhase phase, uint32_t now_ms) {
    controller->transport_deadline_ms = now_ms + BASE_MODE_CONTROLLER_STATUS_TIMEOUT_MS;
    controller->phase = phase;
    return BASE_MODE_CONTROLLER_ACTION_ENABLE_USB;
}

void base_mode_controller_init(BaseModeController *controller) {
    if (controller == 0) {
        return;
    }
    *controller = (BaseModeController){
        .phase = BASE_MODE_CONTROLLER_RESET,
    };
}

bool base_mode_controller_start(BaseModeController *controller, uint32_t now_ms) {
    if (controller == 0 || controller->phase != BASE_MODE_CONTROLLER_RESET) {
        return false;
    }
    controller->memory_deadline_ms = now_ms + BASE_MODE_CONTROLLER_MEMORY_TIMEOUT_MS;
    controller->usb_enable_deadline_ms = now_ms + BASE_MODE_CONTROLLER_USB_DELAY_MS;
    controller->phase = BASE_MODE_CONTROLLER_MEMORY_STARTUP;
    return true;
}

bool base_mode_controller_start_playstation(BaseModeController *controller, uint32_t now_ms) {
    if (controller == 0 || controller->phase != BASE_MODE_CONTROLLER_RESET) {
        return false;
    }
    controller->usb_enable_deadline_ms = now_ms + BASE_MODE_CONTROLLER_USB_DELAY_MS;
    controller->phase = BASE_MODE_CONTROLLER_HID_PREPARE;
    return true;
}

uint8_t base_mode_controller_step(BaseModeController *controller, uint32_t now_ms,
                                  BaseModeControllerMemoryResult memory_result, bool mode_valid,
                                  bool protocol_active) {
    if (controller == 0) {
        return BASE_MODE_CONTROLLER_ACTION_NONE;
    }

    switch (controller->phase) {
    case BASE_MODE_CONTROLLER_MEMORY_STARTUP:
        if (memory_result == BASE_MODE_CONTROLLER_MEMORY_COMPLETE) {
            controller->phase = BASE_MODE_CONTROLLER_STATUS_USB_DELAY;
            if (after_deadline(now_ms, controller->usb_enable_deadline_ms)) {
                return begin_transport_wait(controller, BASE_MODE_CONTROLLER_STATUS_WAIT, now_ms);
            }
            return BASE_MODE_CONTROLLER_ACTION_NONE;
        }
        if (!mode_valid || !protocol_active) {
            controller->phase = BASE_MODE_CONTROLLER_RESET;
            return BASE_MODE_CONTROLLER_ACTION_FALLBACK_NATIVE;
        }
        if (time_reached(now_ms, controller->memory_deadline_ms)) {
            enter_memory_timeout(controller, now_ms);
            return BASE_MODE_CONTROLLER_ACTION_DISPLAY_ERROR |
                   BASE_MODE_CONTROLLER_ACTION_RESET_MEMORY;
        }
        return BASE_MODE_CONTROLLER_ACTION_NONE;

    case BASE_MODE_CONTROLLER_STATUS_USB_DELAY:
        if (!after_deadline(now_ms, controller->usb_enable_deadline_ms)) {
            return BASE_MODE_CONTROLLER_ACTION_NONE;
        }
        return begin_transport_wait(controller, BASE_MODE_CONTROLLER_STATUS_WAIT, now_ms);

    case BASE_MODE_CONTROLLER_STATUS_WAIT:
        if (!after_deadline(now_ms, controller->transport_deadline_ms)) {
            return BASE_MODE_CONTROLLER_ACTION_NONE;
        }
        controller->phase = BASE_MODE_CONTROLLER_STATUS_ACTIVE;
        return BASE_MODE_CONTROLLER_ACTION_NONE;

    case BASE_MODE_CONTROLLER_STATUS_ACTIVE:
        if (mode_valid && protocol_active) {
            return BASE_MODE_CONTROLLER_ACTION_NONE;
        }
        controller->phase = BASE_MODE_CONTROLLER_RESET;
        return BASE_MODE_CONTROLLER_ACTION_FALLBACK_NATIVE;

    case BASE_MODE_CONTROLLER_HID_PREPARE:
        controller->phase = BASE_MODE_CONTROLLER_HID_USB_DELAY;
        if (after_deadline(now_ms, controller->usb_enable_deadline_ms)) {
            return begin_transport_wait(controller, BASE_MODE_CONTROLLER_HID_WAIT, now_ms);
        }
        return BASE_MODE_CONTROLLER_ACTION_NONE;

    case BASE_MODE_CONTROLLER_HID_USB_DELAY:
        if (!after_deadline(now_ms, controller->usb_enable_deadline_ms)) {
            return BASE_MODE_CONTROLLER_ACTION_NONE;
        }
        return begin_transport_wait(controller, BASE_MODE_CONTROLLER_HID_WAIT, now_ms);

    case BASE_MODE_CONTROLLER_HID_WAIT:
        if (!after_deadline(now_ms, controller->transport_deadline_ms)) {
            return BASE_MODE_CONTROLLER_ACTION_NONE;
        }
        controller->phase = BASE_MODE_CONTROLLER_HID_ACTIVE;
        return BASE_MODE_CONTROLLER_ACTION_NONE;

    case BASE_MODE_CONTROLLER_HID_ACTIVE:
        if (mode_valid && protocol_active) {
            return BASE_MODE_CONTROLLER_ACTION_NONE;
        }
        controller->phase = BASE_MODE_CONTROLLER_RESET;
        return BASE_MODE_CONTROLLER_ACTION_FALLBACK_NATIVE;

    case BASE_MODE_CONTROLLER_MEMORY_TIMEOUT:
        if (!after_deadline(now_ms, controller->transport_deadline_ms)) {
            return BASE_MODE_CONTROLLER_ACTION_NONE;
        }
        controller->phase = BASE_MODE_CONTROLLER_RESET;
        return BASE_MODE_CONTROLLER_ACTION_FALLBACK_NATIVE |
               BASE_MODE_CONTROLLER_ACTION_RESET_MEMORY;

    case BASE_MODE_CONTROLLER_RESET:
    default:
        return BASE_MODE_CONTROLLER_ACTION_NONE;
    }
}

bool base_mode_controller_memory_active(const BaseModeController *controller) {
    return controller != 0 && (controller->phase == BASE_MODE_CONTROLLER_MEMORY_STARTUP ||
                               controller->phase == BASE_MODE_CONTROLLER_MEMORY_TIMEOUT);
}

BaseModeControllerPhase base_mode_controller_phase(const BaseModeController *controller) {
    return controller != 0 ? controller->phase : BASE_MODE_CONTROLLER_RESET;
}

bool base_mode_controller_hid_active(const BaseModeController *controller) {
    if (controller == 0) {
        return false;
    }
    return controller->phase >= BASE_MODE_CONTROLLER_HID_PREPARE &&
           controller->phase <= BASE_MODE_CONTROLLER_HID_ACTIVE;
}
