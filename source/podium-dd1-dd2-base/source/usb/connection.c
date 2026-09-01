#include "usb/connection.h"

#include <stdbool.h>
#include <stdint.h>

/** @brief USB connection debounce and notification intervals. */
enum {
    USB_CONNECTION_DISCONNECT_DELAY_MS = 200, /**< Disconnect debounce interval in milliseconds. */
    USB_CONNECTION_NOTIFICATION_MS = 2000, /**< Disconnect notification interval in milliseconds. */
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

void usb_connection_monitor_init(UsbConnectionMonitor *monitor) {
    *monitor = (UsbConnectionMonitor){0};
}

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
