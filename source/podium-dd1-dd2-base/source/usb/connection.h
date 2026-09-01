#ifndef OPENTEC_BASE_USB_CONNECTION_H
#define OPENTEC_BASE_USB_CONNECTION_H

#include <stdbool.h>
#include <stdint.h>

/** @brief Actions emitted while USB disconnect notification state advances. */
typedef enum {
    USB_CONNECTION_ACTION_NONE = 0, /**< No connection-monitor action is required. */
    USB_CONNECTION_ACTION_NOTIFY_DISCONNECTED = 1 << 0, /**< Request notification preparation. */
    USB_CONNECTION_ACTION_SHOW_DISCONNECTED = 1 << 1,   /**< Request notification display. */
    USB_CONNECTION_ACTION_CLEAR_NOTIFICATION = 1 << 2,  /**< Request notification clearing. */
} UsbConnectionAction;

/** @brief Debounce and display deadlines retained by the USB connection monitor. */
typedef struct {
    uint32_t disconnect_ready_ms;   /**< Time when a sustained disconnect becomes reportable. */
    uint32_t notification_clear_ms; /**< Time when the notification may be cleared. */
    bool notification_requested;    /**< True after the notification request is emitted. */
    bool disconnected; /**< True after a debounced disconnect is eligible for notification. */
} UsbConnectionMonitor;

/**
 * @brief Initializes USB connection monitoring.
 *
 * Clears the disconnect delay, notification deadline, and latched connection state.
 *
 * @param[out] monitor USB connection monitor to initialize.
 */
void usb_connection_monitor_init(UsbConnectionMonitor *monitor);

/**
 * @brief Debounces USB disconnection and schedules its display notification.
 *
 * A connected sample clears disconnect state and starts a 200-millisecond disconnect delay. A
 * sustained disconnect emits notification actions once the display is ready and clears the
 * notification after its display interval.
 *
 * @param[in,out] monitor Persistent disconnect and notification state.
 * @param[in] connected True while USB VBUS is present.
 * @param[in] notification_ready True when the display can accept the disconnect notification.
 * @param[in] now_ms Current monotonic time in milliseconds.
 * @return Bitwise combination of notify, show, and clear actions for this update.
 */
UsbConnectionAction usb_connection_monitor_update(UsbConnectionMonitor *monitor, bool connected,
                                                  bool notification_ready, uint32_t now_ms);

#endif
