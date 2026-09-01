#ifndef OPENTEC_BASE_WHEEL_DISPLAY_OVERLAY_H
#define OPENTEC_BASE_WHEEL_DISPLAY_OVERLAY_H

#include <stdbool.h>
#include <stdint.h>

#include "wheel/display_output.h"

/** @brief Presentation phases for a temporary attached-wheel display overlay. */
typedef enum {
    WHEEL_DISPLAY_OVERLAY_IDLE,       /**< No overlay owns the display. */
    WHEEL_DISPLAY_OVERLAY_HOLD_LABEL, /**< The initial hold-command label is visible. */
    WHEEL_DISPLAY_OVERLAY_COUNTDOWN,  /**< The hold-command countdown is visible. */
    WHEEL_DISPLAY_OVERLAY_COMMAND,    /**< A short command label is visible. */
} WheelDisplayOverlayPhase;

/** @brief Temporary command presentation layered over the normal wheel display. */
typedef struct {
    WheelDisplayOutput output;      /**< Current overlay glyph output. */
    uint32_t hold_until_ms;         /**< Deadline for the initial hold label. */
    uint32_t deadline_ms;           /**< Deadline at which the overlay is released. */
    uint8_t command;                /**< Command that started the presentation. */
    uint8_t remaining_seconds;      /**< Last rendered hold-command countdown. */
    WheelDisplayOverlayPhase phase; /**< Current presentation phase. */
    bool active;                    /**< Whether the overlay currently owns the display. */
} WheelDisplayOverlay;

/**
 * @brief Initializes a temporary display overlay.
 *
 * Clears the output, timing, command, phase, and ownership state.
 *
 * @param[out] overlay Overlay state to initialize.
 */
void wheel_display_overlay_init(WheelDisplayOverlay *overlay);

/**
 * @brief Starts or replaces a temporary display command presentation.
 *
 * Hold command 0x80 uses the fifteen-second hold presentation; other commands use the two-second
 * command presentation. A new command replaces any active presentation.
 *
 * @param[out] overlay Overlay state to start or replace.
 * @param[in] command Command selecting the presentation.
 * @param[in] now_ms Current monotonic time in milliseconds.
 */
void wheel_display_overlay_begin(WheelDisplayOverlay *overlay, uint8_t command, uint32_t now_ms);

/**
 * @brief Advances an active temporary display presentation.
 *
 * Updates the hold countdown after its label interval and clears the overlay at its deadline.
 *
 * @param[in,out] overlay Overlay state to update.
 * @param[in] now_ms Current monotonic time in milliseconds.
 * @return True when the output, phase, or ownership state changed; otherwise false.
 */
bool wheel_display_overlay_update(WheelDisplayOverlay *overlay, uint32_t now_ms);

#endif
