#ifndef OPENTEC_BASE_DISPLAY_PROMPT_H
#define OPENTEC_BASE_DISPLAY_PROMPT_H

#include <stdbool.h>
#include <stdint.h>

#include "display/framebuffer.h"

/**
 * @brief Stores acknowledgement input state for a display prompt.
 *
 * The latch records whether input was observed so acknowledgement can be reported after release.
 */
typedef struct {
    bool input_seen; /**< Whether acknowledgement input has been observed while visible. */
} DisplayPrompt;

/**
 * @brief Renders the force-output acknowledgement prompt.
 *
 * Clears the framebuffer and, while visible, draws the official white-panel high-torque icon and
 * one-line inverted Font10 acknowledgement text.
 *
 * @param[in,out] framebuffer Framebuffer receiving the prompt.
 * @param[in] visible Whether the prompt should be drawn after clearing the framebuffer.
 */
void display_prompt_render(DisplayFramebuffer framebuffer, bool visible);

/**
 * @brief Renders the Torque Key acknowledgement prompt.
 *
 * Clears the framebuffer and, while visible, draws the official white-panel high-torque icon,
 * inverted Font10 safety message, and non-inverted acknowledgement label.
 *
 * @param[in,out] framebuffer Framebuffer receiving the prompt.
 * @param[in] visible Whether the prompt should be drawn after clearing the framebuffer.
 */
void display_prompt_render_torque_key(DisplayFramebuffer framebuffer, bool visible);

/**
 * @brief Renders the paddle bite-point prompt.
 *
 * Clears the framebuffer and centers the supplied percentage as a large value while visible.
 *
 * @param[in,out] framebuffer Framebuffer receiving the prompt.
 * @param[in] visible Whether the prompt should be drawn after clearing the framebuffer.
 * @param[in] percent Bite-point percentage to display.
 */
void display_prompt_render_bite_point(DisplayFramebuffer framebuffer, bool visible,
                                      uint8_t percent);

/**
 * @brief Updates acknowledgement input state.
 *
 * Latches active input and reports one acknowledgement after input is released while the prompt
 * remains visible; hiding the prompt clears the latch.
 *
 * @param[in,out] prompt Prompt input state to update.
 * @param[in] visible Whether the prompt remains visible.
 * @param[in] input_active Whether the caller-selected acknowledgement input is active.
 * @return True once after latched input is released while visible.
 */
bool display_prompt_update(DisplayPrompt *prompt, bool visible, bool input_active);

#endif
