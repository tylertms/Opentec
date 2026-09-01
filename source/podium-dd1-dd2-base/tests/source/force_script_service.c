#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "force_feedback/script_service.h"

static uint32_t float_bits(float value) {
    uint32_t bits;
    memcpy(&bits, &value, sizeof(bits));
    return bits;
}

static ForceFeedbackScriptStore prepare_store(void) {
    ForceFeedbackScriptStore store = {
        .data =
            {
                0xa0,
                0x10,
                5,
                0x40,
                0xa0,
                0x10,
                9,
                0x40,
                0x07,
            },
        .slots =
            {
                [0] = {.offset = 0, .size = 4, .allocated = true},
                [2] = {.offset = 4, .size = 4, .allocated = true},
                [3] = {.offset = 8, .size = 1, .allocated = true},
            },
        .used = 9,
    };
    return store;
}

static void test_runs_active_slots_in_order(void) {
    ForceFeedbackScriptRuntime runtime = {.active_slot = 7};
    runtime.slots[0].state = FORCE_FEEDBACK_SCRIPT_SLOT_ACTIVE;
    runtime.slots[1].state = FORCE_FEEDBACK_SCRIPT_SLOT_PAUSED;
    runtime.slots[2].state = FORCE_FEEDBACK_SCRIPT_SLOT_ACTIVE;
    runtime.slots[0].execution_count = 1;

    ForceFeedbackScriptClock clock = {0};
    clock.slot_ticks[0] = 50;
    clock.slot_ticks[2] = UINT32_C(0x337f9801);
    ForceFeedbackScriptStore store = prepare_store();

    assert(!force_feedback_script_service_run(&runtime, &store, &clock));

    assert(runtime.active_slot == 2);
    assert(runtime.slots[0].values[0] == 5);
    assert(runtime.slots[2].values[0] == 9);
    assert(runtime.slots[1].values[0] == 0);
    assert(runtime.slots[0].execution_count == 2);
    assert(runtime.slots[0].tick_snapshot == 50);
    assert(runtime.slots[0].average_rate == float_bits(250000.0f));
    assert(runtime.slots[0].delta_rate == float_bits(0.0f));
    assert(runtime.slots[2].tick_snapshot == 0);
    assert(clock.slot_ticks[2] == 0);
    assert(clock.active_slot == 2);
    assert(!clock.script_executing);
}

static void test_preserves_completion_result(void) {
    ForceFeedbackScriptRuntime runtime = {0};
    runtime.slots[3].state = FORCE_FEEDBACK_SCRIPT_SLOT_ACTIVE;
    ForceFeedbackScriptClock clock = {0};
    ForceFeedbackScriptStore store = prepare_store();

    assert(!force_feedback_script_service_run(&runtime, &store, &clock));

    assert(runtime.slots[3].state == FORCE_FEEDBACK_SCRIPT_SLOT_INACTIVE);
    assert(runtime.slots[3].execution_count == 1);
}

static void test_runs_slot_fifteen(void) {
    ForceFeedbackScriptRuntime runtime = {0};
    runtime.slots[15].state = FORCE_FEEDBACK_SCRIPT_SLOT_ACTIVE;
    ForceFeedbackScriptClock clock = {0};
    ForceFeedbackScriptStore store = {0};
    store.data[0] = 0xa0;
    store.data[1] = 0x10;
    store.data[2] = 7;
    store.data[3] = 0x40;
    store.slots[15] = (ForceFeedbackScriptStorageSlot){.offset = 0, .size = 4, .allocated = true};

    assert(!force_feedback_script_service_run(&runtime, &store, &clock));
    assert(runtime.active_slot == 15);
    assert(runtime.slots[15].values[0] == 7);
    assert(runtime.slots[15].execution_count == 1);
}

static void test_reports_slot_faults(void) {
    ForceFeedbackScriptRuntime runtime = {0};
    runtime.slots[0].state = FORCE_FEEDBACK_SCRIPT_SLOT_ACTIVE;
    ForceFeedbackScriptClock clock = {0};
    ForceFeedbackScriptStore store = prepare_store();
    store.data[0] = 0x0a;
    store.slots[0].size = 1;

    assert(!force_feedback_script_service_run(&runtime, &store, &clock));
    assert(runtime.slots[0].state == FORCE_FEEDBACK_SCRIPT_SLOT_FAULT);
    assert(runtime.slots[0].execution_count == 1);

    assert(!force_feedback_script_service_run(&runtime, &store, &clock));
    assert(!force_feedback_script_service_run(NULL, &store, &clock));
    assert(!force_feedback_script_service_run(&runtime, NULL, &clock));
    assert(!force_feedback_script_service_run(&runtime, &store, NULL));
}

int main(void) {
    test_runs_active_slots_in_order();
    test_preserves_completion_result();
    test_runs_slot_fifteen();
    test_reports_slot_faults();
    return 0;
}
