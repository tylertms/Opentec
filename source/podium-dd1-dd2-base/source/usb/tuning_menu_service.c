#include "usb/tuning_menu_service.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "usb/device.h"
#include "usb/vendor_command.h"

/** @brief Tuning-menu command and response constants. */
enum {
    TUNING_MENU_ACTION_NAVIGATE = 2,       /**< Menu-navigation command action. */
    TUNING_MENU_ACTION_REFRESH_STATUS = 3, /**< Menu-status refresh command action. */
    TUNING_MENU_STATUS_COMMAND = 2,        /**< Status-response command identifier. */
    TUNING_MENU_SERVICE_CODE_REPEAT = 7,   /**< Service code exempt from duplicate suppression. */
};

void usb_tuning_menu_service_init(UsbTuningMenuService *service) {
    if (service != NULL) {
        *service = (UsbTuningMenuService){.active_page = USB_TUNING_MENU_PAGE_ROOT};
    }
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
    if (command->arguments[0] != TUNING_MENU_ACTION_NAVIGATE || command->length < 2) {
        return false;
    }

    uint8_t selection = command->arguments[1];
    if (selection >= 1 && selection <= 6) {
        if (service->active_page != (UsbTuningMenuPage)selection) {
            service->active_page = (UsbTuningMenuPage)selection;
            service->response_pending = true;
        }
    }
    return true;
}

bool usb_tuning_menu_service_request_native_service_response(UsbTuningMenuService *service) {
    if (service == NULL) {
        return false;
    }

    uint8_t service_code = (uint8_t)service->active_page;
    if (service_code == service->last_service_code &&
        service_code != TUNING_MENU_SERVICE_CODE_REPEAT) {
        return false;
    }
    service->service_code = service_code;
    service->last_service_code = service_code;
    service->service_response_pending = true;
    return true;
}

bool usb_tuning_menu_service_response_pending(const UsbTuningMenuService *service) {
    return service != NULL && (service->response_pending || service->service_response_pending);
}

void usb_tuning_menu_service_encode_response(const UsbTuningMenuService *service,
                                             uint8_t output[USB_DEVICE_REPORT_SIZE]) {
    for (uint8_t index = 0; index < USB_DEVICE_REPORT_SIZE; index++) {
        output[index] = 0;
    }
    output[0] = UINT8_MAX;
    output[1] = TUNING_MENU_STATUS_COMMAND;
    output[2] =
        service->service_response_pending ? service->service_code : (uint8_t)service->active_page;
}

void usb_tuning_menu_service_response_sent(UsbTuningMenuService *service) {
    if (service != NULL) {
        service->response_pending = false;
        service->service_response_pending = false;
    }
}
