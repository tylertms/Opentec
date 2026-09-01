#ifndef OPENTEC_BASE_USB_TUNING_PROFILE_SERVICE_H
#define OPENTEC_BASE_USB_TUNING_PROFILE_SERVICE_H

#include <stdbool.h>
#include <stdint.h>

#include "profile/bank.h"
#include "usb/vendor_command.h"

/** @brief Timing limits for tuning-profile reset and mode commands. */
enum {
    USB_TUNING_PROFILE_RESET_DELAY_MS =
        10000, /**< Minimum delay between reset commands, in milliseconds. */
    USB_TUNING_PROFILE_MODE_DELAY_MS =
        2000, /**< Minimum delay between mode changes, in milliseconds. */
};

/**
 * @brief Actions produced by a tuning-profile command.
 *
 * Values are bit flags and may be combined to describe multiple effects of one command.
 */
typedef enum {
    USB_TUNING_PROFILE_ACTION_NONE = 0,                 /**< No action was produced. */
    USB_TUNING_PROFILE_ACTION_CLAIM = 1 << 0,           /**< Command route was claimed. */
    USB_TUNING_PROFILE_ACTION_PROFILE_CHANGED = 1 << 1, /**< Active profile changed. */
    USB_TUNING_PROFILE_ACTION_SAVE = 1 << 2,            /**< Profile persistence should be saved. */
    USB_TUNING_PROFILE_ACTION_MODE_CHANGED = 1 << 3,    /**< Standard or Advanced mode changed. */
    USB_TUNING_PROFILE_ACTION_SETTINGS_CHANGED = 1 << 4, /**< Profile settings changed. */
    USB_TUNING_PROFILE_ACTION_RESET_COMPLETED = 1 << 5,  /**< A guarded reset completed. */
    USB_TUNING_PROFILE_ACTION_MODE_TOGGLED = 1 << 6,     /**< Mode toggle completed. */
} UsbTuningProfileAction;

/**
 * @brief Tuning-profile command timing and response state.
 *
 * Retains command guards and the latch used to request the active profile response.
 */
typedef struct {
    uint32_t reset_after_ms; /**< Earliest monotonic time at which another reset is allowed. */
    uint32_t mode_change_after_ms; /**< Earliest monotonic time at which another mode change is
                                      allowed. */
    bool response_pending;         /**< True when an active-profile response is pending. */
} UsbTuningProfileService;

/**
 * @brief Initializes tuning-profile command state.
 *
 * Clears command timing guards and requests an initial active-profile response.
 *
 * @param[out] service Command state to initialize.
 */
void usb_tuning_profile_service_init(UsbTuningProfileService *service);

/**
 * @brief Applies one device-control tuning-profile command.
 *
 * Handles profile updates, selection, refresh, save, guarded resets, and Standard or Advanced mode
 * toggles, returning bit flags for the effects that the firmware must service.
 *
 * @param[in,out] service Command timing and response state to update.
 * @param[in,out] bank Profile bank to update.
 * @param[in] command Decoded device-control command.
 * @param[in] now_ms Current monotonic time in milliseconds.
 * @return Combined action flags; returns #USB_TUNING_PROFILE_ACTION_NONE when the command pointers,
 * route, or arguments are invalid.
 */
UsbTuningProfileAction usb_tuning_profile_service_apply(UsbTuningProfileService *service,
                                                        TuningProfileBank *bank,
                                                        const UsbVendorCommand *command,
                                                        uint32_t now_ms);

/**
 * @brief Reports whether an active-profile response is pending.
 *
 * Reads the response latch without changing command or profile state.
 *
 * @param[in] service Command state to inspect.
 * @return True when an active-profile response is pending; otherwise false.
 */
bool usb_tuning_profile_service_response_pending(const UsbTuningProfileService *service);

/**
 * @brief Marks an active-profile response as sent.
 *
 * Clears the response latch after the complete profile report is accepted by the USB endpoint.
 *
 * @param[in,out] service Command state to update.
 */
void usb_tuning_profile_service_response_sent(UsbTuningProfileService *service);

#endif
