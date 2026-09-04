#include "usb/tuning_profile_service.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "profile/bank.h"
#include "profile/tuning.h"
#include "usb/tuning_profile_report.h"
#include "usb/vendor_command.h"

/** @brief Tuning-profile command action and selector constants. */
enum {
    PROFILE_ACTION_APPLY = 0,          /**< Apply encoded values to a selected profile. */
    PROFILE_ACTION_SELECT = 1,         /**< Select and activate a profile. */
    PROFILE_ACTION_REFRESH = 2,        /**< Request a profile response refresh. */
    PROFILE_ACTION_SAVE = 3,           /**< Request profile persistence. */
    PROFILE_ACTION_RESET_ALL = 4,      /**< Restore retained profiles and Auto core values. */
    PROFILE_ACTION_RESET_STANDARD = 5, /**< Restore the Standard profile. */
    PROFILE_ACTION_TOGGLE_MODE = 6,    /**< Toggle Standard and Advanced mode. */
    PROFILE_SELECTOR_MINIMUM = 1,      /**< Smallest accepted one-based profile selector. */
    PROFILE_SELECTOR_MAXIMUM = 6,      /**< Largest accepted one-based profile selector. */
};

/**
 * @brief Tests a wrap-safe monotonic deadline.
 *
 * Compares two millisecond timestamps while preserving ordering across counter rollover.
 *
 * @param[in] now_ms Current monotonic time in milliseconds.
 * @param[in] deadline_ms Deadline to test.
 * @return True when the deadline has been reached; otherwise false.
 */
static bool deadline_reached(uint32_t now_ms, uint32_t deadline_ms) {
    return (int32_t)(now_ms - deadline_ms) >= 0;
}

/**
 * @brief Tests the strict deadline used by profile-mode toggles.
 *
 * The official mode-toggle path remains blocked when the current time equals its deadline.
 *
 * @param[in] now_ms Current monotonic time in milliseconds.
 * @param[in] deadline_ms Mode-toggle deadline to test.
 * @return True only when the deadline has passed; otherwise false.
 */
static bool mode_toggle_deadline_passed(uint32_t now_ms, uint32_t deadline_ms) {
    return now_ms > deadline_ms;
}

/**
 * @brief Restores the core values of the transient automatic profile.
 *
 * Reset-all restores the six values handled by the official automatic-profile core reset while
 * retaining the secondary values supplied by the attached-wheel interface.
 *
 * @param[in,out] profile Automatic profile receiving its core defaults.
 */
static void reset_automatic_profile_core(TuningProfile *profile) {
    TuningProfile defaults;
    tuning_profile_defaults(&defaults);
    profile->vibration_strength = defaults.vibration_strength;
    profile->brake_indicator_level = defaults.brake_indicator_level;
    profile->brake_force = defaults.brake_force;
    profile->alternate_brake_force = defaults.alternate_brake_force;
    profile->multi_position_mode = defaults.multi_position_mode;
    profile->paddle_mode = defaults.paddle_mode;
}

/**
 * @brief Restores the profile records covered by the official reset-all command.
 *
 * Retained profiles map to slots one through five in the effective bank. Slot zero is the
 * transient automatic profile, whose secondary values and apply-pending marker are retained.
 *
 * @param[in,out] bank Profile bank receiving reset values and Standard selection state.
 */
static void reset_profiles(TuningProfileBank *bank) {
    bool automatic_apply_pending = bank->automatic_apply_pending;
    for (uint8_t slot = 1; slot < TUNING_PROFILE_SLOT_COUNT; slot++) {
        tuning_profile_defaults(&bank->slots[slot]);
    }
    reset_automatic_profile_core(&bank->slots[0]);
    bank->selected_slot = 0;
    bank->active_slot = 0;
    bank->standard_mode_enabled = true;
    bank->automatic_apply_pending = automatic_apply_pending;
}

/**
 * @brief Selects and activates a one-based tuning setup.
 *
 * Converts the report selector to the bank's zero-based index and rejects values outside setups 1
 * through 6.
 *
 * @param[in,out] bank Tuning-profile bank to update.
 * @param[in] selector One-based tuning setup selector.
 * @return True when the setup was selected and activated; otherwise false.
 */
static bool select_profile(TuningProfileBank *bank, uint8_t selector) {
    if (selector < PROFILE_SELECTOR_MINIMUM || selector > PROFILE_SELECTOR_MAXIMUM ||
        !tuning_profile_bank_select(bank, (uint8_t)(selector - 1))) {
        return false;
    }
    tuning_profile_bank_activate_selected(bank);
    return true;
}

void usb_tuning_profile_service_init(UsbTuningProfileService *service) {
    *service = (UsbTuningProfileService){.response_pending = true};
}

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
            reset_profiles(bank);
            service->reset_after_ms = now_ms + USB_TUNING_PROFILE_RESET_DELAY_MS;
            service->response_pending = true;
            result |= USB_TUNING_PROFILE_ACTION_PROFILE_CHANGED |
                      USB_TUNING_PROFILE_ACTION_SETTINGS_CHANGED |
                      USB_TUNING_PROFILE_ACTION_RESET_COMPLETED;
            if (mode_changed) {
                result |= USB_TUNING_PROFILE_ACTION_MODE_CHANGED;
            }
        }
        return result;
    case PROFILE_ACTION_RESET_STANDARD:
        if (deadline_reached(now_ms, service->reset_after_ms)) {
            tuning_profile_defaults(&bank->slots[0]);
            bank->automatic_apply_pending = false;
            service->response_pending = true;
            result |= USB_TUNING_PROFILE_ACTION_SETTINGS_CHANGED;
            if (bank->active_slot == 0) {
                result |= USB_TUNING_PROFILE_ACTION_PROFILE_CHANGED;
            }
        }
        return result;
    case PROFILE_ACTION_TOGGLE_MODE:
        if (mode_toggle_deadline_passed(now_ms, service->mode_change_after_ms)) {
            bool enable_standard = !bank->standard_mode_enabled;
            if (tuning_profile_bank_set_standard_mode(bank, enable_standard)) {
                if (enable_standard) {
                    result |= USB_TUNING_PROFILE_ACTION_PROFILE_CHANGED;
                }
                service->mode_change_after_ms = now_ms + USB_TUNING_PROFILE_MODE_DELAY_MS;
                service->response_pending = true;
                result |= USB_TUNING_PROFILE_ACTION_MODE_CHANGED |
                          USB_TUNING_PROFILE_ACTION_SETTINGS_CHANGED |
                          USB_TUNING_PROFILE_ACTION_MODE_TOGGLED;
            }
        }
        return result;
    default:
        return result;
    }
}

bool usb_tuning_profile_service_response_pending(const UsbTuningProfileService *service) {
    return service != NULL && service->response_pending;
}

void usb_tuning_profile_service_response_sent(UsbTuningProfileService *service) {
    service->response_pending = false;
}

void usb_tuning_profile_service_request_response_if_changed(
    UsbTuningProfileService *service, const TuningProfile *previous,
    const TuningProfile *current) {
    if (service != NULL && usb_tuning_profile_report_changed(previous, current)) {
        usb_tuning_profile_service_request_response(service);
    }
}

/**
 * @brief Requests publication of the active tuning profile.
 *
 * @param[in,out] service Tuning-profile service to update.
 */
void usb_tuning_profile_service_request_response(UsbTuningProfileService *service) {
    if (service != NULL) {
        service->response_pending = true;
    }
}
