#include "system/runtime_bridge.h"

#include <assert.h>
#include <stdbool.h>
#include <stdint.h>

static RuntimeBridgeInput input_at(uint32_t now_ms) {
    return (RuntimeBridgeInput){.now_ms = now_ms};
}

static void test_rejects_invalid_and_overlapping_transitions(void) {
    RuntimeBridge bridge;
    runtime_bridge_init(&bridge);

    assert(runtime_bridge_start(NULL, USB_RUNTIME_MODE_USB_BRIDGE) == 0);
    assert(runtime_bridge_start(&bridge, USB_RUNTIME_MODE_NORMAL) == 0);
    assert(bridge.phase == RUNTIME_BRIDGE_IDLE);
    assert(bridge.mode == USB_RUNTIME_MODE_NORMAL);

    assert(runtime_bridge_start(&bridge, USB_RUNTIME_MODE_AUXILIARY) ==
           RUNTIME_BRIDGE_ACTION_REQUEST_AUXILIARY_HANDSHAKE);
    assert(runtime_bridge_start(&bridge, USB_RUNTIME_MODE_USB_BRIDGE) == 0);
    assert(!runtime_bridge_active(&bridge));
    assert(!runtime_bridge_active(NULL));
    assert(runtime_bridge_step(NULL, NULL) == 0);
    assert(runtime_bridge_step(&bridge, NULL) == 0);
}

static void test_runs_auxiliary_transition(void) {
    RuntimeBridge bridge;
    RuntimeBridgeInput input = input_at(1000);
    runtime_bridge_init(&bridge);

    assert(runtime_bridge_start(&bridge, USB_RUNTIME_MODE_AUXILIARY_RECOVERY) ==
           RUNTIME_BRIDGE_ACTION_REQUEST_AUXILIARY_HANDSHAKE);
    assert(runtime_bridge_step(&bridge, &input) == 0);

    input.auxiliary_handshake_complete = true;
    assert(runtime_bridge_step(&bridge, &input) ==
           (RUNTIME_BRIDGE_ACTION_PREPARE_USB | RUNTIME_BRIDGE_ACTION_ENABLE_TRANSFER_TIMER));
    input.now_ms = 1009;
    assert(runtime_bridge_step(&bridge, &input) == 0);
    input.now_ms = 1010;
    assert(runtime_bridge_step(&bridge, &input) == 0);
    input.now_ms = 1011;
    assert(runtime_bridge_step(&bridge, &input) == RUNTIME_BRIDGE_ACTION_START_TRANSFER);

    input.transfer_status = RUNTIME_BRIDGE_TRANSFER_COMPLETE;
    assert(runtime_bridge_step(&bridge, &input) == 0);
    input.now_ms = 1099;
    assert(runtime_bridge_step(&bridge, &input) == 0);
    input.now_ms = 1100;
    assert(runtime_bridge_step(&bridge, &input) == 0);
    input.now_ms = 1101;
    assert(runtime_bridge_step(&bridge, &input) == RUNTIME_BRIDGE_ACTION_ACTIVATE_UPDATER_USB);
    assert(runtime_bridge_active(&bridge));
    assert(runtime_bridge_step(&bridge, &input) == RUNTIME_BRIDGE_ACTION_SERVICE_UPDATER);
}

static void test_runs_startup_auxiliary_recovery(void) {
    RuntimeBridge bridge;
    RuntimeBridgeInput input = input_at(1000);
    runtime_bridge_init(&bridge);

    assert(runtime_bridge_start_auxiliary_recovery(NULL, input.now_ms) == 0);
    assert(runtime_bridge_start_auxiliary_recovery(&bridge, input.now_ms) ==
           (RUNTIME_BRIDGE_ACTION_PREPARE_USB | RUNTIME_BRIDGE_ACTION_ENABLE_TRANSFER_TIMER));
    assert(runtime_bridge_start_auxiliary_recovery(&bridge, input.now_ms) == 0);
    input.now_ms = 1011;
    assert(runtime_bridge_step(&bridge, &input) == RUNTIME_BRIDGE_ACTION_START_TRANSFER);
    input.transfer_status = RUNTIME_BRIDGE_TRANSFER_FAILED;
    assert(runtime_bridge_step(&bridge, &input) == (RUNTIME_BRIDGE_ACTION_DISABLE_TRANSFER_TIMER |
                                                    RUNTIME_BRIDGE_ACTION_RESTORE_NORMAL_USB));
    assert(bridge.mode == USB_RUNTIME_MODE_NORMAL);
    assert(bridge.phase == RUNTIME_BRIDGE_IDLE);
}

