#include <assert.h>
#include <stdbool.h>
#include <stdint.h>

#include "security/code.h"

static void enter_edit_phase(SecurityCode *code, SecurityCodeSettings *settings,
                             SecurityCodeInput *input) {
    input->primary_buttons = 0xe100;
    SecurityCodeUpdate update = security_code_update(code, settings, input, 0);
    assert(!update.active);
    assert(code->phase == SECURITY_CODE_PREPARE);

    input->primary_buttons = 0;
    update = security_code_update(code, settings, input, 1);
    assert(update.active);
    assert(update.presentation.kind == SECURITY_CODE_PRESENTATION_PROMPT);
    assert(code->phase == SECURITY_CODE_WAIT);

    update = security_code_update(code, settings, input, 1000);
    assert(update.presentation.kind == SECURITY_CODE_PRESENTATION_KEEP);
    assert(code->phase == SECURITY_CODE_WAIT);
    update = security_code_update(code, settings, input, 1001);
    assert(update.presentation.kind == SECURITY_CODE_PRESENTATION_KEEP);
    assert(code->phase == SECURITY_CODE_EDIT);
}

static void test_direct_chords_follow_activation_state(void) {
    SecurityCode code;
    SecurityCodeSettings settings = {0};
    SecurityCodeInput input = {.primary_buttons = 0xe000};
    security_code_init(&code);

    assert(!security_code_update(&code, &settings, &input, 0).active);
    assert(code.phase == SECURITY_CODE_INACTIVE);
    input.primary_buttons = 0xe100;
    assert(!security_code_update(&code, &settings, &input, 1).active);
    assert(code.phase == SECURITY_CODE_PREPARE);
    assert(code.enable_requested);

    security_code_init(&code);
    settings.enabled = true;
    input.primary_buttons = 0xe100;
    assert(!security_code_update(&code, &settings, &input, 2).active);
    assert(code.phase == SECURITY_CODE_INACTIVE);
    input.primary_buttons = 0xe800;
    assert(!security_code_update(&code, &settings, &input, 3).active);
    assert(code.phase == SECURITY_CODE_PREPARE);
    assert(!code.enable_requested);
}

static void test_adapter_chords_start_requests(void) {
    SecurityCode code;
    SecurityCodeSettings settings = {0};
    SecurityCodeInput input = {
        .adapter_buttons = {0x01, 0x16, 0},
        .adapter_connected = true,
    };
    security_code_init(&code);

    assert(!security_code_update(&code, &settings, &input, 0).active);
    assert(code.phase == SECURITY_CODE_PREPARE);
    assert(code.enable_requested);

    security_code_init(&code);
    settings.enabled = true;
    input.adapter_buttons[0] = 0x08;
    assert(!security_code_update(&code, &settings, &input, 1).active);
    assert(code.phase == SECURITY_CODE_PREPARE);
    assert(!code.enable_requested);
}

static void test_prompt_and_entry_delays(void) {
    SecurityCode code;
    SecurityCodeSettings settings = {0};
    SecurityCodeInput input = {.wheel_mode = 6};
    security_code_init(&code);
    enter_edit_phase(&code, &settings, &input);

    assert(code.entered_digits[0] == 0);
    assert(code.entered_digits[1] == 0);
    assert(code.entered_digits[2] == 0);
    assert(code.input_deadline_ms == 2001);

    input.primary_buttons = 0x0100;
    SecurityCodeUpdate update = security_code_update(&code, &settings, &input, 2000);
    assert(code.entered_digits[0] == 0);
    input.primary_buttons = 0;
    update = security_code_update(&code, &settings, &input, 2000);
    assert(update.presentation.kind == SECURITY_CODE_PRESENTATION_DIGITS);
    assert(code.input_deadline_ms == 2000);
    input.primary_buttons = 0x0100;
    security_code_update(&code, &settings, &input, 2001);
    assert(code.entered_digits[0] == 1);
    assert(code.input_deadline_ms == 2501);
}

static void test_digit_actions_wrap_and_follow_priority(void) {
    SecurityCode code = {
        .entered_digits = {9, 0, 0},
        .phase = SECURITY_CODE_EDIT,
    };
    SecurityCodeSettings settings = {0};
    SecurityCodeInput input = {.primary_buttons = 0x0900};

    security_code_update(&code, &settings, &input, 0);
    assert(code.entered_digits[0] == 0);
    input.primary_buttons = 0;
    security_code_update(&code, &settings, &input, 1);
    input.primary_buttons = 0x0800;
    security_code_update(&code, &settings, &input, 2);
    assert(code.entered_digits[0] == 9);

    input.primary_buttons = 0;
    security_code_update(&code, &settings, &input, 3);
    input.primary_buttons = 0x0200;
    security_code_update(&code, &settings, &input, 4);
    assert(code.selected_digit == 2);
    input.primary_buttons = 0;
    security_code_update(&code, &settings, &input, 5);
    input.primary_buttons = 0x0400;
    security_code_update(&code, &settings, &input, 6);
    assert(code.selected_digit == 0);
}

static void test_adapter_actions_override_direct_controls(void) {
    SecurityCode code = {.phase = SECURITY_CODE_EDIT};
    SecurityCodeSettings settings = {0};
    SecurityCodeInput input = {
        .primary_buttons = 0x0100,
        .adapter_buttons = {0x04, 0, 0},
        .adapter_connected = true,
    };

    security_code_update(&code, &settings, &input, 0);
    assert(code.entered_digits[0] == 0);
    assert(code.selected_digit == 1);
}

