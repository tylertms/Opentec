#ifndef OPENTEC_BASE_SECURITY_CODE_H
#define OPENTEC_BASE_SECURITY_CODE_H

#include <stdbool.h>
#include <stdint.h>

enum { SECURITY_CODE_DIGIT_COUNT = 3 };

typedef struct {
    uint8_t digits[SECURITY_CODE_DIGIT_COUNT];
    bool enabled;
} SecurityCodeSettings;

typedef struct {
    uint8_t wheel_mode;
    uint16_t primary_buttons;
    uint16_t secondary_buttons;
    uint8_t adapter_buttons[3];
    bool adapter_connected;
} SecurityCodeInput;

typedef enum {
    SECURITY_CODE_INACTIVE,
    SECURITY_CODE_PREPARE,
    SECURITY_CODE_WAIT,
    SECURITY_CODE_EDIT,
    SECURITY_CODE_CONFIRM,
} SecurityCodePhase;

typedef enum {
    SECURITY_CODE_PRESENTATION_KEEP,
    SECURITY_CODE_PRESENTATION_PROMPT,
    SECURITY_CODE_PRESENTATION_DIGITS,
    SECURITY_CODE_PRESENTATION_CLEAR,
} SecurityCodePresentationKind;

typedef struct {
    SecurityCodePresentationKind kind;
    uint8_t glyphs[SECURITY_CODE_DIGIT_COUNT];
    uint16_t report;
} SecurityCodePresentation;

typedef struct {
    SecurityCodePresentation presentation;
    uint8_t prompt_command;
    bool active;
    bool settings_changed;
    bool mismatch;
} SecurityCodeUpdate;

typedef struct {
    uint8_t entered_digits[SECURITY_CODE_DIGIT_COUNT];
    uint32_t input_deadline_ms;
    uint32_t blink_deadline_ms;
    SecurityCodePhase phase;
    uint8_t selected_digit;
    uint8_t blink_phase;
    bool enable_requested;
} SecurityCode;

void security_code_init(SecurityCode *code);
SecurityCodeUpdate security_code_update(SecurityCode *code, SecurityCodeSettings *settings,
                                        const SecurityCodeInput *input, uint32_t now_ms);

#endif
