#include "security/code.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/** @brief Internal security-code timing, control, and presentation constants. */
enum {
    SECURITY_CODE_PROMPT_DELAY_MS = 1000,          /**< Delay before showing a prompt. */
    SECURITY_CODE_ENTRY_DELAY_MS = 1000,           /**< Delay before accepting digit input. */
    SECURITY_CODE_REPEAT_DELAY_MS = 500,           /**< Delay between repeated input actions. */
    SECURITY_CODE_BLINK_DELAY_MS = 150,            /**< Selected-digit blink interval. */
    SECURITY_CODE_ENABLE_PRIMARY_CHORD = 0xe100,   /**< Direct enable chord. */
    SECURITY_CODE_DISABLE_PRIMARY_CHORD = 0xe800,  /**< Direct disable chord. */
    SECURITY_CODE_ADAPTER_COMMON_CHORD = 0x16,     /**< Common adapter activation chord. */
    SECURITY_CODE_ADAPTER_ENABLE = 0x01,           /**< Adapter enable bit. */
    SECURITY_CODE_ADAPTER_DISABLE = 0x08,          /**< Adapter disable bit. */
    SECURITY_CODE_ADAPTER_PREVIOUS = 0x02,         /**< Adapter previous bit. */
    SECURITY_CODE_ADAPTER_NEXT = 0x04,             /**< Adapter next bit. */
    SECURITY_CODE_PRIMARY_INCREMENT = 0x0100,      /**< Direct increment bit. */
    SECURITY_CODE_PRIMARY_DECREMENT = 0x0800,      /**< Direct decrement bit. */
    SECURITY_CODE_PRIMARY_PREVIOUS = 0x0200,       /**< Direct previous bit. */
    SECURITY_CODE_PRIMARY_NEXT = 0x0400,           /**< Direct next bit. */
    SECURITY_CODE_PRIMARY_SPECIAL_CANCEL = 0x1000, /**< Direct special-cancel bit. */
    SECURITY_CODE_SECONDARY_CONFIRM = 0x0200,      /**< Direct confirm bit. */
    SECURITY_CODE_SECONDARY_CANCEL = 0x0800,       /**< Direct cancel bit. */
    SECURITY_CODE_ADAPTER_CONFIRM = 0x08,          /**< Adapter confirm bit. */
    SECURITY_CODE_ADAPTER_CANCEL = 0x04,           /**< Adapter cancel bit. */
    SECURITY_CODE_ENABLE_PROMPT = 0x1e,            /**< Native enable prompt command. */
    SECURITY_CODE_DISABLE_PROMPT = 0x1f,           /**< Native disable prompt command. */
    SECURITY_CODE_FIRST_REPORT = 0x20,             /**< First selected-digit report mask. */
    SECURITY_CODE_SECOND_REPORT = 0x10,            /**< Second selected-digit report mask. */
    SECURITY_CODE_THIRD_REPORT = 0x08,             /**< Third selected-digit report mask. */
    SECURITY_CODE_GLYPH_L = 0x38,                  /**< Seven-segment L glyph. */
    SECURITY_CODE_GLYPH_O = 0x3f,                  /**< Seven-segment O glyph. */
    SECURITY_CODE_GLYPH_C = 0x39,                  /**< Seven-segment C glyph. */
};

/** @brief Normalized actions available during digit entry. */
typedef struct {
    bool increment; /**< True when the selected digit should increase. */
    bool decrement; /**< True when the selected digit should decrease. */
    bool previous;  /**< True when selection should move to the previous digit. */
    bool next;      /**< True when selection should move to the next digit. */
    bool confirm;   /**< True when entry should be confirmed. */
    bool cancel;    /**< True when entry should be cancelled. */
} SecurityCodeActionInput;

/**
 * @brief Tests a millisecond deadline with unsigned wraparound.
 *
 * Treats the forward half of the time range as reached and the reverse half as pending.
 *
 * @param[in] now_ms Current monotonic time in milliseconds.
 * @param[in] deadline_ms Deadline to test.
 * @return true when the deadline has been reached; false while it remains pending.
 */
static bool deadline_reached(uint32_t now_ms, uint32_t deadline_ms) {
    return now_ms - deadline_ms < UINT32_C(0x80000000);
}

