#include <assert.h>
#include <stdbool.h>
#include <stdint.h>

#include "system/base_mode_controller.h"

static void test_initializes_and_starts_once(void) {
    BaseModeController controller;
    base_mode_controller_init(&controller);

    assert(base_mode_controller_phase(&controller) == BASE_MODE_CONTROLLER_RESET);
    assert(!base_mode_controller_memory_active(&controller));
    assert(!base_mode_controller_start(NULL, 0));
    assert(base_mode_controller_start(&controller, 1000));
    assert(!base_mode_controller_start(&controller, 1001));
    assert(base_mode_controller_phase(&controller) == BASE_MODE_CONTROLLER_MEMORY_STARTUP);
    assert(base_mode_controller_memory_active(&controller));
    assert(base_mode_controller_step(NULL, 0, BASE_MODE_CONTROLLER_MEMORY_RUNNING, true, true) ==
           BASE_MODE_CONTROLLER_ACTION_NONE);
}

static void test_memory_timeout_has_second_bounded_window(void) {
    BaseModeController controller;
    base_mode_controller_init(&controller);
    assert(base_mode_controller_start(&controller, 1000));

    assert(base_mode_controller_step(&controller, 2999, BASE_MODE_CONTROLLER_MEMORY_RUNNING, true,
                                     true) == BASE_MODE_CONTROLLER_ACTION_NONE);
    assert(base_mode_controller_step(&controller, 3000, BASE_MODE_CONTROLLER_MEMORY_RUNNING, true,
                                     true) ==
           (BASE_MODE_CONTROLLER_ACTION_DISPLAY_ERROR | BASE_MODE_CONTROLLER_ACTION_RESET_MEMORY));
    assert(base_mode_controller_phase(&controller) == BASE_MODE_CONTROLLER_MEMORY_TIMEOUT);
    assert(base_mode_controller_memory_active(&controller));
    assert(base_mode_controller_step(&controller, 5000, BASE_MODE_CONTROLLER_MEMORY_RUNNING, true,
                                     true) == BASE_MODE_CONTROLLER_ACTION_NONE);
    assert(base_mode_controller_step(&controller, 5001, BASE_MODE_CONTROLLER_MEMORY_RUNNING, true,
                                     true) == (BASE_MODE_CONTROLLER_ACTION_FALLBACK_NATIVE |
                                               BASE_MODE_CONTROLLER_ACTION_RESET_MEMORY));
    assert(base_mode_controller_phase(&controller) == BASE_MODE_CONTROLLER_RESET);
    assert(!base_mode_controller_memory_active(&controller));
}

static void test_success_waits_for_usb_and_status_deadlines(void) {
    BaseModeController controller;
    base_mode_controller_init(&controller);
    assert(base_mode_controller_start(&controller, 1000));

    assert(base_mode_controller_step(&controller, 1050, BASE_MODE_CONTROLLER_MEMORY_COMPLETE, true,
                                     true) == BASE_MODE_CONTROLLER_ACTION_NONE);
    assert(base_mode_controller_phase(&controller) == BASE_MODE_CONTROLLER_STATUS_USB_DELAY);
    assert(base_mode_controller_step(&controller, 1100, BASE_MODE_CONTROLLER_MEMORY_RUNNING, true,
                                     true) == BASE_MODE_CONTROLLER_ACTION_NONE);
    assert(base_mode_controller_step(&controller, 1101, BASE_MODE_CONTROLLER_MEMORY_RUNNING, true,
                                     true) == BASE_MODE_CONTROLLER_ACTION_ENABLE_USB);
    assert(base_mode_controller_phase(&controller) == BASE_MODE_CONTROLLER_STATUS_WAIT);
    assert(base_mode_controller_step(&controller, 3101, BASE_MODE_CONTROLLER_MEMORY_RUNNING, true,
                                     true) == BASE_MODE_CONTROLLER_ACTION_NONE);
    assert(base_mode_controller_step(&controller, 3102, BASE_MODE_CONTROLLER_MEMORY_RUNNING, true,
                                     true) == BASE_MODE_CONTROLLER_ACTION_NONE);
    assert(base_mode_controller_phase(&controller) == BASE_MODE_CONTROLLER_STATUS_ACTIVE);
    assert(base_mode_controller_step(&controller, 3102, BASE_MODE_CONTROLLER_MEMORY_RUNNING, true,
                                     true) == BASE_MODE_CONTROLLER_ACTION_NONE);
    assert(base_mode_controller_step(&controller, 3103, BASE_MODE_CONTROLLER_MEMORY_RUNNING, false,
                                     true) == BASE_MODE_CONTROLLER_ACTION_FALLBACK_NATIVE);
    assert(base_mode_controller_phase(&controller) == BASE_MODE_CONTROLLER_RESET);

    base_mode_controller_init(&controller);
    assert(base_mode_controller_start(&controller, 1000));
    assert(base_mode_controller_step(&controller, 1200, BASE_MODE_CONTROLLER_MEMORY_COMPLETE, true,
                                     true) == BASE_MODE_CONTROLLER_ACTION_ENABLE_USB);
    assert(base_mode_controller_phase(&controller) == BASE_MODE_CONTROLLER_STATUS_WAIT);
}

static void test_invalid_memory_context_falls_back_immediately(void) {
    BaseModeController controller;
    base_mode_controller_init(&controller);
    assert(base_mode_controller_start(&controller, UINT32_MAX - 100u));
    assert(base_mode_controller_step(&controller, UINT32_MAX - 99u,
                                     BASE_MODE_CONTROLLER_MEMORY_RUNNING, true,
                                     false) == BASE_MODE_CONTROLLER_ACTION_FALLBACK_NATIVE);
    assert(base_mode_controller_phase(&controller) == BASE_MODE_CONTROLLER_RESET);

    base_mode_controller_init(&controller);
    assert(base_mode_controller_start(&controller, UINT32_MAX - 100u));
    assert(base_mode_controller_step(&controller, 1898u, BASE_MODE_CONTROLLER_MEMORY_RUNNING, true,
                                     true) == BASE_MODE_CONTROLLER_ACTION_NONE);
    assert(base_mode_controller_step(&controller, 1899u, BASE_MODE_CONTROLLER_MEMORY_COMPLETE, true,
                                     true) == BASE_MODE_CONTROLLER_ACTION_ENABLE_USB);
    assert(base_mode_controller_phase(&controller) == BASE_MODE_CONTROLLER_STATUS_WAIT);
}

int main(void) {
    test_initializes_and_starts_once();
    test_memory_timeout_has_second_bounded_window();
    test_success_waits_for_usb_and_status_deadlines();
    test_invalid_memory_context_falls_back_immediately();
    assert(base_mode_controller_phase(NULL) == BASE_MODE_CONTROLLER_RESET);
    assert(!base_mode_controller_memory_active(NULL));
    return 0;
}
