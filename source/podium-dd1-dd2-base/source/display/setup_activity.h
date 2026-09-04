#ifndef OPENTEC_BASE_DISPLAY_SETUP_ACTIVITY_H
#define OPENTEC_BASE_DISPLAY_SETUP_ACTIVITY_H

#include <stdbool.h>
#include <stdint.h>

/**
 * @brief Stored phases of the setup-activity display sequence.
 *
 * The values are the raw phase byte used by the reference service at 0x04a558. Phases one through
 * four select the four setup-activity records, and phase five waits before returning to phase one.
 */
enum {
    DISPLAY_SETUP_ACTIVITY_IDLE = 0,
    DISPLAY_SETUP_ACTIVITY_FIRST = 1,
    DISPLAY_SETUP_ACTIVITY_LAST = 4,
    DISPLAY_SETUP_ACTIVITY_RESTART = 5,
};

/**
 * @brief State for the local setup-activity display sequence.
 *
 * The state owns the reference phase byte, 32-bit deadline, selected text phase, and display
 * revision. A zero-initialized value is idle.
 */
typedef struct {
    uint32_t deadline_ms; /**< Strict deadline for the current setup-activity phase. */
    uint8_t phase;        /**< Current reference phase byte. */
    uint8_t text_phase;   /**< Setup-activity text phase selected for rendering. */
    uint8_t revision;     /**< Incremented when a new setup-activity record is selected. */
} DisplaySetupActivity;

/**
 * @brief Initializes setup-activity state.
 *
 * @param[out] activity Setup-activity state to clear; null is ignored.
 */
void display_setup_activity_init(DisplaySetupActivity *activity);

/**
 * @brief Advances the setup-activity display state.
 *
 * The reference service at 0x04a558 reads the extended-status/startup latch at 0x6318 and treats
 * the USB operating-mode word at 0x40d2 as active whenever it is not Fanatec mode. Its deadline
 * branches at 0x04a584-0x04a588 and 0x04a5d6-0x04a5da advance only when unsigned now_ms is
 * strictly greater than deadline_ms. Phase five also waits for that strict expiration before
 * returning to phase one.
 *
 * @param[in,out] activity Setup-activity state to advance; null returns false.
 * @param[in] pedal_handshake_active Raw 0x6318 latch exposed by the pedal service.
 * @param[in] non_fanatec_mode True when raw 0x40d2 is not USB_OPERATING_MODE_FANATEC.
 * @param[in] now_ms Current unsigned time in milliseconds.
 * @return True while either raw setup-activity input is active.
 */
bool display_setup_activity_update(DisplaySetupActivity *activity, bool pedal_handshake_active,
                                   bool non_fanatec_mode, uint32_t now_ms);

#endif
