#ifndef OPENTEC_BASE_SECURITY_CODE_H
#define OPENTEC_BASE_SECURITY_CODE_H

#include <stdbool.h>
#include <stdint.h>

/** @brief Number of decimal digits in a security code. */
enum { SECURITY_CODE_DIGIT_COUNT = 3 /**< Decimal digits in each security code. */ };

/** @brief Retained security-code digits and activation state. */
typedef struct {
    uint8_t digits[SECURITY_CODE_DIGIT_COUNT]; /**< Decimal code digits. */
    bool enabled; /**< True when security-code protection is enabled. */
} SecurityCodeSettings;

/** @brief Attached-wheel and adapter controls for security-code interaction. */
typedef struct {
    uint8_t wheel_mode;         /**< Attached-wheel mode identifier. */
    uint16_t primary_buttons;   /**< Primary attached-wheel button bits. */
    uint16_t secondary_buttons; /**< Secondary attached-wheel button bits. */
    uint8_t adapter_buttons[3]; /**< Adapter button bytes. */
    bool adapter_connected;     /**< True when an adapter is connected. */
} SecurityCodeInput;

/** @brief Current phase of security-code interaction. */
typedef enum {
    SECURITY_CODE_INACTIVE, /**< No security-code interaction is active. */
    SECURITY_CODE_PREPARE,  /**< Activation or deactivation prompt is preparing. */
    SECURITY_CODE_WAIT,     /**< Prompt delay is in progress. */
    SECURITY_CODE_EDIT,     /**< Code digits are being edited. */
    SECURITY_CODE_CONFIRM,  /**< Entered digits await confirmation. */
} SecurityCodePhase;

/** @brief Presentation operation returned by security-code updates. */
typedef enum {
    SECURITY_CODE_PRESENTATION_KEEP,   /**< Keep the current presentation. */
    SECURITY_CODE_PRESENTATION_PROMPT, /**< Show the enable or disable prompt. */
    SECURITY_CODE_PRESENTATION_DIGITS, /**< Show entered digits. */
    SECURITY_CODE_PRESENTATION_CLEAR,  /**< Clear the security-code presentation. */
} SecurityCodePresentationKind;

/** @brief Security-code display glyphs and independent wheel-report ownership. */
typedef struct {
    SecurityCodePresentationKind kind;         /**< Presentation operation. */
    uint8_t glyphs[SECURITY_CODE_DIGIT_COUNT]; /**< Seven-segment glyphs for the digits. */
    uint16_t display_report;  /**< Two-byte wheel report with mask low and digit position high. */
    bool uses_local_display;  /**< Whether this presentation writes local wheel glyphs. */
    bool uses_display_report; /**< Whether this presentation owns the wheel display report. */
} SecurityCodePresentation;

/** @brief Result of one security-code interaction update. */
typedef struct {
    SecurityCodePresentation presentation; /**< Display presentation operation. */
    uint8_t prompt_command;                /**< Optional native prompt command. */
    bool active;                           /**< True while security-code processing owns input. */
    bool settings_changed;                 /**< True when retained settings changed. */
    bool mismatch;                         /**< True when a disable code did not match. */
} SecurityCodeUpdate;

/** @brief Mutable state for one security-code interaction. */
typedef struct {
    uint8_t entered_digits[SECURITY_CODE_DIGIT_COUNT]; /**< Digits entered in the current
                                                          interaction. */
    uint32_t input_deadline_ms;                        /**< Next input-repeat or entry deadline. */
    uint32_t blink_deadline_ms;                        /**< Next selected-digit blink deadline. */
    SecurityCodePhase phase;                           /**< Current interaction phase. */
    uint8_t selected_digit;                            /**< Zero-based digit currently selected. */
    uint8_t blink_phase;                               /**< Current blink phase. */
    bool enable_requested; /**< True when the interaction requests enabling. */
} SecurityCode;

/**
 * @brief Initializes security-code interaction state.
 *
 * Clears entered digits, timers, flags, and returns the interaction to the inactive phase.
 *
 * @param[out] code Interaction state to initialize.
 */
void security_code_init(SecurityCode *code);

/**
 * @brief Reports whether security-code interaction owns local controls.
 *
 * Treats every non-inactive phase as active, independent of whether the retained security code is
 * currently enabled.
 *
 * @param[in] code Interaction state to inspect.
 * @return True while an interaction phase is active; false for inactive or null state.
 */
bool security_code_interaction_active(const SecurityCode *code);

/**
 * @brief Advances security-code interaction.
 *
 * Detects activation chords, edits decimal digits, confirms enable or disable requests, and
 * returns the presentation and state changes produced by this sample.
 *
 * @param[in,out] code Interaction state to advance.
 * @param[in,out] settings Retained security-code settings.
 * @param[in] input Current attached-wheel and adapter input.
 * @param[in] now_ms Current monotonic time in milliseconds.
 * @return Current presentation and state result; all fields are zero when inputs are invalid.
 */
SecurityCodeUpdate security_code_update(SecurityCode *code, SecurityCodeSettings *settings,
                                        const SecurityCodeInput *input, uint32_t now_ms);

#endif
