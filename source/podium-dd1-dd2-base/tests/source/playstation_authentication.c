#include "usb/playstation_authentication.h"

#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

enum {
    CHUNK_SIZE = 0x38,
    DATA_OFFSET = 4,
};

static void test_initializes_and_builds_format_report(void) {
    UsbPlaystationAuthentication authentication;
    uint8_t report[USB_PLAYSTATION_AUTHENTICATION_FORMAT_REPORT_SIZE];

    usb_playstation_authentication_init(&authentication);
    usb_playstation_authentication_format_report(&authentication, report);

    assert(report[0] == 0xf3);
    assert(report[1] == 0);
    assert(report[2] == CHUNK_SIZE);
    assert(report[3] == CHUNK_SIZE);
    assert(memcmp(report + 4, (uint8_t[4]){0}, 4) == 0);
    assert(authentication.receive_chunk_size == CHUNK_SIZE);
    assert(authentication.transmit_chunk_size == CHUNK_SIZE);
    assert(!authentication.checksum_enabled);
}

static void send_request(UsbPlaystationAuthentication *authentication, uint8_t sequence) {
    uint8_t report[USB_PLAYSTATION_AUTHENTICATION_REPORT_SIZE] = {0};
    report[0] = 0xf0;
    report[1] = sequence;
    for (uint8_t fragment = 0; fragment <= 4; fragment++) {
        report[2] = fragment;
        for (uint8_t index = 0; index < CHUNK_SIZE; index++) {
            report[DATA_OFFSET + index] = (uint8_t)(fragment * CHUNK_SIZE + index);
        }
        assert(usb_playstation_authentication_receive(authentication, report));
    }
}

static void test_assembles_request_and_exposes_pending_status(void) {
    UsbPlaystationAuthentication authentication;
    uint8_t request[USB_PLAYSTATION_AUTHENTICATION_REQUEST_SIZE];
    uint8_t status[USB_PLAYSTATION_AUTHENTICATION_STATUS_REPORT_SIZE];
    usb_playstation_authentication_init(&authentication);

    send_request(&authentication, 0x5a);

    usb_playstation_authentication_status_report(&authentication, status);
    assert(status[0] == 0xf2);
    assert(status[1] == 0x5a);
    assert(status[2] == USB_PLAYSTATION_AUTHENTICATION_PENDING);
    assert(memcmp(status + 3, (uint8_t[13]){0}, 13) == 0);
    assert(usb_playstation_authentication_take_request(&authentication, request));
    assert(!usb_playstation_authentication_take_request(&authentication, request));
    for (uint16_t index = 0; index < sizeof(request); index++) {
        assert(request[index] == (uint8_t)index);
    }
}

static void test_rejects_invalid_upload_reports(void) {
    UsbPlaystationAuthentication authentication;
    uint8_t report[USB_PLAYSTATION_AUTHENTICATION_REPORT_SIZE] = {0};
    usb_playstation_authentication_init(&authentication);

    assert(!usb_playstation_authentication_receive(&authentication, report));
    report[0] = 0xf0;
    report[2] = 5;
    assert(!usb_playstation_authentication_receive(&authentication, report));
}