static void test_runs_startup_status_recovery(void) {
    RuntimeBridge bridge;
    runtime_bridge_init(&bridge);

    assert(runtime_bridge_start_status_recovery(NULL) == 0);
    assert(runtime_bridge_start_status_recovery(&bridge) ==
           (RUNTIME_BRIDGE_ACTION_INITIALIZE_DIRECT_TRANSFER |
            RUNTIME_BRIDGE_ACTION_ACTIVATE_UPDATER_USB));
    assert(runtime_bridge_active(&bridge));
    assert(bridge.mode == USB_RUNTIME_MODE_STATUS_BRIDGE);
    assert(runtime_bridge_start_status_recovery(&bridge) == 0);
}

static void test_runs_status_transition(void) {
    RuntimeBridge bridge;
    RuntimeBridgeInput input = input_at(20);
    runtime_bridge_init(&bridge);

    assert(runtime_bridge_start(&bridge, USB_RUNTIME_MODE_STATUS_BRIDGE) ==
           RUNTIME_BRIDGE_ACTION_MARK_WHEEL_STATUS);
    assert(runtime_bridge_step(&bridge, &input) == 0);
    input.marked_wheel_status_received = true;
    assert(runtime_bridge_step(&bridge, &input) ==
           (RUNTIME_BRIDGE_ACTION_PREPARE_USB | RUNTIME_BRIDGE_ACTION_INITIALIZE_DIRECT_TRANSFER));
    input.now_ms = 30;
    assert(runtime_bridge_step(&bridge, &input) == 0);
    input.now_ms = 31;
    assert(runtime_bridge_step(&bridge, &input) ==
           (RUNTIME_BRIDGE_ACTION_ENABLE_TRANSFER_TIMER | RUNTIME_BRIDGE_ACTION_START_TRANSFER));
}

static void test_retries_usb_transition_after_300_milliseconds(void) {
    RuntimeBridge bridge;
    RuntimeBridgeInput input = input_at(100);
    runtime_bridge_init(&bridge);

    assert(runtime_bridge_start(&bridge, USB_RUNTIME_MODE_USB_BRIDGE) == 0);
    assert(runtime_bridge_step(&bridge, &input) == 0);
    input.usb_bridge_ready = true;
    assert(runtime_bridge_step(&bridge, &input) ==
           (RUNTIME_BRIDGE_ACTION_PREPARE_USB | RUNTIME_BRIDGE_ACTION_ENABLE_TRANSFER_TIMER));
    input.now_ms = 110;
    assert(runtime_bridge_step(&bridge, &input) == 0);
    input.now_ms = 111;
    assert(runtime_bridge_step(&bridge, &input) == RUNTIME_BRIDGE_ACTION_START_TRANSFER);
    input.transfer_status = RUNTIME_BRIDGE_TRANSFER_PENDING;
    input.now_ms = 399;
    assert(runtime_bridge_step(&bridge, &input) == 0);
    input.now_ms = 400;
    assert(runtime_bridge_step(&bridge, &input) == 0);
    assert(bridge.phase == RUNTIME_BRIDGE_WAIT_TRANSFER);
    input.now_ms = 401;
    assert(runtime_bridge_step(&bridge, &input) == 0);
    assert(bridge.phase == RUNTIME_BRIDGE_WAIT_USB_READY);

    input.usb_bridge_ready = true;
    assert(runtime_bridge_step(&bridge, &input) ==
           (RUNTIME_BRIDGE_ACTION_PREPARE_USB | RUNTIME_BRIDGE_ACTION_ENABLE_TRANSFER_TIMER));
    input.now_ms = 411;
    assert(runtime_bridge_step(&bridge, &input) == 0);
    input.now_ms = 412;
    assert(runtime_bridge_step(&bridge, &input) == RUNTIME_BRIDGE_ACTION_START_TRANSFER);
    input.transfer_status = RUNTIME_BRIDGE_TRANSFER_COMPLETE;
    assert(runtime_bridge_step(&bridge, &input) == 0);
    input.now_ms = 701;
    assert(runtime_bridge_step(&bridge, &input) == 0);
    input.now_ms = 702;
    assert(runtime_bridge_step(&bridge, &input) == RUNTIME_BRIDGE_ACTION_ACTIVATE_UPDATER_USB);
}

