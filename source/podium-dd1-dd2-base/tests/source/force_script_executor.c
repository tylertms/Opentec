#include <assert.h>
#include <stddef.h>
#include <stdint.h>

#include "force_feedback/script_executor.h"

static void test_executes_records_in_order(void) {
    ForceFeedbackScriptRuntime runtime = {.active_slot = 2};
    const uint8_t script[] = {
        0x10, 0x02, 0x02, 0x20, 0x12, 0x20, 0x13, 0x40, 0x00, 0x00, 0x00, 0x21,
    };
    assert(force_feedback_script_execute(&runtime, script, sizeof(script)) ==
           FORCE_FEEDBACK_SCRIPT_EXECUTION_FINISHED);
    assert(runtime.variables[0] == UINT32_C(0x40000000));
    assert(runtime.variables[1] == UINT32_C(0x40800000));
}

static void test_suppresses_selected_records(void) {
    ForceFeedbackScriptRuntime runtime = {.active_slot = 0};
    const uint8_t script[] = {
        0x01, 0x10, 1, 0x10, 0x02, 0x02, 0x20, 0x10, 0x02, 0x02, 0x21,
    };
    assert(force_feedback_script_execute(&runtime, script, sizeof(script)) ==
           FORCE_FEEDBACK_SCRIPT_EXECUTION_FINISHED);
    assert(runtime.variables[0] == 0);
    assert(runtime.variables[1] == UINT32_C(0x40000000));
}

static void test_conditionally_suppresses_records(void) {
    ForceFeedbackScriptRuntime runtime = {.active_slot = 0};
    const uint8_t zero_script[] = {
        0x02, 0x00, 0x10, 1, 0x10, 0x02, 0x02, 0x20, 0x10, 0x02, 0x02, 0x21,
    };
    assert(force_feedback_script_execute(&runtime, zero_script, sizeof(zero_script)) ==
           FORCE_FEEDBACK_SCRIPT_EXECUTION_FINISHED);
    assert(runtime.variables[0] == 0);
    assert(runtime.variables[1] == UINT32_C(0x40000000));

    runtime = (ForceFeedbackScriptRuntime){.active_slot = 0};
    const uint8_t nonzero_script[] = {
        0x03, 0x02, 0x10, 1, 0x10, 0x02, 0x02, 0x20, 0x10, 0x02, 0x02, 0x21,
    };
    assert(force_feedback_script_execute(&runtime, nonzero_script, sizeof(nonzero_script)) ==
           FORCE_FEEDBACK_SCRIPT_EXECUTION_FINISHED);
    assert(runtime.variables[0] == 0);
    assert(runtime.variables[1] == UINT32_C(0x40000000));
}

static void test_stop_commands_leave_slot_state(void) {
    ForceFeedbackScriptRuntime runtime = {.active_slot = 4};
    runtime.slots[4].state = FORCE_FEEDBACK_SCRIPT_SLOT_ACTIVE;
    const uint8_t stop[] = {0x04, 0x10, 0x02, 0x02, 0x20};
    assert(force_feedback_script_execute(&runtime, stop, sizeof(stop)) ==
           FORCE_FEEDBACK_SCRIPT_EXECUTION_FINISHED);
    assert(runtime.slots[4].state == FORCE_FEEDBACK_SCRIPT_SLOT_ACTIVE);
    assert(runtime.variables[0] == 0);

    const uint8_t stop_if_nonzero[] = {0x06, 0x02};
    assert(force_feedback_script_execute(&runtime, stop_if_nonzero, sizeof(stop_if_nonzero)) ==
           FORCE_FEEDBACK_SCRIPT_EXECUTION_FINISHED);
}

static void test_completion_commands_deactivate_slot(void) {
    ForceFeedbackScriptRuntime runtime = {.active_slot = 5};
    runtime.slots[5].state = FORCE_FEEDBACK_SCRIPT_SLOT_ACTIVE;
    const uint8_t complete_if_zero[] = {0x08, 0x00};
    assert(force_feedback_script_execute(&runtime, complete_if_zero, sizeof(complete_if_zero)) ==
           FORCE_FEEDBACK_SCRIPT_EXECUTION_COMPLETED);
    assert(runtime.slots[5].state == FORCE_FEEDBACK_SCRIPT_SLOT_INACTIVE);

    runtime.slots[5].state = FORCE_FEEDBACK_SCRIPT_SLOT_ACTIVE;
    const uint8_t complete[] = {0x07};
    assert(force_feedback_script_execute(&runtime, complete, sizeof(complete)) ==
           FORCE_FEEDBACK_SCRIPT_EXECUTION_COMPLETED);
    assert(runtime.slots[5].state == FORCE_FEEDBACK_SCRIPT_SLOT_INACTIVE);
}

static void test_faults_invalid_scripts(void) {
    ForceFeedbackScriptRuntime runtime = {.active_slot = 1};
    runtime.slots[1].state = FORCE_FEEDBACK_SCRIPT_SLOT_ACTIVE;
    const uint8_t invalid_command[] = {0x0a};
    assert(force_feedback_script_execute(&runtime, invalid_command, sizeof(invalid_command)) ==
           FORCE_FEEDBACK_SCRIPT_EXECUTION_SILENT_FAULT);
    assert(runtime.slots[1].state == FORCE_FEEDBACK_SCRIPT_SLOT_FAULT);

    runtime.slots[1].state = FORCE_FEEDBACK_SCRIPT_SLOT_ACTIVE;
    const uint8_t invalid_operation[] = {0x13, 0x02, 0x00, 0x20};
    assert(force_feedback_script_execute(&runtime, invalid_operation, sizeof(invalid_operation)) ==
           FORCE_FEEDBACK_SCRIPT_EXECUTION_FAULT);
    assert(runtime.slots[1].state == FORCE_FEEDBACK_SCRIPT_SLOT_FAULT);

    runtime.slots[1].state = FORCE_FEEDBACK_SCRIPT_SLOT_ACTIVE;
    const uint8_t unfinished_advance[] = {0x01, 0x10, 2, 0x00};
    assert(
        force_feedback_script_execute(&runtime, unfinished_advance, sizeof(unfinished_advance)) ==
        FORCE_FEEDBACK_SCRIPT_EXECUTION_FAULT);
    assert(runtime.slots[1].state == FORCE_FEEDBACK_SCRIPT_SLOT_FAULT);
}

int main(void) {
    test_executes_records_in_order();
    test_suppresses_selected_records();
    test_conditionally_suppresses_records();
    test_stop_commands_leave_slot_state();
    test_completion_commands_deactivate_slot();
    test_faults_invalid_scripts();
    return 0;
}
