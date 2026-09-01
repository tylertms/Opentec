#ifndef OPENTEC_BASE_USB_HOST_CAPABILITY_RECOVERY_H
#define OPENTEC_BASE_USB_HOST_CAPABILITY_RECOVERY_H

#include <stdbool.h>
#include <stdint.h>

/** @brief Inputs that select and gate Xbox host-capability recovery. */
typedef struct {
    uint8_t wheel_mode;               /**< Attached-wheel protocol mode. */
    uint16_t wheel_capability_flags;  /**< Attached-wheel host-capability flags. */
    bool xbox_mode;                   /**< True when the USB device uses Xbox mode. */
    bool host_capability_enabled;     /**< True when the host capability is already enabled. */
    bool adapter_requests_capability; /**< True when an attached adapter requests the capability. */
} UsbHostCapabilityRecoveryInput;

/** @brief Persistent deadline state for Xbox host-capability recovery. */
typedef struct {
    uint32_t deadline_ms; /**< Earliest time at which recovery may signal USB resume. */
} UsbHostCapabilityRecovery;

/** @brief Actions returned by Xbox host-capability recovery. */
typedef enum {
    USB_HOST_CAPABILITY_RECOVERY_NONE,          /**< No USB recovery action is required. */
    USB_HOST_CAPABILITY_RECOVERY_SIGNAL_RESUME, /**< Signal USB resume to recover the capability. */
} UsbHostCapabilityRecoveryAction;

/**
 * @brief Initializes host-capability recovery state.
 *
 * Clears the recovery deadline. The first update outside an applicable Xbox configuration arms the
 * delay used before recovery signaling.
 *
 * @param[out] recovery Recovery state to initialize.
 */
void usb_host_capability_recovery_init(UsbHostCapabilityRecovery *recovery);

/**
 * @brief Updates Xbox host-capability recovery state.
 *
 * Arms the recovery delay outside Xbox mode or when no applicable wheel or adapter capability is
 * requested. In an applicable Xbox configuration, returns a resume action only after the deadline
 * has passed and the host capability is still disabled, then schedules the retry delay.
 *
 * @param[in,out] recovery Persistent recovery deadline.
 * @param[in] input Current USB mode and host-capability inputs.
 * @param[in] now_ms Current monotonic time in milliseconds.
 * @return The required recovery action, with resume signaling only when recovery is due; otherwise
 * no action.
 */
UsbHostCapabilityRecoveryAction
usb_host_capability_recovery_update(UsbHostCapabilityRecovery *recovery,
                                    UsbHostCapabilityRecoveryInput input, uint32_t now_ms);

#endif
