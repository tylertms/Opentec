#ifndef OPENTEC_BASE_USB_TUNING_MENU_SERVICE_H
#define OPENTEC_BASE_USB_TUNING_MENU_SERVICE_H

#include <stdbool.h>
#include <stdint.h>

#include "usb/device.h"
#include "usb/vendor_command.h"

/** @brief Stable one-based page identifiers used by the tuning-menu protocol. */
typedef enum {
    USB_TUNING_MENU_PAGE_ROOT = 1,
    USB_TUNING_MENU_PAGE_WHEEL_INPUT = 2,
    USB_TUNING_MENU_PAGE_AUXILIARY_POSITION = 3,
    USB_TUNING_MENU_PAGE_FORCE_FEEDBACK = 4,
    USB_TUNING_MENU_PAGE_WHEEL_ACCESSORY = 5,
    USB_TUNING_MENU_PAGE_AUXILIARY_CALIBRATION = 6,
} UsbTuningMenuPage;

/** @brief Active tuning-menu page and pending USB status state. */
typedef struct {
    UsbTuningMenuPage active_page;
    bool response_pending;
} UsbTuningMenuService;

void usb_tuning_menu_service_init(UsbTuningMenuService *service);
bool usb_tuning_menu_service_apply(UsbTuningMenuService *service, const UsbVendorCommand *command);
bool usb_tuning_menu_service_response_pending(const UsbTuningMenuService *service);
void usb_tuning_menu_service_encode_response(const UsbTuningMenuService *service,
                                             uint8_t output[USB_DEVICE_REPORT_SIZE]);
void usb_tuning_menu_service_response_sent(UsbTuningMenuService *service);

#endif
