#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "wheel/updater_direct_service.h"

static uint8_t transmitted[WHEEL_UPDATER_BRIDGE_MAX_REQUEST_SIZE];
static uint8_t transmitted_length;
static bool write_allowed;
static const uint8_t *received;
static uint8_t received_length;

void platform_serial_link_enter_direct_mode(void) {}

bool platform_serial_link_direct_write(const uint8_t *data, uint8_t length) {
    if (!write_allowed) {
        return false;
    }
    memcpy(transmitted, data, length);
    transmitted_length = length;
    return true;
}

bool platform_serial_link_direct_read(uint8_t *data, uint8_t length) {
    if (received_length < length) {
        return false;
    }
    memcpy(data, received, length);
    received += length;
    received_length -= length;
    return true;
}

void platform_serial_link_direct_clear(void) {
    received = NULL;
    received_length = 0;
}

static void reset_transport(void) {
    transmitted_length = 0;
    write_allowed = true;
    received = NULL;
    received_length = 0;
}

static void test_rejects_invalid_service_and_request(void) {
    WheelUpdaterDirectService service;
    const uint8_t request[] = {0x5a, 0xb0};
    wheel_updater_direct_service_init(&service);

    assert(!wheel_updater_direct_service_start(NULL, request, sizeof(request)));
    assert(!wheel_updater_direct_service_start(&service, NULL, sizeof(request)));
    assert(!wheel_updater_direct_service_active(NULL));
    wheel_updater_direct_service_init(NULL);
    wheel_updater_direct_service_run(NULL, 0);
}

static void test_retries_busy_write(void) {
    WheelUpdaterDirectService service;
    const uint8_t request[] = {0x5a, 0xb0, 0x33};
    reset_transport();
    wheel_updater_direct_service_init(&service);
    assert(wheel_updater_direct_service_start(&service, request, sizeof(request)));

    write_allowed = false;
    wheel_updater_direct_service_run(&service, 0);
    assert(transmitted_length == 0);
    write_allowed = true;
    wheel_updater_direct_service_run(&service, 0);
    assert(transmitted_length == sizeof(request));
    assert(memcmp(transmitted, request, sizeof(request)) == 0);
}

static void test_completes_write_only_request(void) {
    WheelUpdaterDirectService service;
    const uint8_t request[] = {0x5a, 0xa1};
    reset_transport();
    wheel_updater_direct_service_init(&service);
    assert(wheel_updater_direct_service_start(&service, request, sizeof(request)));

    wheel_updater_direct_service_run(&service, 0);
    assert(wheel_updater_direct_service_active(&service));
    wheel_updater_direct_service_run(&service, 0);
    assert(!wheel_updater_direct_service_active(&service));
}

static void test_exchanges_full_variable_response(void) {
    WheelUpdaterDirectService service;
    const uint8_t request[] = {0x5a, 0xb0};
    uint8_t response[WHEEL_UPDATER_BRIDGE_MAX_RESPONSE_SIZE] = {0x5a, 0xa4, 60, 0, 0x34, 0x12};
    for (uint8_t index = 6; index < sizeof(response); index++) {
        response[index] = index;
    }
    reset_transport();
    wheel_updater_direct_service_init(&service);
    assert(wheel_updater_direct_service_start(&service, request, sizeof(request)));

    wheel_updater_direct_service_run(&service, 100);
    wheel_updater_direct_service_run(&service, 100);
    received = response;
    received_length = sizeof(response);
    for (uint8_t iteration = 0; iteration < 16; iteration++) {
        wheel_updater_direct_service_run(&service, 102);
    }

    const uint8_t *actual = NULL;
    uint8_t actual_length = 0;
    assert(wheel_updater_direct_service_take_response(&service, &actual, &actual_length));
    assert(actual_length == sizeof(response));
    assert(memcmp(actual, response, sizeof(response)) == 0);
    assert(!wheel_updater_direct_service_active(&service));
}

int main(void) {
    test_rejects_invalid_service_and_request();
    test_retries_busy_write();
    test_completes_write_only_request();
    test_exchanges_full_variable_response();
    return 0;
}
