#include "usb/logitech_input.h"

#include <assert.h>
#include <stdint.h>
#include <string.h>

static const LogitechInputState state = {
    .steering = 0x9234,
    .buttons = 0x5abcde,
    .hat = 7,
    .axes = {0x11, 0x22, 0x33},
};

static void test_driving_force_ex(void) {
    const uint8_t expected[LOGITECH_DRIVING_FORCE_EX_REPORT_SIZE] = {
        0x48, 0x7a, 0x33, 0x11, 0x07, 0x22, 0x33,
    };
    uint8_t report[LOGITECH_DRIVING_FORCE_EX_REPORT_SIZE];

    assert(logitech_driving_force_ex_encode(report, &state));
    assert(memcmp(report, expected, sizeof(expected)) == 0);
}

static void test_driving_force_pro(void) {
    const uint8_t expected[LOGITECH_DRIVING_FORCE_PRO_REPORT_SIZE] = {
        0x8d, 0xa4, 0x00, 0x37, 0x7f, 0x11, 0xa5, 0x5a,
    };
    uint8_t report[LOGITECH_DRIVING_FORCE_PRO_REPORT_SIZE] = {
        0, 0, 0, 0, 0, 0, 0xa5, 0x5a,
    };

    assert(logitech_driving_force_pro_encode(report, &state));
    assert(memcmp(report, expected, sizeof(expected)) == 0);
}

static void test_driving_force_pro_maps_official_button_lanes(void) {
    static const struct {
        uint8_t buttons[3];
        uint8_t report[LOGITECH_DRIVING_FORCE_PRO_REPORT_SIZE];
    } mappings[] = {
        {{0x40, 0x00, 0x00}, {0x00, 0x00, 0x00, 0x01, 0x80, 0x00, 0xff, 0x00}},
        {{0x80, 0x00, 0x00}, {0x00, 0x00, 0x00, 0x02, 0x80, 0x00, 0xff, 0x00}},
        {{0x00, 0x01, 0x00}, {0x00, 0x00, 0x00, 0x04, 0x80, 0x00, 0xff, 0x00}},
        {{0x00, 0x08, 0x00}, {0x00, 0x00, 0x00, 0x08, 0x80, 0x00, 0xff, 0x00}},
        {{0x00, 0x02, 0x00}, {0x00, 0x00, 0x00, 0x10, 0x80, 0x00, 0xff, 0x00}},
        {{0x00, 0x10, 0x00}, {0x00, 0x00, 0x00, 0x20, 0x80, 0x00, 0xff, 0x00}},
        {{0x00, 0x40, 0x00}, {0x00, 0x00, 0x00, 0x40, 0x80, 0x00, 0xff, 0x00}},
        {{0x00, 0x80, 0x00}, {0x00, 0x00, 0x00, 0x80, 0x80, 0x00, 0xff, 0x00}},
        {{0x00, 0x04, 0x00}, {0x00, 0x00, 0x00, 0x00, 0x81, 0x00, 0xff, 0x00}},
        {{0x00, 0x20, 0x00}, {0x00, 0x00, 0x00, 0x00, 0x82, 0x00, 0xff, 0x00}},
    };

    for (size_t index = 0; index < sizeof(mappings) / sizeof(mappings[0]); index++) {
        LogitechInputSource source = {.pedals = {0, UINT16_MAX, 0}};
        LogitechInputState mapped;
        uint8_t report[LOGITECH_DRIVING_FORCE_PRO_REPORT_SIZE] = {
            0, 0, 0, 0, 0, 0, 0xff, 0,
        };
        memcpy(source.buttons, mappings[index].buttons, sizeof(source.buttons));
        logitech_input_map(&mapped, LOGITECH_INPUT_MODEL_DRIVING_FORCE_PRO, &source);
        assert(logitech_driving_force_pro_encode(report, &mapped));
        assert(memcmp(report, mappings[index].report, sizeof(report)) == 0);
    }
}

static void test_driving_force_pro_maps_sequential_transition_bits(void) {
    static const struct {
        uint8_t sequential_buttons;
        uint8_t expected_secondary;
    } mappings[] = {
        {0x02, 0x08},
        {0x01, 0x04},
    };

    for (size_t index = 0; index < sizeof(mappings) / sizeof(mappings[0]); index++) {
        LogitechInputSource source = {
            .sequential_buttons = mappings[index].sequential_buttons,
            .sequential = true,
        };
        LogitechInputState mapped;
        uint8_t report[LOGITECH_DRIVING_FORCE_PRO_REPORT_SIZE] = {
            0, 0, 0, 0, 0, 0, 0xa5, 0x5a,
        };
        logitech_input_map(&mapped, LOGITECH_INPUT_MODEL_DRIVING_FORCE_PRO, &source);
        assert(logitech_driving_force_pro_encode(report, &mapped));
        assert((report[4] & 0x0fu) == mappings[index].expected_secondary);
        assert(report[6] == 0xa5);
        assert(report[7] == 0x5a);
    }
}

static void test_g27(void) {
    const uint8_t expected[LOGITECH_G27_REPORT_SIZE] = {
        0xe7, 0xcd, 0xab, 0x35, 0x92, 0x11, 0x22, 0x33, 0x80, 0x80, 0x03,
    };
    uint8_t report[LOGITECH_G27_REPORT_SIZE];

    assert(logitech_g27_encode(report, &state));
    assert(memcmp(report, expected, sizeof(expected)) == 0);
}

static void test_validation(void) {
    uint8_t ex[LOGITECH_DRIVING_FORCE_EX_REPORT_SIZE];
    uint8_t pro[LOGITECH_DRIVING_FORCE_PRO_REPORT_SIZE];
    uint8_t g27[LOGITECH_G27_REPORT_SIZE];

    assert(!logitech_driving_force_ex_encode(NULL, &state));
    assert(!logitech_driving_force_ex_encode(ex, NULL));
    assert(!logitech_driving_force_pro_encode(NULL, &state));
    assert(!logitech_driving_force_pro_encode(pro, NULL));
    assert(!logitech_g27_encode(NULL, &state));
    assert(!logitech_g27_encode(g27, NULL));
}

int main(void) {
    test_driving_force_ex();
    test_driving_force_pro();
    test_driving_force_pro_maps_official_button_lanes();
    test_driving_force_pro_maps_sequential_transition_bits();
    test_g27();
    test_validation();
    return 0;
}
