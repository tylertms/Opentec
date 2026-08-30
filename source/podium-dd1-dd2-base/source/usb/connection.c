#include "usb/connection.h"

#include <stdbool.h>
#include <stdint.h>

enum {
    USB_CONNECTION_DISCONNECT_DELAY_MS = 200,
    USB_CONNECTION_NOTIFICATION_MS = 2000,
};

/**
 * @brief Tests an inclusive connection-display deadline.
 *
 * Uses the unsigned millisecond ordering applied to the reconnect delay.
 *
 * @param[in] now_ms Current monotonic time in milliseconds.
 * @param[in] deadline_ms Absolute millisecond deadline.
 * @return True when the current time is at or beyond the deadline; otherwise false.
 */
static bool deadline_reached(uint32_t now_ms, uint32_t deadline_ms) {
    return now_ms >= deadline_ms;
}

/**
 * @brief Tests a strict connection-display deadline.
 *
 * Uses the unsigned millisecond ordering applied when the notification page is cleared.
 *
 * @param[in] now_ms Current monotonic time in milliseconds.
 * @param[in] deadline_ms Absolute millisecond deadline.
 * @return True when the current time is beyond the deadline; otherwise false.
 */
static bool deadline_passed(uint32_t now_ms, uint32_t deadline_ms) { return now_ms > deadline_ms; }

/**
 * @brief Initializes USB connection monitoring.
 *
 * Clears the disconnect delay, notification deadline, and latched connection state.
 *
 * @param[out] monitor USB connection monitor to initialize.
 */
void usb_connection_monitor_init(UsbConnectionMonitor *monitor) {
    *monitor = (UsbConnectionMonitor){0};
}

/**
 * @brief Debounces USB disconnection and schedules its display notification.
 *
 * A connected sample clears the disconnected state and starts a 200-millisecond disconnect delay.
 * A sustained disconnected sample emits one notification request, keeps the notification visible
 * for 2000 milliseconds, and finally emits one clear action. The disconnected state remains set
 * until the connection input returns.
 *
 * @param[in,out] monitor Persistent disconnect and notification state.
 * @param[in] connected True while USB VBUS is present.
 * @param[in] notification_ready True when the display can accept the disconnect notification.
 * @param[in] now_ms Current monotonic time in milliseconds.
 * @return Bitwise combination of notify, show, and clear actions for this update.
 */
UsbConnectionAction usb_connection_monitor_update(UsbConnectionMonitor *monitor, bool connected,
                                                  bool notification_ready, uint32_t now_ms) {
    UsbConnectionAction actions = USB_CONNECTION_ACTION_NONE;

    if (connected) {
        monitor->disconnected = false;
        monitor->notification_requested = false;
        monitor->disconnect_ready_ms = now_ms + USB_CONNECTION_DISCONNECT_DELAY_MS;
    } else if (notification_ready && deadline_reached(now_ms, monitor->disconnect_ready_ms)) {
        if (!monitor->notification_requested) {
            actions |= USB_CONNECTION_ACTION_NOTIFY_DISCONNECTED;
            monitor->notification_clear_ms = now_ms + USB_CONNECTION_NOTIFICATION_MS;
            monitor->notification_requested = true;
        }
        if (!deadline_passed(now_ms, monitor->notification_clear_ms)) {
            actions |= USB_CONNECTION_ACTION_SHOW_DISCONNECTED;
        }
        monitor->disconnected = true;
    }

    if (monitor->notification_clear_ms != 0 &&
        deadline_passed(now_ms, monitor->notification_clear_ms)) {
        actions |= USB_CONNECTION_ACTION_CLEAR_NOTIFICATION;
        monitor->notification_clear_ms = 0;
    }

    return actions;
}
