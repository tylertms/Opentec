#include <assert.h>
#include <string.h>

#include "usb/remote_hid_queue.h"

static void assert_zero_report(const uint8_t report[USB_REMOTE_HID_REPORT_SIZE]) {
    for (size_t index = 0; index < USB_REMOTE_HID_REPORT_SIZE; index++) {
        assert(report[index] == 0);
    }
}

static UsbRemoteHidQueueRecord make_record(uint16_t flags, uint8_t value_low, uint8_t value_high,
                                           uint8_t data) {
    return (UsbRemoteHidQueueRecord){
        .flags = flags,
        .value_low = value_low,
        .value_high = value_high,
        .data = data,
    };
}

static void test_empty_report_is_zero_filled(void) {
    UsbRemoteHidQueue queue;
    uint8_t report[USB_REMOTE_HID_REPORT_SIZE];

    usb_remote_hid_queue_init(&queue);
    memset(report, 0xa5, sizeof(report));

    assert(!usb_remote_hid_queue_pending(&queue));
    assert(!usb_remote_hid_queue_encode(&queue, USB_REMOTE_HID_HOST_PLAYSTATION, report));
    assert_zero_report(report);
}

static void test_sparse_records_are_encoded_and_drained_in_order(void) {
    UsbRemoteHidQueue queue;
    UsbRemoteHidQueueRecord records[13];
    uint8_t report[USB_REMOTE_HID_REPORT_SIZE];

    usb_remote_hid_queue_init(&queue);
    for (uint8_t index = 0; index < 13; index++) {
        records[index] = make_record((uint16_t)(0x100 + index), index, (uint8_t)(index + 1),
                                     (uint8_t)(index + 2));
        assert(usb_remote_hid_queue_enqueue(&queue, &records[index]));
    }

    assert(usb_remote_hid_queue_pending(&queue));
    assert(usb_remote_hid_queue_encode(&queue, USB_REMOTE_HID_HOST_NATIVE, report));
    assert(report[0] == USB_REMOTE_HID_MARKER_NATIVE);
    assert(report[1] == USB_REMOTE_HID_REPORT_ID);
    assert(report[2] == USB_REMOTE_HID_REPORT_TYPE);
    for (uint8_t index = 0; index < USB_REMOTE_HID_REPORT_RECORD_LIMIT; index++) {
        assert(memcmp(report + USB_REMOTE_HID_REPORT_HEADER_SIZE +
                          index * USB_REMOTE_HID_QUEUE_RECORD_SIZE,
                      &records[index], sizeof(records[index])) == 0);
    }
    assert(usb_remote_hid_queue_pending(&queue));

    assert(usb_remote_hid_queue_encode(&queue, USB_REMOTE_HID_HOST_PLAYSTATION, report));
    assert(report[0] == USB_REMOTE_HID_MARKER_PLAYSTATION);
    assert(memcmp(report + USB_REMOTE_HID_REPORT_HEADER_SIZE, &records[12], sizeof(records[12])) ==
           0);
    assert(!usb_remote_hid_queue_pending(&queue));
}

static void test_invalid_records_and_full_queue_are_rejected(void) {
    UsbRemoteHidQueue queue;
    UsbRemoteHidQueueRecord empty = {0};
    UsbRemoteHidQueueRecord record = make_record(1, 2, 3, 4);
    uint8_t report[USB_REMOTE_HID_REPORT_SIZE];

    usb_remote_hid_queue_init(&queue);
    assert(!usb_remote_hid_queue_enqueue(&queue, NULL));
    assert(!usb_remote_hid_queue_enqueue(&queue, &empty));
    for (uint8_t index = 0; index < USB_REMOTE_HID_QUEUE_CAPACITY; index++) {
        record.flags = (uint16_t)(index + 1);
        assert(usb_remote_hid_queue_enqueue(&queue, &record));
    }
    assert(!usb_remote_hid_queue_enqueue(&queue, &record));
    assert(usb_remote_hid_queue_encode(&queue, USB_REMOTE_HID_HOST_XBOX, report));
}

int main(void) {
    test_empty_report_is_zero_filled();
    test_sparse_records_are_encoded_and_drained_in_order();
    test_invalid_records_and_full_queue_are_rejected();
    return 0;
}
