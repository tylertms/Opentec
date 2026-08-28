#include "usb/tuning_menu_service.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "usb/device.h"
#include "usb/vendor_command.h"

enum {
    TUNING_MENU_ACTION_SELECT_PAGE = 2,
    TUNING_MENU_ACTION_REFRESH_STATUS = 3,
    TUNING_MENU_STATUS_COMMAND = 2,
};

/**
 * @brief Initializes the USB tuning-menu state.
 *
 * Selects the root page and leaves the page-status response idle.
 *
 * @param[out] service Tuning-menu state to initialize.
 */
void usb_tuning_menu_service_init(UsbTuningMenuService *service) {
    *service = (UsbTuningMenuService){.active_page = USB_TUNING_MENU_PAGE_ROOT};
}

/**
 * @brief Applies a tuning-menu page or status command.
 *
 * Action two selects page 1 through 6 and requests a status response for every nonzero selector.
 * Unsupported selectors leave the current page unchanged. Action three requests a status response
 * without changing the page.
 *
 * @param[in,out] service Current tuning-menu page and response state.
 * @param[in] command Decoded vendor command.
 * @return True when the command contains a complete supported tuning-menu action.
 */
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

/**
 * @brief Reports whether a tuning-menu page response is due.
 *
 * Returns the response latch set by a nonzero page selection or explicit status refresh.
 *
 * @param[in] service Tuning-menu response state.
 * @return True when the current page status must be sent.
 */
bool usb_tuning_menu_service_response_pending(const UsbTuningMenuService *service) {
    return service != NULL && service->response_pending;
}

/**
 * @brief Encodes the tuning-menu page response.
 *
 * Clears the native vendor report, writes the FF 02 header, and appends the active one-based page
 * identifier.
 *
 * @param[in] service Current tuning-menu page.
 * @param[out] output Encoded 64-byte vendor report.
 */
void usb_tuning_menu_service_encode_response(const UsbTuningMenuService *service,
                                             uint8_t output[USB_DEVICE_REPORT_SIZE]) {
    for (uint8_t index = 0; index < USB_DEVICE_REPORT_SIZE; index++) {
        output[index] = 0;
    }
    output[0] = UINT8_MAX;
    output[1] = TUNING_MENU_STATUS_COMMAND;
    output[2] = (uint8_t)service->active_page;
}

/**
 * @brief Completes a tuning-menu page response transfer.
 *
 * Clears the response latch after the complete report has been accepted by the USB endpoint.
 *
 * @param[in,out] service Tuning-menu response state.
 */
void usb_tuning_menu_service_response_sent(UsbTuningMenuService *service) {
    service->response_pending = false;
}
