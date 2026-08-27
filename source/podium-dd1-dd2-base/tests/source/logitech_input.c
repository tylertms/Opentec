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
        0x8d, 0xa4, 0x00, 0x37, 0x7f, 0x11, 0x22, 0x33,
    };
    uint8_t report[LOGITECH_DRIVING_FORCE_PRO_REPORT_SIZE];

    assert(logitech_driving_force_pro_encode(report, &state));
    assert(memcmp(report, expected, sizeof(expected)) == 0);
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
    test_g27();
    test_validation();
    return 0;
}