static void test_applies_optional_report_checksums(void) {
    UsbPlaystationAuthentication authentication;
    uint8_t upload[USB_PLAYSTATION_AUTHENTICATION_REPORT_SIZE] = {0};
    uint8_t status[USB_PLAYSTATION_AUTHENTICATION_STATUS_REPORT_SIZE];
    uint8_t response[USB_PLAYSTATION_AUTHENTICATION_RESPONSE_SIZE] = {0};
    uint8_t download[USB_PLAYSTATION_AUTHENTICATION_REPORT_SIZE];
    usb_playstation_authentication_init(&authentication);
    authentication.checksum_enabled = true;
    authentication.sequence = 0x5a;
    authentication.status = USB_PLAYSTATION_AUTHENTICATION_PENDING;

    usb_playstation_authentication_status_report(&authentication, status);
    assert(memcmp(status + 12, (uint8_t[]){0x3a, 0x55, 0x14, 0xe4}, 4) == 0);

    upload[0] = 0xf0;
    upload[1] = 0x5a;
    memcpy(upload + 60, (uint8_t[]){0x1c, 0x46, 0x15, 0xc1}, 4);
    assert(usb_playstation_authentication_receive(&authentication, upload));
    upload[60] ^= 1;
    assert(!usb_playstation_authentication_receive(&authentication, upload));
    assert(authentication.status == USB_PLAYSTATION_AUTHENTICATION_CHECKSUM_ERROR);

    assert(usb_playstation_authentication_publish_response(&authentication, response,
                                                           sizeof(response)));
    assert(usb_playstation_authentication_response_report(&authentication, download));
    assert(memcmp(download + 60, (uint8_t[]){0x55, 0x9a, 0x49, 0xb4}, 4) == 0);
}

static void test_streams_complete_response(void) {
    UsbPlaystationAuthentication authentication;
    uint8_t response[USB_PLAYSTATION_AUTHENTICATION_RESPONSE_SIZE];
    uint8_t report[USB_PLAYSTATION_AUTHENTICATION_REPORT_SIZE];
    usb_playstation_authentication_init(&authentication);
    authentication.sequence = 0xa4;
    for (uint16_t index = 0; index < sizeof(response); index++) {
        response[index] = (uint8_t)(index ^ 0x96u);
    }

    assert(usb_playstation_authentication_publish_response(&authentication, response,
                                                           sizeof(response)));
    assert(usb_playstation_authentication_response_active(&authentication));
    for (uint8_t fragment = 0; fragment <= 18; fragment++) {
        assert(usb_playstation_authentication_response_report(&authentication, report));
        assert(report[0] == 0xf1);
        assert(report[1] == 0xa4);
        assert(report[2] == fragment);
        uint16_t offset = (uint16_t)fragment * CHUNK_SIZE;
        uint8_t count = fragment == 18 ? 0x20 : CHUNK_SIZE;
        assert(memcmp(report + DATA_OFFSET, response + offset, count) == 0);
        if (fragment == 18) {
            assert(memcmp(report + DATA_OFFSET + count,
                          (uint8_t[USB_PLAYSTATION_AUTHENTICATION_REPORT_SIZE]){0},
                          USB_PLAYSTATION_AUTHENTICATION_REPORT_SIZE - DATA_OFFSET - count) == 0);
        }
    }
    assert(authentication.status == USB_PLAYSTATION_AUTHENTICATION_RESPONSE_ACTIVE);
    assert(!usb_playstation_authentication_response_active(&authentication));
    assert(!usb_playstation_authentication_response_report(&authentication, report));
}

static void test_rejects_wrong_response_size_and_reports_failure(void) {
    UsbPlaystationAuthentication authentication;
    uint8_t response[USB_PLAYSTATION_AUTHENTICATION_RESPONSE_SIZE] = {0};
    uint8_t status[USB_PLAYSTATION_AUTHENTICATION_STATUS_REPORT_SIZE];
    usb_playstation_authentication_init(&authentication);

    assert(!usb_playstation_authentication_publish_response(&authentication, response,
                                                            sizeof(response) - 1));
    usb_playstation_authentication_fail(&authentication);
    usb_playstation_authentication_status_report(&authentication, status);
    assert(status[2] == USB_PLAYSTATION_AUTHENTICATION_RESPONSE_ERROR);
}

int main(void) {
    test_initializes_and_builds_format_report();
    test_assembles_request_and_exposes_pending_status();
    test_rejects_invalid_upload_reports();
    test_applies_optional_report_checksums();
    test_streams_complete_response();
    test_rejects_wrong_response_size_and_reports_failure();
    return 0;
}
