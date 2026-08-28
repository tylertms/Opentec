#include "usb/xbox_gip_metadata.h"

#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

static uint32_t metadata_hash(const uint8_t *data, size_t length) {
    uint32_t hash = 2166136261u;
    for (size_t index = 0; index < length; index++) {
        hash = (hash ^ data[index]) * 16777619u;
    }
    return hash;
}

int main(void) {
    uint8_t metadata[USB_XBOX_GIP_METADATA_SIZE];
    usb_xbox_gip_metadata_encode(metadata);
    assert(metadata_hash(metadata, sizeof(metadata)) == 0x29072c11u);
    assert(metadata[58] == 26 && metadata[59] == 0);
    assert(memcmp(&metadata[60], "Microsoft.Xbox.Input.Wheel", 26) == 0);
    assert(metadata[86] == 24 && metadata[87] == 0);
    assert(memcmp(&metadata[88], "Windows.Xbox.Input.Wheel", 24) == 0);
    assert(metadata[112] == 39 && metadata[113] == 0);
    assert(memcmp(&metadata[114], "Windows.Xbox.Input.NavigationController", 39) == 0);

    static const uint8_t message_types[] = {0x20, 0x21, 0x25, 0x11, 0x0a,
                                            0x0b, 0x0c, 0x0d, 0x0e, 0x0f};
    for (size_t index = 0; index < sizeof(message_types); index++) {
        size_t offset = 219 + index * 23;
        assert(metadata[offset] == 0x17 && metadata[offset + 2] == message_types[index]);
        assert(metadata[offset + 5] == 1);
        assert(metadata[offset + 7] == (index < 4 ? 0x10 : 0x08));
    }
    return 0;
}
