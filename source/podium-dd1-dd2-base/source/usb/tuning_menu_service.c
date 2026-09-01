#include "usb/tuning_menu_service.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "usb/device.h"
#include "usb/vendor_command.h"

/** @brief Tuning-menu command and response constants. */
enum {
    TUNING_MENU_ACTION_SELECT_PROFILE = 2,
    TUNING_MENU_ACTION_REFRESH_STATUS = 3, /**< Refresh-status command action. */
    TUNING_MENU_STATUS_COMMAND = 2,        /**< Status-response command identifier. */
};

void usb_tuning_menu_service_init(UsbTuningMenuService *service) {
    *service = (UsbTuningMenuService){.active_page = USB_TUNING_MENU_PAGE_ROOT};
}

bool usb_tuning_menu_service_apply(UsbTuningMenuService *service, const UsbVendorCommand *command) {
    if (service == NULL || command == NULL || command->kind != USB_VENDOR_COMMAND_TUNING_MENU ||
        command->arguments == NULL || command->length == 0) {
        return false;
    }

    if (command->arguments[0] == TUNING_MENU_ACTION_REFRESH_STATUS) {
        service->response_pending = true;
        return true;
    }
    if (command->arguments[0] != TUNING_MENU_ACTION_SELECT_PROFILE || command->length < 2) {
        return false;
    }

    uint8_t selection = command->arguments[1];
    if (selection >= 1 && selection <= 6) {
        service->selected_profile = selection;
        service->profile_selection_pending = true;
        service->response_pending = true;
    }
    return true;
}

/**
 * @brief Takes a pending tuning-profile selection.
 *
 * @param[in,out] service Tuning-menu service retaining the selection.
 * @param[out] selection Destination for the one-based profile selection.
 * @return True when a selection was returned; otherwise false.
 */
bool usb_tuning_menu_service_take_profile_selection(UsbTuningMenuService *service,
                                                    uint8_t *selection) {
    if (service == NULL || selection == NULL || !service->profile_selection_pending) {
        return false;
    }
    *selection = service->selected_profile;
    service->profile_selection_pending = false;
    return true;
}

bool usb_tuning_menu_service_response_pending(const UsbTuningMenuService *service) {
    return service != NULL && service->response_pending;
}

void usb_tuning_menu_service_encode_response(const UsbTuningMenuService *service,
                                             uint8_t output[USB_DEVICE_REPORT_SIZE]) {
    for (uint8_t index = 0; index < USB_DEVICE_REPORT_SIZE; index++) {
        output[index] = 0;
    }
    output[0] = UINT8_MAX;
    output[1] = TUNING_MENU_STATUS_COMMAND;
    output[2] = service->selected_profile;
}

void usb_tuning_menu_service_response_sent(UsbTuningMenuService *service) {
    service->response_pending = false;
}
