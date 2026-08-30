#include <assert.h>
#include <stdint.h>
#include <string.h>

#include "usb/transfer_response.h"

static void fill_payload(uint8_t *payload, uint8_t length) {
    for (uint8_t index = 0; index < length; index++) {
        payload[index] = index;
    }
}

static void test_encodes_single_response(void) {
    UsbTransferResponse response;
    uint8_t payload[62];
    uint8_t report[USB_DEVICE_REPORT_SIZE];
    fill_payload(payload, sizeof(payload));
    usb_transfer_response_init(&response);

    assert(usb_transfer_response_accepting(&response));
    assert(usb_transfer_response_queue(&response, payload, sizeof(payload)));
    assert(usb_transfer_response_prepare(&response, report));
    assert(report[0] == 0xff);
    assert(report[1] == 0x10);
    assert(memcmp(report + 2, payload, sizeof(payload)) == 0);
    assert(!usb_transfer_response_accepting(&response));

    usb_transfer_response_commit(&response);
    assert(usb_transfer_response_accepting(&response));
    assert(!usb_transfer_response_pending(&response));
}

static void test_encodes_fragmented_response(void) {
    UsbTransferResponse response;
    uint8_t payload[124];
    uint8_t report[USB_DEVICE_REPORT_SIZE];
    fill_payload(payload, sizeof(payload));
    usb_transfer_response_init(&response);
    assert(usb_transfer_response_queue(&response, payload, sizeof(payload)));

    assert(usb_transfer_response_prepare(&response, report));
    assert(report[0] == 0xff);
    assert(report[1] == 0x11);
    assert(report[2] == 0);
    assert(report[3] == sizeof(payload));
    assert(memcmp(report + 4, payload, 60) == 0);
    usb_transfer_response_commit(&response);

    assert(usb_transfer_response_prepare(&response, report));
    assert(report[0] == 0xff);
    assert(report[1] == 0x12);
    assert(report[2] == 1);
    assert(memcmp(report + 3, payload + 60, 61) == 0);
    usb_transfer_response_commit(&response);

    assert(usb_transfer_response_prepare(&response, report));
    assert(report[0] == 0xff);
    assert(report[1] == 0x13);
    assert(report[2] == 2);
    assert(memcmp(report + 3, payload + 121, 3) == 0);
    usb_transfer_response_commit(&response);
    assert(usb_transfer_response_accepting(&response));
}

static void test_retains_prepared_report_until_commit(void) {
    UsbTransferResponse response;
    const uint8_t payload[] = {0xaa, 0xbb};
    uint8_t first[USB_DEVICE_REPORT_SIZE];
    uint8_t retry[USB_DEVICE_REPORT_SIZE];
    usb_transfer_response_init(&response);
    assert(usb_transfer_response_queue(&response, payload, sizeof(payload)));

    assert(usb_transfer_response_prepare(&response, first));
    assert(usb_transfer_response_prepare(&response, retry));
    assert(memcmp(first, retry, sizeof(first)) == 0);
    assert(!usb_transfer_response_queue(&response, payload, sizeof(payload)));
}

int main(void) {
    test_encodes_single_response();
    test_encodes_fragmented_response();
    test_retains_prepared_report_until_commit();
    return 0;
}
