#ifndef OPENTEC_BASE_SHIFTER_DISPLAY_H
#define OPENTEC_BASE_SHIFTER_DISPLAY_H

#include <stdbool.h>
#include <stdint.h>

#include "shifter/calibration.h"
#include "shifter/h_pattern.h"
#include "wheel/display_output.h"

/**
 * @brief Identifies the H-pattern display update phase.
 *
 * The phase tracks connection initialization, passive monitoring, and a timed gear presentation.
 */
typedef enum {
    SHIFTER_DISPLAY_WAITING, /**< The attached-wheel display is disconnected or not yet monitoring.
                              */
    SHIFTER_DISPLAY_MONITORING, /**< The display is connected without a timed gear glyph. */
    SHIFTER_DISPLAY_SHOWING,    /**< A gear glyph is currently being held on the display. */
} ShifterDisplayPhase;

/**
 * @brief Retains H-pattern display presentation state.
 *
 * Calibration presentation and ordinary gear presentation share this state machine.
 */
typedef struct {
    ShifterDisplayPhase phase; /**< Current display update phase. */
    ShifterGear last_gear;     /**< Last gear observed by the display service. */
    uint32_t clear_after_ms;   /**< Deadline at which the current glyph may be cleared. */
    bool calibration_visible;  /**< True while calibration owns the display glyphs. */
    bool refresh_requested; /**< True when the next available display update must refresh the gear.
                             */
} ShifterDisplay;

/**
 * @brief Initializes H-pattern display state.
 *
 * Clears the display phase, retained gear, deadlines, and pending requests.
 *
 * @param[out] display Display state to initialize.
 */
void shifter_display_init(ShifterDisplay *display);

/**
 * @brief Requests a gear-display refresh.
 *
 * Retains the request until an active, idle attached-wheel display can present the current gear.
 *
 * @param[in,out] display Display state receiving the refresh request.
 */
void shifter_display_request_refresh(ShifterDisplay *display);

/**
 * @brief Advances H-pattern gear and calibration presentation.
 *
 * Updates the attached-wheel glyph output according to connection, calibration, refresh, and
 * timed gear-display state.
 *
 * @param[in,out] display Persistent display state to update.
 * @param[in] gear Current H-pattern gear.
 * @param[in] wheel_active True when the attached-wheel display is connected.
 * @param[in] calibration_prompt Current calibration presentation phase.
 * @param[in] calibration_position Next calibration position, or complete.
 * @param[in] now_ms Current monotonic time in milliseconds.
 * @param[in,out] output Attached-wheel display output to update.
 * @return True when output glyphs changed.
 */
bool shifter_display_update(ShifterDisplay *display, ShifterGear gear, bool wheel_active,
                            HPatternCalibrationPrompt calibration_prompt,
                            HPatternCalibrationPosition calibration_position, uint32_t now_ms,
                            WheelDisplayOutput *output);

#endif
