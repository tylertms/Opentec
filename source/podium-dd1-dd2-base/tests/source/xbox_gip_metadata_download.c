#include "usb/xbox_gip_metadata_download.h"

#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

typedef struct {
    uint8_t transfer_type;
    uint8_t field_three;
    uint8_t offset_low;
    uint8_t offset_high;
    uint8_t packet_length;
    uint8_t payload_length;
    bool acknowledgement_due;
} PacketExpectation;

static void
build_acknowledgement(uint8_t acknowledgement[USB_XBOX_GIP_METADATA_ACKNOWLEDGEMENT_SIZE],
                      uint16_t transferred) {
    memset(acknowledgement, 0, USB_XBOX_GIP_METADATA_ACKNOWLEDGEMENT_SIZE);
    acknowledgement[0] = 1;
    acknowledgement[5] = USB_XBOX_GIP_METADATA_REPORT_ID;
    acknowledgement[7] = (uint8_t)transferred;
    acknowledgement[8] = (uint8_t)(transferred >> 8);
    acknowledgement[11] = (uint8_t)(USB_XBOX_GIP_METADATA_SIZE - transferred);
    acknowledgement[12] = (uint8_t)((USB_XBOX_GIP_METADATA_SIZE - transferred) >> 8);
}

static void test_emits_complete_metadata_exchange(void) {
    static const PacketExpectation expected[] = {
        {0xf0, 0x3a, 0xc1, 0x03, 64, 58, true},  {0xa0, 0xba, 0x00, 0x3a, 64, 58, false},
        {0xa0, 0xba, 0x00, 0x74, 64, 58, false}, {0xa0, 0x3a, 0xae, 0x01, 64, 58, false},
        {0xa0, 0x3a, 0xe8, 0x01, 64, 58, false}, {0xb0, 0x3a, 0xa2, 0x02, 64, 58, true},
        {0xa0, 0x3a, 0xdc, 0x02, 64, 58, false}, {0xb0, 0x2b, 0x96, 0x03, 49, 43, true},
        {0xa0, 0x00, 0xc1, 0x03, 6, 0, false},
    };
    uint8_t metadata[USB_XBOX_GIP_METADATA_SIZE];
    uint8_t packet[USB_XBOX_GIP_METADATA_PACKET_SIZE];
    uint8_t acknowledgement[USB_XBOX_GIP_METADATA_ACKNOWLEDGEMENT_SIZE];
    UsbXboxGipMetadataDownload download;
    uint16_t metadata_offset = 0;

    usb_xbox_gip_metadata_encode(metadata);
    usb_xbox_gip_metadata_download_init(&download, 0x2a);

    for (size_t index = 0; index < sizeof(expected) / sizeof(expected[0]); index++) {
        uint8_t packet_length = usb_xbox_gip_metadata_download_next(&download, metadata, packet);
        assert(packet_length != 0);
        assert(packet[0] == USB_XBOX_GIP_METADATA_REPORT_ID);
        assert(packet[1] == expected[index].transfer_type);
        assert(packet[2] == 0x2a);
        assert(packet[3] == expected[index].field_three);
        assert(packet[4] == expected[index].offset_low);
        assert(packet[5] == expected[index].offset_high);
        assert(packet_length == expected[index].packet_length);
        assert(memcmp(&packet[6], &metadata[metadata_offset], expected[index].payload_length) == 0);
        metadata_offset += expected[index].payload_length;

        if (expected[index].acknowledgement_due) {
            assert(usb_xbox_gip_metadata_download_next(&download, metadata, packet) == 0);
            build_acknowledgement(acknowledgement, metadata_offset);
            assert(usb_xbox_gip_metadata_download_acknowledge(&download, acknowledgement));
        }
    }

    assert(metadata_offset == USB_XBOX_GIP_METADATA_SIZE);
    assert(download.complete);
    assert(usb_xbox_gip_metadata_download_next(&download, metadata, packet) == 0);
}

static void test_rejects_mismatched_acknowledgement(void) {
    uint8_t metadata[USB_XBOX_GIP_METADATA_SIZE];
    uint8_t packet[USB_XBOX_GIP_METADATA_PACKET_SIZE];
    uint8_t acknowledgement[USB_XBOX_GIP_METADATA_ACKNOWLEDGEMENT_SIZE];
    uint8_t packet_length;
    UsbXboxGipMetadataDownload download;

    usb_xbox_gip_metadata_encode(metadata);
    usb_xbox_gip_metadata_download_init(&download, 1);
    packet_length = usb_xbox_gip_metadata_download_next(&download, metadata, packet);
    assert(packet_length != 0);

    build_acknowledgement(acknowledgement, 58);
    acknowledgement[5] = 6;
    assert(!usb_xbox_gip_metadata_download_acknowledge(&download, acknowledgement));
    assert(download.awaiting_acknowledgement);

    acknowledgement[5] = USB_XBOX_GIP_METADATA_REPORT_ID;
    acknowledgement[11]++;
    assert(!usb_xbox_gip_metadata_download_acknowledge(&download, acknowledgement));
    assert(download.awaiting_acknowledgement);
}

int main(void) {
    test_emits_complete_metadata_exchange();
    test_rejects_mismatched_acknowledgement();
    return 0;
}
