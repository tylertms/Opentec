#include "usb/xbox_gip_service.h"

#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "usb/xbox_gip_metadata.h"
#include "usb/xbox_gip_response.h"

typedef struct {
    UsbXboxGipService service;
    UsbXboxGipServiceIdentity identity;
    uint8_t digest[USB_XBOX_GIP_DIGEST_SIZE];
    uint8_t metadata[USB_XBOX_GIP_METADATA_SIZE];
    uint8_t request[USB_XBOX_GIP_METADATA_PACKET_SIZE];
    uint8_t response[USB_XBOX_GIP_METADATA_PACKET_SIZE];
} Fixture;

static void fixture_init(Fixture *fixture) {
    memset(fixture, 0, sizeof(*fixture));
    usb_xbox_gip_metadata_encode(fixture->metadata);
    fixture->identity = (UsbXboxGipServiceIdentity){
        .variant = BOARD_VARIANT_DD1,
        .wheel_mode = 6,
        .digest = fixture->digest,
        .metadata = fixture->metadata,
    };
    usb_xbox_gip_service_init(&fixture->service);
}

static UsbXboxGipServiceResult poll_service(Fixture *fixture, uint32_t now) {
    UsbXboxGipServiceResult result = usb_xbox_gip_service_poll(
        &fixture->service, &fixture->identity, fixture->request, now, fixture->response);
    memset(fixture->request, 0, sizeof(fixture->request));
    return result;
}

static void acknowledge_metadata(Fixture *fixture) {
    uint16_t transferred = fixture->service.metadata_download.offset;
    fixture->request[0] = 1;
    fixture->request[5] = USB_XBOX_GIP_METADATA_REPORT_ID;
    fixture->request[7] = (uint8_t)transferred;
    fixture->request[8] = (uint8_t)(transferred >> 8);
    fixture->request[11] = (uint8_t)(USB_XBOX_GIP_METADATA_SIZE - transferred);
    fixture->request[12] = (uint8_t)((USB_XBOX_GIP_METADATA_SIZE - transferred) >> 8);
}

static void test_runs_discovery_and_metadata_download(void) {
    Fixture fixture;
    fixture_init(&fixture);

    UsbXboxGipServiceResult result = poll_service(&fixture, 100);
    assert(result.response_length == USB_XBOX_GIP_DIGEST_RESPONSE_SIZE);
    assert(fixture.response[0] == 2 && fixture.response[2] == 1);
    assert(fixture.service.next_sequence == 2);

    fixture.request[0] = 4;
    result = poll_service(&fixture, 101);
    assert(result.response_length == 0);
    assert(fixture.service.metadata_pending);
    assert(fixture.service.session.state == USB_XBOX_GIP_SESSION_METADATA_DOWNLOAD);

    uint16_t reconstructed = 0;
    while (fixture.service.metadata_active) {
        result = poll_service(&fixture, 102);
        if (result.response_length == 0) {
            if (fixture.service.metadata_download.awaiting_acknowledgement) {
                acknowledge_metadata(&fixture);
            }
            continue;
        }

        assert(fixture.response[0] == USB_XBOX_GIP_METADATA_REPORT_ID);
        assert(fixture.response[2] == 2);
        uint8_t payload_length = result.response_length == 64 ? 58 : result.response_length - 6;
        if (fixture.service.metadata_download.complete) {
            payload_length = 0;
        }
        assert(memcmp(&fixture.response[6], &fixture.metadata[reconstructed], payload_length) == 0);
        reconstructed += payload_length;
    }

    assert(reconstructed == USB_XBOX_GIP_METADATA_SIZE);
    assert(fixture.service.session.state == USB_XBOX_GIP_SESSION_READY);
}

static void test_responds_to_session_commands(void) {
    Fixture fixture;
    fixture_init(&fixture);
    fixture.service.session.state = USB_XBOX_GIP_SESSION_READY;
    fixture.service.next_sequence = 2;

    fixture.request[0] = 5;
    fixture.request[4] = 0;
    UsbXboxGipServiceResult result = poll_service(&fixture, 0);
    assert(result.session_actions ==
           (USB_XBOX_GIP_SESSION_ACTION_SEND_READY | USB_XBOX_GIP_SESSION_ACTION_REFRESH_STATE));
    assert(result.response_length == USB_XBOX_GIP_READY_RESPONSE_SIZE);
    assert(fixture.response[2] == 2);
    assert(fixture.service.next_sequence == 3);

    fixture.request[0] = 5;
    fixture.request[1] = 0x77;
    fixture.request[4] = 3;
    result = poll_service(&fixture, 1);
    assert(result.response_length == USB_XBOX_GIP_TRANSFER_STATUS_RESPONSE_SIZE);
    assert(fixture.response[2] == 3);
    assert(fixture.response[5] == 5 && fixture.response[6] == 0x77);
    assert(fixture.service.next_sequence == 3);
}

static void test_classifies_force_feedback_application_packets(void) {
    Fixture fixture;
    fixture_init(&fixture);
    fixture.service.session.state = USB_XBOX_GIP_SESSION_ACTIVE;

    for (uint8_t packet = 0x0b; packet <= 0x0e; packet++) {
        fixture.request[0] = packet;
        UsbXboxGipServiceResult result = poll_service(&fixture, packet);
        assert(result.application_output);
        assert(result.response_length == 0);
    }

    fixture.request[0] = 0x0a;
    assert(!poll_service(&fixture, 0).application_output);
    fixture.request[0] = 0x0f;
    assert(!poll_service(&fixture, 0).application_output);
}

int main(void) {
    test_runs_discovery_and_metadata_download();
    test_responds_to_session_commands();
    test_classifies_force_feedback_application_packets();
    return 0;
}
