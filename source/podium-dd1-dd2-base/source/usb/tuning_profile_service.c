#include "usb/tuning_profile_service.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "profile/bank.h"
#include "profile/tuning.h"
#include "usb/tuning_profile_report.h"
#include "usb/vendor_command.h"

enum {
    PROFILE_ACTION_APPLY = 0,
    PROFILE_ACTION_SELECT = 1,
    PROFILE_ACTION_REFRESH = 2,
    PROFILE_ACTION_SAVE = 3,
    PROFILE_ACTION_RESET_ALL = 4,
    PROFILE_ACTION_RESET_STANDARD = 5,
    PROFILE_ACTION_TOGGLE_MODE = 6,
    PROFILE_SELECTOR_MINIMUM = 1,
    PROFILE_SELECTOR_MAXIMUM = 6,
};

static bool deadline_reached(uint32_t now_ms, uint32_t deadline_ms) {
    return (int32_t)(now_ms - deadline_ms) >= 0;
}

static bool select_profile(TuningProfileBank *bank, uint8_t selector) {
    if (selector < PROFILE_SELECTOR_MINIMUM || selector > PROFILE_SELECTOR_MAXIMUM ||
        !tuning_profile_bank_select(bank, (uint8_t)(selector - 1))) {
        return false;
    }
    tuning_profile_bank_activate_selected(bank);
    return true;
}

/**
 * @brief Initializes tuning-profile vendor-command state.
 *
 * Allows the first reset and mode command immediately and requests an initial profile response.
 *
 * @param[out] service Tuning-profile command state to initialize.
 */
void usb_tuning_profile_service_init(UsbTuningProfileService *service) {
    *service = (UsbTuningProfileService){.response_pending = true};
}

/**
 * @brief Applies an opcode-three tuning-profile command.
 *
 * Supports profile value updates, one-based profile selection, response refresh, explicit save,
 * all-profile reset, Standard-profile reset, and rate-limited Standard or Advanced mode changes.
 * Reset-all commands share a ten-second guard with Standard-profile resets.
 *
 * @param[in,out] service Tuning-profile command timing and response state.
 * @param[in,out] bank Tuning profiles and Standard or Advanced mode.
 * @param[in] command Decoded vendor command.
 * @param[in] now_ms Current monotonic time in milliseconds.
 * @return Actions for the firmware integration to perform.
 */
UsbTuningProfileAction usb_tuning_profile_service_apply(UsbTuningProfileService *service,
                                                        TuningProfileBank *bank,
                                                        const UsbVendorCommand *command,
                                                        uint32_t now_ms) {
    if (service == NULL || bank == NULL || command == NULL ||
        command->kind != USB_VENDOR_COMMAND_DEVICE_CONTROL_UPDATE || command->arguments == NULL ||
        command->length == 0) {
        return USB_TUNING_PROFILE_ACTION_NONE;
    }

    uint8_t action = command->arguments[0];
    UsbTuningProfileAction result = USB_TUNING_PROFILE_ACTION_CLAIM;
    switch (action) {
    case PROFILE_ACTION_APPLY: {
        if (command->length < USB_TUNING_PROFILE_VALUE_COUNT + 2 ||
            !select_profile(bank, command->arguments[1])) {
            return result;
        }
        if (!usb_tuning_profile_report_decode(&command->arguments[2],
                                              &bank->slots[bank->active_slot])) {
            return result;
        }
        service->response_pending = true;
        return result | USB_TUNING_PROFILE_ACTION_PROFILE_CHANGED |
               USB_TUNING_PROFILE_ACTION_SETTINGS_CHANGED;
    }
    case PROFILE_ACTION_SELECT:
        if (command->length >= 2 && select_profile(bank, command->arguments[1])) {
            service->response_pending = true;
            result |= USB_TUNING_PROFILE_ACTION_PROFILE_CHANGED |
                      USB_TUNING_PROFILE_ACTION_SETTINGS_CHANGED;
        }
        return result;
    case PROFILE_ACTION_REFRESH:
        service->response_pending = true;
        return result;
    case PROFILE_ACTION_SAVE:
        return result | USB_TUNING_PROFILE_ACTION_SAVE;
    case PROFILE_ACTION_RESET_ALL:
        if (deadline_reached(now_ms, service->reset_after_ms)) {
            bool mode_changed = !bank->standard_mode_enabled;
            tuning_profile_bank_defaults(bank);
            service->reset_after_ms = now_ms + USB_TUNING_PROFILE_RESET_DELAY_MS;
            service->response_pending = true;
            result |= USB_TUNING_PROFILE_ACTION_PROFILE_CHANGED |
                      USB_TUNING_PROFILE_ACTION_SETTINGS_CHANGED;
            if (mode_changed) {
                result |= USB_TUNING_PROFILE_ACTION_MODE_CHANGED;
            }
        }
        return result;
    case PROFILE_ACTION_RESET_STANDARD:
        if (deadline_reached(now_ms, service->reset_after_ms)) {
            tuning_profile_defaults(&bank->slots[0]);
            service->response_pending = true;
            result |= USB_TUNING_PROFILE_ACTION_SETTINGS_CHANGED;
            if (bank->active_slot == 0) {
                result |= USB_TUNING_PROFILE_ACTION_PROFILE_CHANGED;
            }
        }
        return result;
    case PROFILE_ACTION_TOGGLE_MODE:
        if (deadline_reached(now_ms, service->mode_change_after_ms)) {
            bank->standard_mode_enabled = !bank->standard_mode_enabled;
            service->mode_change_after_ms = now_ms + USB_TUNING_PROFILE_MODE_DELAY_MS;
            service->response_pending = true;
            result |=
                USB_TUNING_PROFILE_ACTION_MODE_CHANGED | USB_TUNING_PROFILE_ACTION_SETTINGS_CHANGED;
        }
        return result;
    default:
        return result;
    }
}

/**
 * @brief Reports whether a tuning-profile response is due.
 *
 * Returns the response latch set by initialization, refresh, or a visible profile-state change.
 *
 * @param[in] service Tuning-profile command state.
 * @return True when the active profile response must be sent.
 */
bool usb_tuning_profile_service_response_pending(const UsbTuningProfileService *service) {
    return service != NULL && service->response_pending;
}

/**
 * @brief Completes a tuning-profile response transfer.
 *
 * Clears the response latch after the complete report has been accepted by the USB endpoint.
 *
 * @param[in,out] service Tuning-profile command state.
 */
void usb_tuning_profile_service_response_sent(UsbTuningProfileService *service) {
    service->response_pending = false;
}