/**
 * @brief Reports whether the selected wheel path uses the auxiliary digit marker.
 *
 * Selects the five direct report modes and every connected adapter path.
 *
 * @param[in] input Current attached-wheel and adapter input.
 * @return true when digit selection is carried by the auxiliary report; false otherwise.
 */
static bool report_display_used(const SecurityCodeInput *input) {
    return input->adapter_connected || input->wheel_mode == 0x0a || input->wheel_mode == 0x0c ||
           input->wheel_mode == 0x0f || input->wheel_mode == 0x17 || input->wheel_mode == 0x1c;
}

/**
 * @brief Detects the security-code enable chord.
 *
 * Accepts either the complete direct-wheel mask or the adapter's common chord with its enable bit.
 *
 * @param[in] input Current attached-wheel and adapter input.
 * @return true when the enable chord is held; false otherwise.
 */
static bool enable_chord_held(const SecurityCodeInput *input) {
    bool direct = (input->primary_buttons & SECURITY_CODE_ENABLE_PRIMARY_CHORD) ==
                  SECURITY_CODE_ENABLE_PRIMARY_CHORD;
    bool adapter = input->adapter_connected &&
                   (input->adapter_buttons[1] & SECURITY_CODE_ADAPTER_COMMON_CHORD) ==
                       SECURITY_CODE_ADAPTER_COMMON_CHORD &&
                   (input->adapter_buttons[0] & SECURITY_CODE_ADAPTER_ENABLE) != 0;
    return direct || adapter;
}

/**
 * @brief Detects the security-code disable chord.
 *
 * Accepts either the complete direct-wheel mask or the adapter's common chord with its disable bit.
 *
 * @param[in] input Current attached-wheel and adapter input.
 * @return true when the disable chord is held; false otherwise.
 */
static bool disable_chord_held(const SecurityCodeInput *input) {
    bool direct = (input->primary_buttons & SECURITY_CODE_DISABLE_PRIMARY_CHORD) ==
                  SECURITY_CODE_DISABLE_PRIMARY_CHORD;
    bool adapter = input->adapter_connected &&
                   (input->adapter_buttons[1] & SECURITY_CODE_ADAPTER_COMMON_CHORD) ==
                       SECURITY_CODE_ADAPTER_COMMON_CHORD &&
                   (input->adapter_buttons[0] & SECURITY_CODE_ADAPTER_DISABLE) != 0;
    return direct || adapter;
}

/**
 * @brief Normalizes digit-entry controls for the active input path.
 *
 * Gives a connected adapter exclusive control of all six actions. Direct modes 0x06 and 0x15 use
 * their primary special-cancel bit; other direct modes use the secondary cancel bit.
 *
 * @param[in] input Current attached-wheel and adapter input.
 * @return Logical digit-entry actions.
 */
static SecurityCodeActionInput action_input(const SecurityCodeInput *input) {
    if (input->adapter_connected) {
        return (SecurityCodeActionInput){
            .increment = (input->adapter_buttons[0] & SECURITY_CODE_ADAPTER_ENABLE) != 0,
            .decrement = (input->adapter_buttons[0] & SECURITY_CODE_ADAPTER_DISABLE) != 0,
            .previous = (input->adapter_buttons[0] & SECURITY_CODE_ADAPTER_PREVIOUS) != 0,
            .next = (input->adapter_buttons[0] & SECURITY_CODE_ADAPTER_NEXT) != 0,
            .confirm = (input->adapter_buttons[2] & SECURITY_CODE_ADAPTER_CONFIRM) != 0,
            .cancel = (input->adapter_buttons[2] & SECURITY_CODE_ADAPTER_CANCEL) != 0,
        };
    }

    bool special_cancel = input->wheel_mode == 0x06 || input->wheel_mode == 0x15;
    return (SecurityCodeActionInput){
        .increment = (input->primary_buttons & SECURITY_CODE_PRIMARY_INCREMENT) != 0,
        .decrement = (input->primary_buttons & SECURITY_CODE_PRIMARY_DECREMENT) != 0,
        .previous = (input->primary_buttons & SECURITY_CODE_PRIMARY_PREVIOUS) != 0,
        .next = (input->primary_buttons & SECURITY_CODE_PRIMARY_NEXT) != 0,
        .confirm = (input->secondary_buttons & SECURITY_CODE_SECONDARY_CONFIRM) != 0,
        .cancel = special_cancel
                      ? (input->primary_buttons & SECURITY_CODE_PRIMARY_SPECIAL_CANCEL) != 0
                      : (input->secondary_buttons & SECURITY_CODE_SECONDARY_CANCEL) != 0,
    };
}

