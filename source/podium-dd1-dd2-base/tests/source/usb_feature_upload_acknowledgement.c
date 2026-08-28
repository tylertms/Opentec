#include <assert.h>
#include <stdint.h>
#include <string.h>

#include "usb/feature_upload_acknowledgement.h"

static void test_encodes_segmented_acknowledgement(void) {
    static const uint8_t request[USB_FEATURE_UPLOAD_PACKET_SIZE] = {6, 0xb0};
    static const uint8_t expected[] = {1, 0x20, 0x2a, 9, 0, 6, 0xb0, 0x83, 0, 0, 0, 3, 0};
    uint8_t acknowledgement[USB_FEATURE_UPLOAD_ACKNOWLEDGEMENT_SIZE];

    assert(usb_feature_upload_acknowledgement_segmented_encode(0x2a, request, 131, 134,
                                                               acknowledgement));
    assert(memcmp(acknowledgement, expected, sizeof(expected)) == 0);
}

static void test_encodes_compact_acknowledgement(void) {
    static const uint8_t request[USB_FEATURE_UPLOAD_PACKET_SIZE] = {6, 0x30, 0, 12};
    static const uint8_t expected[] = {1, 0x20, 7, 9, 0, 6, 0x30, 12, 0, 0, 0, 12, 0};
    uint8_t acknowledgement[USB_FEATURE_UPLOAD_ACKNOWLEDGEMENT_SIZE];

    assert(usb_feature_upload_acknowledgement_compact_encode(7, request, acknowledgement));
    assert(memcmp(acknowledgement, expected, sizeof(expected)) == 0);
}

static void test_rejects_invalid_progress(void) {
    uint8_t request[USB_FEATURE_UPLOAD_PACKET_SIZE] = {0};
    uint8_t acknowledgement[USB_FEATURE_UPLOAD_ACKNOWLEDGEMENT_SIZE];

    assert(!usb_feature_upload_acknowledgement_segmented_encode(0, request, 2, 1, acknowledgement));
    assert(!usb_feature_upload_acknowledgement_segmented_encode(0, 0, 0, 0, acknowledgement));
    assert(!usb_feature_upload_acknowledgement_compact_encode(0, request, 0));
}

int main(void) {
    test_encodes_segmented_acknowledgement();
    test_encodes_compact_acknowledgement();
    test_rejects_invalid_progress();
    return 0;
}
