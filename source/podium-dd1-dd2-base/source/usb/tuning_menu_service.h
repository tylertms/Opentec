#ifndef OPENTEC_BASE_USB_TUNING_MENU_SERVICE_H
#define OPENTEC_BASE_USB_TUNING_MENU_SERVICE_H

#include <stdbool.h>
#include <stdint.h>

#include "usb/device.h"
#include "usb/vendor_command.h"

/**
 * @brief Stable one-based page identifiers used by the tuning-menu protocol.
 *
 * Values are sent in the page-status response.
 */
typedef enum {
    USB_TUNING_MENU_PAGE_ROOT = 1,                    /**< Root tuning page. */
    USB_TUNING_MENU_PAGE_SYSTEM_INFORMATION = 2,      /**< System-information page. */
    USB_TUNING_MENU_PAGE_FORCE_FEEDBACK_ANALYSIS = 3, /**< Force-feedback analysis page. */
    USB_TUNING_MENU_PAGE_MOTOR_DATA_ANALYSIS = 4,     /**< Motor-data analysis page. */
    USB_TUNING_MENU_PAGE_THERMAL_POWER = 5,           /**< Thermal and power page. */
    USB_TUNING_MENU_PAGE_AUXILIARY_CALIBRATION = 6,   /**< Auxiliary-calibration page. */
} UsbTuningMenuPage;

/**
 * @brief Active tuning-menu page and pending USB status state.
 *
 * Tracks the page encoded in the next status response and whether that response is due.
 */
typedef struct {
    UsbTuningMenuPage active_page;  /**< Currently selected one-based page. */
    uint8_t selected_profile;       /**< One-based profile selected by the latest command. */
    bool profile_selection_pending; /**< True while the profile selection awaits consumption. */
    bool response_pending;          /**< True when a page-status response must be encoded. */
} UsbTuningMenuService;

/**
 * @brief Initializes the USB tuning-menu service.
 *
 * Selects the root page and clears the pending-response state.
 *
 * @param[out] service Tuning-menu state to initialize.
 */
void usb_tuning_menu_service_init(UsbTuningMenuService *service);

/**
 * @brief Applies one tuning-menu command.
 *
 * Handles page selection and explicit status refresh actions while retaining the current page for
 * unsupported page selectors.
 *
 * @param[in,out] service Tuning-menu state to update.
 * @param[in] command Decoded tuning-menu vendor command.
 * @return True when the command has a recognized action and required arguments; otherwise false.
 */
bool usb_tuning_menu_service_apply(UsbTuningMenuService *service, const UsbVendorCommand *command);

/**
 * @brief Takes a pending one-based tuning-profile selection.
 *
 * @param[in,out] service Tuning-menu state retaining the selection.
 * @param[out] selection Destination for the selected one-based profile.
 * @return True when a pending selection was returned; otherwise false.
 */
bool usb_tuning_menu_service_take_profile_selection(UsbTuningMenuService *service,
                                                    uint8_t *selection);

/**
 * @brief Reports whether a tuning-menu status response is pending.
 *
 * Reads the response latch without changing the selected page or pending state.
 *
 * @param[in] service Tuning-menu state to inspect.
 * @return True when a status response is pending; otherwise false.
 */
bool usb_tuning_menu_service_response_pending(const UsbTuningMenuService *service);

/**
 * @brief Encodes the pending tuning-menu status response.
 *
 * Clears the output and writes the fixed vendor header followed by the active one-based page
 * identifier.
 *
 * @param[in] service Tuning-menu state providing the active page.
 * @param[out] output Destination for the complete USB report.
 */
void usb_tuning_menu_service_encode_response(const UsbTuningMenuService *service,
                                             uint8_t output[USB_DEVICE_REPORT_SIZE]);

/**
 * @brief Marks a tuning-menu status response as sent.
 *
 * Clears the response latch after the encoded report has been accepted by the USB endpoint.
 *
 * @param[in,out] service Tuning-menu state to update.
 */
void usb_tuning_menu_service_response_sent(UsbTuningMenuService *service);

#endif
