#ifndef OPENTEC_BASE_USB_TUNING_MENU_SERVICE_H
#define OPENTEC_BASE_USB_TUNING_MENU_SERVICE_H

#include <stdbool.h>
#include <stdint.h>

#include "usb/device.h"
#include "usb/vendor_command.h"

/**
 * @brief Stable one-based page identifiers used by the native tuning-menu protocol.
 *
 * Values are sent in the page-status response and select the same six entries used by the
 * Fanatec menu controller.
 */
typedef enum {
    USB_TUNING_MENU_PAGE_ROOT = 1,                  /**< Root menu entry. */
    USB_TUNING_MENU_PAGE_WHEEL_INPUT = 2,           /**< Wheel-input menu entry. */
    USB_TUNING_MENU_PAGE_AUXILIARY_POSITION = 3,    /**< Auxiliary-position menu entry. */
    USB_TUNING_MENU_PAGE_FORCE_FEEDBACK = 4,        /**< Force-feedback menu entry. */
    USB_TUNING_MENU_PAGE_WHEEL_ACCESSORY = 5,       /**< Wheel-accessory menu entry. */
    USB_TUNING_MENU_PAGE_AUXILIARY_CALIBRATION = 6, /**< Auxiliary-calibration menu entry. */
    USB_TUNING_MENU_PAGE_SYSTEM_INFORMATION = USB_TUNING_MENU_PAGE_WHEEL_INPUT,
    USB_TUNING_MENU_PAGE_FORCE_FEEDBACK_ANALYSIS = USB_TUNING_MENU_PAGE_FORCE_FEEDBACK,
    USB_TUNING_MENU_PAGE_MOTOR_DATA_ANALYSIS = USB_TUNING_MENU_PAGE_WHEEL_ACCESSORY,
    USB_TUNING_MENU_PAGE_THERMAL_POWER = USB_TUNING_MENU_PAGE_AUXILIARY_POSITION,
} UsbTuningMenuPage;

/**
 * @brief Active tuning-menu page and pending USB status state.
 *
 * Tracks the page owned by the foreground menu controller, the raw action-two selection waiting for
 * that controller, and the native service-response duplicate suppression state.
 */
typedef struct {
    UsbTuningMenuPage active_page; /**< Page currently selected by the foreground controller. */
    uint8_t pending_selection;     /**< Raw action-two selection awaiting foreground consumption. */
    uint8_t last_selection;    /**< Last action-two selection accepted for duplicate suppression. */
    uint8_t service_code;      /**< Service code encoded by the next native response. */
    uint8_t last_service_code; /**< Last service code sent for duplicate suppression. */
    bool response_pending;     /**< True when a menu status response must be encoded. */
    bool service_response_pending; /**< True when a native service response is pending. */
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
 * Stages action-two menu navigation for the foreground controller and handles action-three status
 * refreshes. Every action-three request arms a status response, including repeated requests;
 * unsupported nonzero selectors remain pending until the controller consumes them without changing
 * the active entry.
 *
 * @param[in,out] service Tuning-menu state to update.
 * @param[in] command Decoded tuning-menu vendor command.
 * @return True when the command has a recognized action and required arguments; otherwise false.
 */
bool usb_tuning_menu_service_apply(UsbTuningMenuService *service, const UsbVendorCommand *command);

/**
 * @brief Consumes one pending action-two page selection at the foreground controller boundary.
 *
 * Maps selections one through six to menu pages, clears every nonzero raw selection including
 * unmapped values, and marks the menu status response pending. The active page is unchanged for
 * an unmapped selection.
 *
 * @param[in,out] service Tuning-menu state retaining the pending selection.
 * @return True when a nonzero selection was consumed; otherwise false for null or idle state.
 */
bool usb_tuning_menu_service_consume_pending_selection(UsbTuningMenuService *service);

/**
 * @brief Requests the native service response for the active menu entry.
 *
 * Applies the official duplicate suppression rule for the dedicated native service command.
 * Service code seven is always allowed to repeat; other codes are suppressed until the active
 * entry changes. Action-three status refreshes use the unconditional status path instead.
 *
 * @param[in,out] service Tuning-menu state retaining the active entry.
 * @return True when a response was armed; otherwise false for a duplicate request or null state.
 */
bool usb_tuning_menu_service_request_native_service_response(UsbTuningMenuService *service);

/**
 * @brief Reports whether a tuning-menu status response is pending.
 *
 * Reads either response latch without changing the selected page or pending state.
 *
 * @param[in] service Tuning-menu state to inspect.
 * @return True when a status response is pending; otherwise false.
 */
bool usb_tuning_menu_service_response_pending(const UsbTuningMenuService *service);

/**
 * @brief Encodes the pending tuning-menu status response.
 *
 * Clears the output and writes the fixed vendor header followed by the pending native service code
 * or active one-based menu identifier.
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