static void test_special_cancel_replaces_secondary_cancel(void) {
    SecurityCode code = {.phase = SECURITY_CODE_EDIT};
    SecurityCodeSettings settings = {0};
    SecurityCodeInput input = {
        .wheel_mode = 6,
        .secondary_buttons = 0x0800,
    };

    security_code_update(&code, &settings, &input, 0);
    assert(code.phase == SECURITY_CODE_EDIT);
    input.primary_buttons = 0x1000;
    security_code_update(&code, &settings, &input, 1);
    assert(code.phase == SECURITY_CODE_CONFIRM);
}

static void test_enable_confirmation_stores_entered_code(void) {
    SecurityCode code = {
        .entered_digits = {4, 2, 7},
        .phase = SECURITY_CODE_CONFIRM,
        .enable_requested = true,
    };
    SecurityCodeSettings settings = {0};
    SecurityCodeInput input = {0};

    SecurityCodeUpdate update = security_code_update(&code, &settings, &input, 0);
    assert(update.active);
    assert(update.settings_changed);
    assert(update.presentation.kind == SECURITY_CODE_PRESENTATION_CLEAR);
    assert(settings.enabled);
    assert(settings.digits[0] == 4);
    assert(settings.digits[1] == 2);
    assert(settings.digits[2] == 7);
    assert(code.phase == SECURITY_CODE_INACTIVE);
}

static void test_disable_confirmation_requires_matching_code(void) {
    SecurityCode code = {
        .entered_digits = {1, 2, 4},
        .phase = SECURITY_CODE_CONFIRM,
    };
    SecurityCodeSettings settings = {
        .digits = {1, 2, 3},
        .enabled = true,
    };
    SecurityCodeInput input = {0};

    SecurityCodeUpdate update = security_code_update(&code, &settings, &input, 100);
    assert(update.active);
    assert(update.mismatch);
    assert(!update.settings_changed);
    assert(settings.enabled);
    assert(code.phase == SECURITY_CODE_PREPARE);

    code.entered_digits[2] = 3;
    code.phase = SECURITY_CODE_CONFIRM;
    update = security_code_update(&code, &settings, &input, 101);
    assert(update.settings_changed);
    assert(!update.mismatch);
    assert(!settings.enabled);
    assert(settings.digits[0] == 1);
    assert(settings.digits[1] == 2);
    assert(settings.digits[2] == 3);
}

static void test_prompt_modes_and_digit_markers(void) {
    SecurityCode code = {
        .entered_digits = {1, 2, 3},
        .phase = SECURITY_CODE_PREPARE,
        .enable_requested = true,
    };
    SecurityCodeSettings settings = {0};
    SecurityCodeInput input = {.wheel_mode = 0x0c};

    SecurityCodeUpdate update = security_code_update(&code, &settings, &input, 0);
    assert(update.prompt_command == 0x1e);
    assert(update.presentation.glyphs[0] == 0);
    assert(update.presentation.glyphs[1] == 0);
    assert(update.presentation.glyphs[2] == 0);

    code.phase = SECURITY_CODE_PREPARE;
    input.wheel_mode = 6;
    update = security_code_update(&code, &settings, &input, 1);
    assert(update.prompt_command == 0x1e);
    assert(update.presentation.glyphs[0] == 0x38);
    assert(update.presentation.glyphs[1] == 0x3f);
    assert(update.presentation.glyphs[2] == 0x39);

    code.phase = SECURITY_CODE_EDIT;
    code.selected_digit = 1;
    code.blink_phase = 0;
    code.blink_deadline_ms = 100;
    input.wheel_mode = 1;
    update = security_code_update(&code, &settings, &input, 50);
    assert(update.presentation.report == 0);
    assert(update.presentation.glyphs[0] == 0x06);
    assert(update.presentation.glyphs[1] == 0);
    assert(update.presentation.glyphs[2] == 0x4f);

    code.phase = SECURITY_CODE_EDIT;
    code.blink_phase = 0;
    code.blink_deadline_ms = 100;
    input.wheel_mode = 0x0a;
    update = security_code_update(&code, &settings, &input, 50);
    assert(update.presentation.report == 0x10);
    assert(update.presentation.glyphs[1] == 0x5b);
}

static void test_deadlines_wrap_safely(void) {
    SecurityCode code = {
        .phase = SECURITY_CODE_WAIT,
        .input_deadline_ms = 10,
    };
    SecurityCodeSettings settings = {0};
    SecurityCodeInput input = {0};

    security_code_update(&code, &settings, &input, UINT32_MAX);
    assert(code.phase == SECURITY_CODE_WAIT);
    security_code_update(&code, &settings, &input, 10);
    assert(code.phase == SECURITY_CODE_EDIT);
}

int main(void) {
    test_direct_chords_follow_activation_state();
    test_adapter_chords_start_requests();
    test_prompt_and_entry_delays();
    test_digit_actions_wrap_and_follow_priority();
    test_adapter_actions_override_direct_controls();
    test_special_cancel_replaces_secondary_cancel();
    test_enable_confirmation_stores_entered_code();
    test_disable_confirmation_requires_matching_code();
    test_prompt_modes_and_digit_markers();
    test_deadlines_wrap_safely();
    return 0;
}
