#include <assert.h>
#include <stddef.h>
#include <stdint.h>

static int8_t signed_status(uint8_t value) { return (int8_t)value; }

#include "wheel/accessory.h"

static void test_initializes_disconnected_state(void) {
    WheelAccessory accessory = {
        .version = UINT32_MAX,
        .initial_status = -1,
        .model = UINT8_MAX,
        .kind = WHEEL_ACCESSORY_EXTENDED,
    };

    wheel_accessory_init(&accessory);

    assert(accessory.kind == WHEEL_ACCESSORY_DISCONNECTED);
    assert(accessory.version == 0);
    assert(accessory.initial_status == 0);
    assert(accessory.model == 0);
    wheel_accessory_init(NULL);
}

static void test_detects_legacy_standard_and_extended_protocols(void) {
    WheelAccessory accessory;
    wheel_accessory_init(&accessory);

    assert(wheel_accessory_apply_probe(&accessory, 5, UINT32_C(0x12345678)));
    assert(accessory.kind == WHEEL_ACCESSORY_LEGACY);
    assert(wheel_accessory_transfer_code(&accessory) == 0x38);
    assert(wheel_accessory_mode_code(&accessory) == 0);

    assert(wheel_accessory_apply_probe(&accessory, signed_status(0x94), 1));
    assert(accessory.kind == WHEEL_ACCESSORY_STANDARD);
    assert(accessory.model == 5);
    assert(wheel_accessory_mode_code(&accessory) == 1);
    assert(wheel_accessory_mode_flags(&accessory) == 0x8a);

    assert(wheel_accessory_apply_probe(&accessory, signed_status(0x95), 2));
    assert(accessory.kind == WHEEL_ACCESSORY_EXTENDED);
    assert(accessory.model == 5);
    assert(wheel_accessory_mode_code(&accessory) == 2);
    assert(wheel_accessory_mode_flags(&accessory) == 0x8b);
    assert(wheel_accessory_position_modulus(&accessory) == UINT32_C(0x5c7f));

    assert(wheel_accessory_apply_probe(&accessory, signed_status(0x96), 3));
    assert(accessory.kind == WHEEL_ACCESSORY_EXTENDED);
    assert(wheel_accessory_mode_code(&accessory) == 3);
}

static void test_rejects_reserved_protocol_without_replacing_identity(void) {
    WheelAccessory accessory;
    wheel_accessory_init(&accessory);
    assert(wheel_accessory_apply_probe(&accessory, signed_status(0x95), 1));

    assert(!wheel_accessory_apply_probe(&accessory, signed_status(0x97), UINT32_C(0xabcdef12)));
    assert(accessory.kind == WHEEL_ACCESSORY_EXTENDED);
    assert(accessory.model == 5);
    assert(accessory.initial_status == signed_status(0x97));
    assert(accessory.version == UINT32_C(0xabcdef12));
    assert(wheel_accessory_mode_code(&accessory) == 4);
    assert(wheel_accessory_mode_flags(&accessory) == 0x8b);
    assert(wheel_accessory_position_modulus(&accessory) == UINT32_C(0x5c7f));

    assert(wheel_accessory_apply_probe(&accessory, signed_status(0x91), 0));
    assert(accessory.model == 4);
    assert(wheel_accessory_position_modulus(&accessory) == UINT32_C(0x5c7f));
    assert(wheel_accessory_apply_probe(&accessory, signed_status(0x89), 0));
    assert(accessory.model == 2);
    assert(wheel_accessory_position_modulus(&accessory) == UINT32_C(0x5d2b));
}

static void test_preserves_model_when_legacy_status_follows_extended(void) {
    WheelAccessory accessory;
    wheel_accessory_init(&accessory);
    assert(wheel_accessory_apply_probe(&accessory, signed_status(0x95), 0));

    assert(wheel_accessory_apply_probe(&accessory, 2, 0));
    assert(accessory.kind == WHEEL_ACCESSORY_LEGACY);
    assert(accessory.model == 5);
    assert(wheel_accessory_mode_code(&accessory) == 0);
    assert(wheel_accessory_mode_flags(&accessory) == 0x0b);
}

static void test_handles_unavailable_state(void) {
    assert(!wheel_accessory_apply_probe(NULL, 0, 0));
    assert(wheel_accessory_transfer_code(NULL) == 0);
    assert(wheel_accessory_mode_code(NULL) == 0);
    assert(wheel_accessory_mode_flags(NULL) == 0);
}

int main(void) {
    test_initializes_disconnected_state();
    test_detects_legacy_standard_and_extended_protocols();
    test_rejects_reserved_protocol_without_replacing_identity();
    test_preserves_model_when_legacy_status_follows_extended();
    test_handles_unavailable_state();
    return 0;
}
