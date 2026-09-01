#ifndef OPENTEC_BASE_WHEEL_STARTUP_DISPLAY_H
#define OPENTEC_BASE_WHEEL_STARTUP_DISPLAY_H

#include <stdbool.h>
#include <stdint.h>

#include "motor/identity.h"
#include "wheel/display_output.h"

/** @brief Phases of the attached-wheel startup display sequence. */
typedef enum {
    WHEEL_STARTUP_DISPLAY_DASHES,        /**< Show startup dashes before the base version. */
    WHEEL_STARTUP_DISPLAY_BASE_VERSION,  /**< Show the base firmware version. */
    WHEEL_STARTUP_DISPLAY_MOTOR_VERSION, /**< Show the managed motor-controller version. */
    WHEEL_STARTUP_DISPLAY_READY_DELAY, /**< Hold startup dashes before declaring the wheel ready. */
    WHEEL_STARTUP_DISPLAY_CALIBRATION, /**< Show the calibration label while position input is
                                          unavailable. */
    WHEEL_STARTUP_DISPLAY_CALIBRATION_PAUSE, /**< Leave the display blank between calibration-label
                                                presentations. */
    WHEEL_STARTUP_DISPLAY_COMPLETE,          /**< Startup presentation is complete. */
} WheelStartupDisplayPhase;

/** @brief State for the attached-wheel startup display sequence. */
typedef struct {
    WheelStartupDisplayPhase phase; /**< Current startup presentation phase. */
    uint32_t deadline_ms;           /**< Monotonic deadline for the current startup phase. */
    uint32_t version_presentation_close_ms; /**< Monotonic close time for the tuning-display version
                                               presentation. */
    bool ready;                             /**< True after the startup presentation completes. */
    bool version_presentation_pending; /**< True while a tuning-display version request is pending.
                                        */
    bool version_presentation_close_armed;   /**< True while waiting for the version presentation
                                                close time. */
    bool version_presentation_close_pending; /**< True after the version presentation reaches its
                                                close time. */
} WheelStartupDisplay;

/**
 * @brief Initializes the startup display sequence.
 *
 * Starts the sequence at its initial dash presentation and clears all deadlines and event latches.
 *
 * @param[out] display Startup display state to initialize.
 */
void wheel_startup_display_init(WheelStartupDisplay *display);

/**
 * @brief Advances the startup display sequence.
 *
 * Updates the startup glyphs for the current time and reports whether the attached-wheel display
 * output changed. The sequence does not advance while the wheel is inactive or after completion.
 *
 * @param[in,out] display Startup display state to update.
 * @param[in] wheel_active True while the attached-wheel protocol is active.
 * @param[in] tuning_display_supported True when the wheel provides a separate tuning display.
 * @param[in] position_ready True when a valid motor-position report is available.
 * @param[in] motor_identity Motor-controller identity, or null when unavailable.
 * @param[in] now_ms Current monotonic time in milliseconds.
 * @param[in,out] output Attached-wheel display output to update.
 * @return True when the display glyphs or marker changed; otherwise false.
 */
bool wheel_startup_display_update(WheelStartupDisplay *display, bool wheel_active,
                                  bool tuning_display_supported, bool position_ready,
                                  const MotorIdentity *motor_identity, uint32_t now_ms,
                                  WheelDisplayOutput *output);

/**
 * @brief Reports startup readiness.
 *
 * Reads the retained readiness latch without advancing the display sequence.
 *
 * @param[in] display Startup display state to inspect.
 * @return True after startup completes; otherwise false.
 */
bool wheel_startup_display_ready(const WheelStartupDisplay *display);

/**
 * @brief Takes a pending tuning-display version presentation request.
 *
 * Consumes the one-shot request raised when startup enters the tuning-display version interval.
 *
 * @param[in,out] display Startup display state holding the request.
 * @return True when a request was pending; otherwise false.
 */
bool wheel_startup_display_take_version_presentation(WheelStartupDisplay *display);

/**
 * @brief Takes a pending tuning-display version presentation close request.
 *
 * Consumes the one-shot request raised after the tuning-display version interval elapses.
 *
 * @param[in,out] display Startup display state holding the request.
 * @return True when a close request was pending; otherwise false.
 */
bool wheel_startup_display_take_version_presentation_close(WheelStartupDisplay *display);

#endif
