#include "usb/tuning_menu_service.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "usb/device.h"
#include "usb/vendor_command.h"

/** @brief Tuning-menu command and response constants. */
enum {
    TUNING_MENU_ACTION_SELECT_PAGE = 2,    /**< Select-page command action. */
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
    if (command->arguments[0] != TUNING_MENU_ACTION_SELECT_PAGE || command->length < 2) {
        return false;
    }

    uint8_t page = command->arguments[1];
    if (page >= USB_TUNING_MENU_PAGE_ROOT && page <= USB_TUNING_MENU_PAGE_AUXILIARY_CALIBRATION) {
        service->active_page = (UsbTuningMenuPage)page;
    }
    if (page != 0) {
        service->response_pending = true;
    }
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
    output[2] = (uint8_t)service->active_page;
}

void usb_tuning_menu_service_response_sent(UsbTuningMenuService *service) {
    service->response_pending = false;
}