/**
 * @brief Advances the five-step selected-digit blink cycle.
 *
 * Advances at most once per service pass and restarts the 150-millisecond deadline after each step.
 *
 * @param[in,out] code Security-code interaction state.
 * @param[in] now_ms Current monotonic time in milliseconds.
 */
static void advance_blink(SecurityCode *code, uint32_t now_ms) {
    if (!deadline_reached(now_ms, code->blink_deadline_ms)) {
        return;
    }
    code->blink_deadline_ms = now_ms + SECURITY_CODE_BLINK_DELAY_MS;
    code->blink_phase = code->blink_phase > 3 ? 0 : (uint8_t)(code->blink_phase + 1u);
}

/**
 * @brief Builds the enable or disable prompt presentation.
 *
 * Uses the native prompt command in modes 0x0C, 0x06, and 0x15. Mode 0x0C leaves the local glyphs
 * clear, while every other mode presents LOC.
 *
 * @param[in] code Security-code interaction state.
 * @param[in] input Current attached-wheel and adapter input.
 * @return Prompt presentation and optional native command.
 */
static SecurityCodeUpdate prompt_update(const SecurityCode *code, const SecurityCodeInput *input) {
    SecurityCodeUpdate update = {
        .presentation.kind = SECURITY_CODE_PRESENTATION_PROMPT,
        .prompt_command =
            code->enable_requested ? SECURITY_CODE_ENABLE_PROMPT : SECURITY_CODE_DISABLE_PROMPT,
        .active = true,
    };
    if (input->wheel_mode != 0x0c) {
        update.presentation.glyphs[0] = SECURITY_CODE_GLYPH_L;
        update.presentation.glyphs[1] = SECURITY_CODE_GLYPH_O;
        update.presentation.glyphs[2] = SECURITY_CODE_GLYPH_C;
    }
    if (input->wheel_mode != 0x0c && input->wheel_mode != 0x06 && input->wheel_mode != 0x15) {
        update.prompt_command = 0;
    }
    return update;
}

/**
 * @brief Builds the current three-digit entry presentation.
 *
 * Encodes all three decimal digits, then either publishes the selected-digit report mask or blanks
 * the digit selected at the start of this update during blink phase zero. This preserves the
 * firmware's one-update render cadence when an action moves selection.
 *
 * @param[in] code Security-code interaction state.
 * @param[in] input Current attached-wheel and adapter input.
 * @param[in] selected_digit Digit selected before this update's action is applied.
 * @return Digit presentation for the active entry field.
 */
static SecurityCodePresentation digit_presentation(const SecurityCode *code,
                                                   const SecurityCodeInput *input,
                                                   uint8_t selected_digit) {
    /** @brief Seven-segment glyphs for decimal digits. */
    static const uint8_t digit_glyphs[] = {0x3f, 0x06, 0x5b, 0x4f, 0x66,
                                           0x6d, 0x7d, 0x07, 0x7f, 0x6f};
    /** @brief Native report masks for selected digits. */
    static const uint8_t reports[] = {SECURITY_CODE_FIRST_REPORT, SECURITY_CODE_SECOND_REPORT,
                                      SECURITY_CODE_THIRD_REPORT};
    SecurityCodePresentation presentation = {.kind = SECURITY_CODE_PRESENTATION_DIGITS};
    for (uint8_t digit = 0; digit < SECURITY_CODE_DIGIT_COUNT; digit++) {
        presentation.glyphs[digit] = digit_glyphs[code->entered_digits[digit]];
    }
    if (report_display_used(input)) {
        presentation.report = reports[selected_digit];
    } else if (code->blink_phase == 0) {
        presentation.glyphs[selected_digit] = 0;
    }
    return presentation;
}

/**
 * @brief Applies one ready digit-entry action in firmware priority order.
 *
 * Processes increment, decrement, previous, next, confirm, and cancel in that order. Decimal edits
 * and digit navigation wrap, and the four repeating actions start a 500-millisecond delay.
 *
 * @param[in,out] code Security-code interaction state.
 * @param[in] actions Normalized digit-entry actions.
 * @param[in] now_ms Current monotonic time in milliseconds.
 */
