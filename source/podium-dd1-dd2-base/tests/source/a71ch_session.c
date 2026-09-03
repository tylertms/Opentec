#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "platform/aux_bus.h"
#include "secure_element/a71ch.h"
#include "secure_element/session.h"

static PlatformAuxBusStatus bus_status;
static A71chCommand started_command;
static uint8_t *started_response;
static uint8_t start_count;
static uint8_t clear_count;
static uint8_t init_count;
static bool bus_accepts;

bool a71ch_bus_start(A71chCommand command, uint8_t *response) {
    started_command = command;
    started_response = response;
    ++start_count;
    return bus_accepts;
}

PlatformAuxBusStatus platform_aux_bus_status(void) { return bus_status; }

void platform_aux_bus_init(void) {
    bus_status = PLATFORM_AUX_BUS_IDLE;
    ++init_count;
}

void platform_aux_bus_clear(void) {
    bus_status = PLATFORM_AUX_BUS_IDLE;
    ++clear_count;
}

static void reset_bus(void) {
    bus_status = PLATFORM_AUX_BUS_IDLE;
    started_command = 0;
    started_response = 0;
    start_count = 0;
    clear_count = 0;
    init_count = 0;
    bus_accepts = true;
}

static void finish_transfer(A71chSessionService *service, PlatformAuxBusStatus result,
                            uint32_t now_ms) {
    bus_status = result;
    a71ch_session_service_run(service, now_ms);
}

static void test_requires_explicit_start_and_idle_bus(void) {
    A71chSessionService service;

    reset_bus();
    a71ch_session_service_init(&service);
    a71ch_session_service_run(&service, 0);
    assert(start_count == 0);
    assert(a71ch_session_service_status(&service) == A71CH_SESSION_SERVICE_IDLE);

    a71ch_session_service_start(&service);
    bus_status = PLATFORM_AUX_BUS_BUSY;
    a71ch_session_service_run(&service, 0);
    assert(start_count == 0);
    bus_status = PLATFORM_AUX_BUS_IDLE;
    a71ch_session_service_run(&service, 0);
    assert(start_count == 1);
    assert(started_command == A71CH_WAKE_UP);
    assert(started_response == 0);
}

static void test_retries_failed_and_rejected_transactions(void) {
    A71chSessionService service;

    reset_bus();
    a71ch_session_service_init(&service);
    a71ch_session_service_start(&service);
    a71ch_session_service_run(&service, 0);
    finish_transfer(&service, PLATFORM_AUX_BUS_FAILED, 1);
    assert(clear_count == 1);
    a71ch_session_service_run(&service, 2);
    assert(start_count == 2);
    assert(started_command == A71CH_WAKE_UP);

    finish_transfer(&service, PLATFORM_AUX_BUS_SUCCEEDED, 3);
    bus_accepts = false;
    a71ch_session_service_run(&service, 4);
    assert(started_command == A71CH_SOFT_RESET);
    assert(start_count == 3);
    a71ch_session_service_run(&service, 5);
    assert(start_count == 4);
    assert(started_command == A71CH_SOFT_RESET);
}

static void test_reinitializes_the_bus_before_restarting_a_session(void) {
    A71chSessionService service;

    reset_bus();
    a71ch_session_service_init(&service);
    a71ch_session_service_start(&service);
    a71ch_session_service_run(&service, 0);
    assert(start_count == 1);

    bus_status = PLATFORM_AUX_BUS_BUSY;
    uint8_t previous_init_count = init_count;
    a71ch_session_service_init(&service);
    assert(init_count == (uint8_t)(previous_init_count + 1));
    a71ch_session_service_start(&service);
    a71ch_session_service_run(&service, 0);
    assert(start_count == 2);
    assert(started_command == A71CH_WAKE_UP);
}

static void test_honors_startup_status_retry_delay(void) {
    A71chSessionService service;

    reset_bus();
    a71ch_session_service_init(&service);
    a71ch_session_service_start(&service);
    a71ch_session_service_run(&service, 100);
    finish_transfer(&service, PLATFORM_AUX_BUS_SUCCEEDED, 100);
    a71ch_session_service_run(&service, 100);
    started_response[0] = 1;
    started_response[1] = 7;
    finish_transfer(&service, PLATFORM_AUX_BUS_SUCCEEDED, 100);

    a71ch_session_service_run(&service, 104);
    assert(start_count == 2);
    a71ch_session_service_run(&service, 105);
    assert(start_count == 3);
    assert(started_command == A71CH_SOFT_RESET);
}

static void test_completes_startup_from_bus_responses(void) {
    static const uint8_t signature[] = {
        0xb8, 0x04, 0x11, 0x01, 0x05, 0x04, 0xb9, 0x02, 0x01, 0x01, 0xba, 0x01, 0x01, 0xbb, 0x0c,
        0x41, 0x37, 0x31, 0x30, 0x35, 0x43, 0x43, 0x32, 0x34, 0x32, 0x52, 0x31, 0xbc, 0x00,
    };
    A71chSessionService service;

    reset_bus();
    a71ch_session_service_init(&service);
    a71ch_session_service_start(&service);

    a71ch_session_service_run(&service, 0);
    finish_transfer(&service, PLATFORM_AUX_BUS_SUCCEEDED, 0);

    a71ch_session_service_run(&service, 0);
    assert(started_command == A71CH_SOFT_RESET);
    started_response[0] = 1;
    started_response[1] = 0;
    finish_transfer(&service, PLATFORM_AUX_BUS_SUCCEEDED, 0);

    a71ch_session_service_run(&service, 5);
    assert(started_command == A71CH_READ_ANSWER_TO_RESET);
    started_response[0] = sizeof(signature) + 1;
    started_response[1] = 0;
    memcpy(&started_response[2], signature, sizeof(signature));
    finish_transfer(&service, PLATFORM_AUX_BUS_SUCCEEDED, 0);

    a71ch_session_service_run(&service, 0);
    assert(started_command == A71CH_PARAMETER_EXCHANGE);
    started_response[0] = 1;
    started_response[1] = 0xcc;
    finish_transfer(&service, PLATFORM_AUX_BUS_SUCCEEDED, 0);

    a71ch_session_service_run(&service, 0);
    assert(started_command == A71CH_READ_STATUS);
    started_response[0] = 1;
    started_response[1] = 7;
    finish_transfer(&service, PLATFORM_AUX_BUS_SUCCEEDED, 0);

    assert(a71ch_session_service_status(&service) == A71CH_SESSION_SERVICE_COMPLETE);
    a71ch_session_service_run(&service, 1);
    assert(start_count == 5);
    assert(clear_count == 5);
}

int main(void) {
    test_requires_explicit_start_and_idle_bus();
    test_retries_failed_and_rejected_transactions();
    test_reinitializes_the_bus_before_restarting_a_session();
    test_honors_startup_status_retry_delay();
    test_completes_startup_from_bus_responses();
    return 0;
}
