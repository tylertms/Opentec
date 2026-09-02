#include "usb/remote_hid_queue.h"

#include <stddef.h>
#include <string.h>

static uint8_t marker_for_host(UsbRemoteHidHost host) {
    switch (host) {
    case USB_REMOTE_HID_HOST_NATIVE:
        return USB_REMOTE_HID_MARKER_NATIVE;
    case USB_REMOTE_HID_HOST_PLAYSTATION:
        return USB_REMOTE_HID_MARKER_PLAYSTATION;
    case USB_REMOTE_HID_HOST_XBOX:
        return USB_REMOTE_HID_MARKER_XBOX;
    default:
        return 0;
    }
}

void usb_remote_hid_queue_init(UsbRemoteHidQueue *queue) {
    if (queue != NULL) {
        memset(queue, 0, sizeof(*queue));
    }
}

bool usb_remote_hid_queue_enqueue(UsbRemoteHidQueue *queue, const UsbRemoteHidQueueRecord *record) {
    if (queue == NULL || record == NULL || record->flags == 0) {
        return false;
    }
    for (uint8_t index = 0; index < USB_REMOTE_HID_QUEUE_CAPACITY; index++) {
        if (queue->records[index].flags == 0) {
            queue->records[index] = *record;
            return true;
        }
    }
    return false;
}

bool usb_remote_hid_queue_pending(const UsbRemoteHidQueue *queue) {
    if (queue == NULL) {
        return false;
    }
    for (uint8_t index = 0; index < USB_REMOTE_HID_QUEUE_CAPACITY; index++) {
        if (queue->records[index].flags != 0) {
            return true;
        }
    }
    return false;
}

bool usb_remote_hid_queue_encode(UsbRemoteHidQueue *queue, UsbRemoteHidHost host,
                                 uint8_t report[USB_REMOTE_HID_REPORT_SIZE]) {
    if (queue == NULL || report == NULL || marker_for_host(host) == 0) {
        return false;
    }

    memset(report, 0, USB_REMOTE_HID_REPORT_SIZE);
    uint8_t count = 0;
    for (uint8_t index = 0;
         index < USB_REMOTE_HID_QUEUE_CAPACITY && count < USB_REMOTE_HID_REPORT_RECORD_LIMIT;
         index++) {
        UsbRemoteHidQueueRecord *record = &queue->records[index];
        if (record->flags == 0) {
            continue;
        }
        memcpy(report + USB_REMOTE_HID_REPORT_HEADER_SIZE +
                   count * USB_REMOTE_HID_QUEUE_RECORD_SIZE,
               record, sizeof(*record));
        *record = (UsbRemoteHidQueueRecord){0};
        count++;
    }
    if (count == 0) {
        return false;
    }

    report[0] = marker_for_host(host);
    report[1] = USB_REMOTE_HID_REPORT_ID;
    report[2] = USB_REMOTE_HID_REPORT_TYPE;
    return true;
}
