#ifndef OPENTEC_BASE_USB_CONNECTION_H
#define OPENTEC_BASE_USB_CONNECTION_H

#include <stdbool.h>
#include <stdint.h>

typedef enum {
    USB_CONNECTION_ACTION_NONE = 0,
    USB_CONNECTION_ACTION_NOTIFY_DISCONNECTED = 1 << 0,
    USB_CONNECTION_ACTION_SHOW_DISCONNECTED = 1 << 1,
    USB_CONNECTION_ACTION_CLEAR_NOTIFICATION = 1 << 2,
} UsbConnectionAction;

typedef struct {
    uint32_t disconnect_ready_ms;
    uint32_t notification_clear_ms;
    bool notification_requested;
    bool disconnected;
} UsbConnectionMonitor;

void usb_connection_monitor_init(UsbConnectionMonitor *monitor);
UsbConnectionAction usb_connection_monitor_update(UsbConnectionMonitor *monitor, bool connected,
                                                  bool notification_ready, uint32_t now_ms);

#endif
