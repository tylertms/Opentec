#include <assert.h>
#include <stdbool.h>
#include <stdint.h>

#include "usb/connection.h"

static void test_delays_disconnect_and_shows_notification(void) {
    UsbConnectionMonitor monitor;
    usb_connection_monitor_init(&monitor);

    assert(usb_connection_monitor_update(&monitor, true, true, 100) == USB_CONNECTION_ACTION_NONE);
    assert(!monitor.disconnected);
    assert(usb_connection_monitor_update(&monitor, false, true, 299) == USB_CONNECTION_ACTION_NONE);
    assert(!monitor.disconnected);

    UsbConnectionAction actions = usb_connection_monitor_update(&monitor, false, true, 300);
    assert(actions ==
           (USB_CONNECTION_ACTION_NOTIFY_DISCONNECTED | USB_CONNECTION_ACTION_SHOW_DISCONNECTED));
    assert(monitor.disconnected);
    assert(monitor.notification_requested);

    assert(usb_connection_monitor_update(&monitor, false, true, 2299) ==
           USB_CONNECTION_ACTION_SHOW_DISCONNECTED);
    assert(usb_connection_monitor_update(&monitor, false, true, 2300) ==
           USB_CONNECTION_ACTION_SHOW_DISCONNECTED);
    assert(usb_connection_monitor_update(&monitor, false, true, 2301) ==
           USB_CONNECTION_ACTION_CLEAR_NOTIFICATION);
    assert(usb_connection_monitor_update(&monitor, false, true, 2302) ==
           USB_CONNECTION_ACTION_NONE);
    assert(monitor.disconnected);
}

static void test_reconnect_rearms_notification(void) {
    UsbConnectionMonitor monitor;
    usb_connection_monitor_init(&monitor);

    usb_connection_monitor_update(&monitor, true, true, 0);
    usb_connection_monitor_update(&monitor, false, true, 200);
    assert(usb_connection_monitor_update(&monitor, true, true, 300) == USB_CONNECTION_ACTION_NONE);
    assert(!monitor.disconnected);
    assert(!monitor.notification_requested);
    assert(usb_connection_monitor_update(&monitor, true, true, 2201) ==
           USB_CONNECTION_ACTION_CLEAR_NOTIFICATION);

    assert(usb_connection_monitor_update(&monitor, false, true, 2400) ==
           USB_CONNECTION_ACTION_NONE);
    assert(usb_connection_monitor_update(&monitor, false, true, 2401) ==
           (USB_CONNECTION_ACTION_NOTIFY_DISCONNECTED | USB_CONNECTION_ACTION_SHOW_DISCONNECTED));
}

static void test_waits_until_notification_is_ready(void) {
    UsbConnectionMonitor monitor;
    usb_connection_monitor_init(&monitor);

    usb_connection_monitor_update(&monitor, true, true, 0);
    assert(usb_connection_monitor_update(&monitor, false, false, 200) ==
           USB_CONNECTION_ACTION_NONE);
    assert(!monitor.disconnected);

    assert(usb_connection_monitor_update(&monitor, false, true, 201) ==
           (USB_CONNECTION_ACTION_NOTIFY_DISCONNECTED | USB_CONNECTION_ACTION_SHOW_DISCONNECTED));
    assert(monitor.disconnected);
}

static void test_deadlines_survive_counter_wrap(void) {
    UsbConnectionMonitor monitor;
    usb_connection_monitor_init(&monitor);

    usb_connection_monitor_update(&monitor, true, true, UINT32_MAX - 99);
    assert(usb_connection_monitor_update(&monitor, false, true, 100) ==
           (USB_CONNECTION_ACTION_NOTIFY_DISCONNECTED | USB_CONNECTION_ACTION_SHOW_DISCONNECTED));
    assert(usb_connection_monitor_update(&monitor, false, true, 2100) ==
           USB_CONNECTION_ACTION_SHOW_DISCONNECTED);
    assert(usb_connection_monitor_update(&monitor, false, true, 2101) ==
           USB_CONNECTION_ACTION_CLEAR_NOTIFICATION);
}

int main(void) {
    test_delays_disconnect_and_shows_notification();
    test_reconnect_rearms_notification();
    test_waits_until_notification_is_ready();
    test_deadlines_survive_counter_wrap();
    return 0;
}