static void apply_action(SecurityCode *code, SecurityCodeActionInput actions, uint32_t now_ms) {
    if (actions.increment) {
        uint8_t *digit = &code->entered_digits[code->selected_digit];
        *digit = *digit == 9 ? 0 : (uint8_t)(*digit + 1u);
    } else if (actions.decrement) {
        uint8_t *digit = &code->entered_digits[code->selected_digit];
        *digit = *digit == 0 ? 9 : (uint8_t)(*digit - 1u);
    } else if (actions.previous) {
        code->selected_digit = code->selected_digit == 0 ? 2 : (uint8_t)(code->selected_digit - 1u);
    } else if (actions.next) {
        code->selected_digit = code->selected_digit == 2 ? 0 : (uint8_t)(code->selected_digit + 1u);
    } else if (actions.confirm || actions.cancel) {
        code->phase = SECURITY_CODE_CONFIRM;
        return;
    } else {
        return;
    }
    code->input_deadline_ms = now_ms + SECURITY_CODE_REPEAT_DELAY_MS;
}

void security_code_init(SecurityCode *code) {
    if (code != NULL) {
        *code = (SecurityCode){0};
    }
}

bool security_code_interaction_active(const SecurityCode *code) {
    return code != NULL && code->phase != SECURITY_CODE_INACTIVE;
}

SecurityCodeUpdate security_code_update(SecurityCode *code, SecurityCodeSettings *settings,
                                        const SecurityCodeInput *input, uint32_t now_ms) {
    SecurityCodeUpdate update = {0};
    if (code == NULL || settings == NULL || input == NULL) {
        return update;
    }

    if (code->phase == SECURITY_CODE_INACTIVE) {
        bool enable = !settings->enabled && enable_chord_held(input);
        bool disable = settings->enabled && disable_chord_held(input);
        if (enable || disable) {
            code->enable_requested = enable;
            code->phase = SECURITY_CODE_PREPARE;
        }
        return update;
    }

    update.active = true;
    if (code->phase == SECURITY_CODE_PREPARE) {
        code->input_deadline_ms = now_ms + SECURITY_CODE_PROMPT_DELAY_MS;
        code->phase = SECURITY_CODE_WAIT;
        return prompt_update(code, input);
    }
    if (code->phase == SECURITY_CODE_WAIT) {
        if (deadline_reached(now_ms, code->input_deadline_ms)) {
            for (uint8_t digit = 0; digit < SECURITY_CODE_DIGIT_COUNT; digit++) {
                code->entered_digits[digit] = 0;
            }
            code->selected_digit = 0;
            code->input_deadline_ms = now_ms + SECURITY_CODE_ENTRY_DELAY_MS;
            code->phase = SECURITY_CODE_EDIT;
        }
        return update;
    }
    if (code->phase == SECURITY_CODE_EDIT) {
        uint8_t selected_digit = code->selected_digit;
        SecurityCodeActionInput actions = action_input(input);
        if (deadline_reached(now_ms, code->input_deadline_ms)) {
            apply_action(code, actions, now_ms);
        } else if (!actions.increment && !actions.decrement && !actions.previous && !actions.next) {
            code->input_deadline_ms = now_ms;
        }
        advance_blink(code, now_ms);
        update.presentation = digit_presentation(code, input, selected_digit);
        return update;
    }
    if (code->phase == SECURITY_CODE_CONFIRM) {
        bool matches = true;
        for (uint8_t digit = 0; digit < SECURITY_CODE_DIGIT_COUNT; digit++) {
            matches = matches && settings->digits[digit] == code->entered_digits[digit];
        }
        if (code->enable_requested) {
            for (uint8_t digit = 0; digit < SECURITY_CODE_DIGIT_COUNT; digit++) {
                settings->digits[digit] = code->entered_digits[digit];
            }
            settings->enabled = true;
            update.settings_changed = true;
            code->phase = SECURITY_CODE_INACTIVE;
        } else if (matches) {
            settings->enabled = false;
            update.settings_changed = true;
            code->phase = SECURITY_CODE_INACTIVE;
        } else {
            code->phase = SECURITY_CODE_PREPARE;
            code->input_deadline_ms = now_ms + SECURITY_CODE_PROMPT_DELAY_MS;
            update.mismatch = true;
        }
        update.presentation.kind = SECURITY_CODE_PRESENTATION_CLEAR;
        return update;
    }

    code->phase = SECURITY_CODE_INACTIVE;
    update.active = false;
    return update;
}