static void test_runs_protocol_fast_path(void) {
    RuntimeBridge bridge;
    RuntimeBridgeInput input = input_at(50);
    runtime_bridge_init(&bridge);

    assert(runtime_bridge_start(&bridge, USB_RUNTIME_MODE_PROTOCOL_BRIDGE) ==
           (RUNTIME_BRIDGE_ACTION_ENABLE_TRANSFER_TIMER | RUNTIME_BRIDGE_ACTION_START_TRANSFER));
    input.transfer_status = RUNTIME_BRIDGE_TRANSFER_COMPLETE;
    assert(runtime_bridge_step(&bridge, &input) == RUNTIME_BRIDGE_ACTION_PREPARE_USB);
    input.now_ms = 149;
    assert(runtime_bridge_step(&bridge, &input) == 0);
    input.now_ms = 150;
    assert(runtime_bridge_step(&bridge, &input) == 0);
    input.now_ms = 151;
    assert(runtime_bridge_step(&bridge, &input) == RUNTIME_BRIDGE_ACTION_ACTIVATE_UPDATER_USB);
}

static void test_runs_protocol_fallback_on_acknowledgement(void) {
    RuntimeBridge bridge;
    RuntimeBridgeInput input = input_at(UINT32_MAX - 400);
    runtime_bridge_init(&bridge);

    runtime_bridge_start(&bridge, USB_RUNTIME_MODE_PROTOCOL_BRIDGE);
    input.transfer_status = RUNTIME_BRIDGE_TRANSFER_FAILED;
    assert(runtime_bridge_step(&bridge, &input) ==
           (RUNTIME_BRIDGE_ACTION_DISABLE_TRANSFER_TIMER |
            RUNTIME_BRIDGE_ACTION_REQUEST_PROTOCOL_COMMAND));

    input.transfer_status = RUNTIME_BRIDGE_TRANSFER_IDLE;
    input.now_ms += 10;
    assert(runtime_bridge_step(&bridge, &input) == 0);
    input.protocol_command_acknowledged = true;
    assert(runtime_bridge_step(&bridge, &input) ==
           (RUNTIME_BRIDGE_ACTION_PREPARE_USB | RUNTIME_BRIDGE_ACTION_ENABLE_TRANSFER_TIMER));
    input.now_ms += 499;
    assert(runtime_bridge_step(&bridge, &input) == 0);
    input.now_ms += 1;
    assert(runtime_bridge_step(&bridge, &input) == 0);
    input.now_ms += 1;
    assert(runtime_bridge_step(&bridge, &input) == RUNTIME_BRIDGE_ACTION_START_TRANSFER);
    input.transfer_status = RUNTIME_BRIDGE_TRANSFER_COMPLETE;
    assert(runtime_bridge_step(&bridge, &input) == 0);
    assert(runtime_bridge_step(&bridge, &input) == RUNTIME_BRIDGE_ACTION_ACTIVATE_UPDATER_USB);
}

static void test_runs_protocol_fallback_after_timeout(void) {
    RuntimeBridge bridge;
    RuntimeBridgeInput input = input_at(200);
    runtime_bridge_init(&bridge);

    runtime_bridge_start(&bridge, USB_RUNTIME_MODE_PROTOCOL_BRIDGE);
    input.transfer_status = RUNTIME_BRIDGE_TRANSFER_FAILED;
    runtime_bridge_step(&bridge, &input);
    input.transfer_status = RUNTIME_BRIDGE_TRANSFER_IDLE;
    input.now_ms = 1199;
    assert(runtime_bridge_step(&bridge, &input) == 0);
    input.now_ms = 1200;
    assert(runtime_bridge_step(&bridge, &input) == 0);
    input.now_ms = 1201;
    assert(runtime_bridge_step(&bridge, &input) ==
           (RUNTIME_BRIDGE_ACTION_PREPARE_USB | RUNTIME_BRIDGE_ACTION_ENABLE_TRANSFER_TIMER));
}

int main(void) {
    test_rejects_invalid_and_overlapping_transitions();
    test_runs_auxiliary_transition();
    test_runs_startup_auxiliary_recovery();
    test_runs_startup_status_recovery();
    test_runs_status_transition();
    test_retries_usb_transition_after_300_milliseconds();
    test_runs_protocol_fast_path();
    test_runs_protocol_fallback_on_acknowledgement();
    test_runs_protocol_fallback_after_timeout();
    return 0;
}
