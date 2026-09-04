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
 * @brief Identifies the presentation owned by the local OLED shifter page.
 */
typedef enum {
    SHIFTER_LOCAL_DISPLAY_NONE,        /**< The local shifter page is clear. */
    SHIFTER_LOCAL_DISPLAY_GEAR,        /**< A temporary ordinary-gear glyph is visible. */
    SHIFTER_LOCAL_DISPLAY_CALIBRATION, /**< Calibration instructions or position are visible. */
} ShifterLocalDisplayKind;

/**
 * @brief Stores the local OLED shifter presentation.
 *
 * Gear presentations use glyph. Calibration presentations use calibration_prompt and
 * calibration_position to select the official local prompt or diagnostic content.
 */
typedef struct {
    ShifterLocalDisplayKind kind; /**< Current local presentation kind. */
    uint8_t glyph;                /**< Raw seven-segment glyph for an ordinary gear presentation. */
    HPatternCalibrationPrompt calibration_prompt;     /**< Current calibration prompt. */
    HPatternCalibrationPosition calibration_position; /**< Position requested by calibration. */
} ShifterLocalDisplay;

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
    bool refresh_side_effect_pending; /**< True while the mode-specific refresh side effect awaits
                                         service. */
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
 * @brief Takes the mode-specific refresh side effect.
 *
 * Returns and clears the one-shot side effect created by a refresh request while leaving
 * refresh_requested available to the calibration start gate.
 *
 * @param[in,out] display Display state receiving the side-effect request.
 * @return True when a pending side effect was consumed.
 */
bool shifter_display_take_refresh_side_effect(ShifterDisplay *display);

/**
 * @brief Advances the local OLED shifter presentation.
 *
 * Tracks the official wait-for-connection, one-second gear presentation, and calibration prompt
 * states without touching attached-wheel display output.
 *
 * @param[in,out] display Persistent shifter display state.
 * @param[in] gear Current H-pattern gear.
 * @param[in] wheel_active True while the attached-wheel protocol is active.
 * @param[in] h_pattern_available True while at least one H-pattern input is present.
 * @param[in] calibration_prompt Current calibration presentation phase.
 * @param[in] calibration_position Next calibration position, or complete.
 * @param[in] now_ms Current monotonic time in milliseconds.
 * @param[out] output Local OLED presentation to update.
 * @return True when the local presentation changed.
 */
bool shifter_display_update_local(ShifterDisplay *display, ShifterGear gear, bool wheel_active,
                                  bool h_pattern_available,
                                  HPatternCalibrationPrompt calibration_prompt,
                                  HPatternCalibrationPosition calibration_position, uint32_t now_ms,
                                  ShifterLocalDisplay *output);

/**
 * @brief Advances H-pattern gear and calibration presentation.
 *
 * Updates the attached-wheel glyph output according to the official phase-first state machine.
 * Showing expiry and neutral clearing run before the connection gate. The first active waiting
 * sample records its gear and returns before rendering, while a monitoring sample can render
 * before connection loss returns the state to waiting. The corresponding official dispatch spans
 * 0x034C78-0x034DC2 and returns through 0x034DC8.
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
