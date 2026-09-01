#ifndef OPENTEC_BASE_WHEEL_CAPABILITY_H
#define OPENTEC_BASE_WHEEL_CAPABILITY_H

#include <stdbool.h>
#include <stdint.h>

#include "profile/tuning.h"
#include "usb/operating_mode_command.h"

/**
 * @brief Retained attached-wheel capability and reporting state.
 *
 * The state combines the latest report capability bytes with wheel-mode defaults and host-selected
 * multi-position behavior.
 */
typedef struct {
    uint16_t report_flags;     /**< Report flags derived from the latest capability response. */
    uint16_t capability_flags; /**< Latest report mode and capability byte packed together. */
    uint8_t multi_position_override; /**< Host override, or 0xff for automatic selection. */
    bool calibration_available; /**< True when calibration is available for the selected mode. */
    bool tuning_menu_available; /**< True when the tuning menu capability bit is set. */
    bool input_available; /**< True after any active input path has been observed since reset. */
} WheelCapabilityState;

/**
 * @brief Initializes attached-wheel capability state.
 *
 * Clears report and availability state and selects automatic multi-position reporting.
 *
 * @param[out] state Attached-wheel capability state to initialize.
 */
void wheel_capability_init(WheelCapabilityState *state);

/**
 * @brief Updates attached-wheel report capability state.
 *
 * Caches the report mode and capability byte and maps capability bits two through five into report
 * flag bits one through four without changing calibration or tuning availability.
 *
 * @param[in,out] state Persistent attached-wheel capability state.
 * @param[in] report_mode Attached-wheel report mode byte.
 * @param[in] report_capabilities Attached-wheel status and capability byte.
 */
void wheel_capability_update_report(WheelCapabilityState *state, uint8_t report_mode,
                                    uint8_t report_capabilities);

/**
 * @brief Updates shared attached-wheel capability state.
 *
 * Refreshes report capability data and applies wheel-mode defaults for calibration and tuning
 * availability.
 *
 * @param[in,out] state Persistent attached-wheel capability state.
 * @param[in] wheel_mode Selected attached-wheel mode.
 * @param[in] report_mode Attached-wheel report mode byte.
 * @param[in] report_capabilities Attached-wheel status and capability byte.
 */
void wheel_capability_update(WheelCapabilityState *state, uint8_t wheel_mode, uint8_t report_mode,
                             uint8_t report_capabilities);

/**
 * @brief Reports the effective attached-wheel input capability.
 *
 * Exposes the retained input latch only for wheel modes that publish it through the primary input
 * report.
 *
 * @param[in] state Persistent attached-wheel capability state.
 * @param[in] wheel_mode Negotiated attached-wheel mode.
 * @return true when state is nonnull, its input latch is set, and the mode exposes that latch;
 * false otherwise.
 */
bool wheel_capability_input_available(const WheelCapabilityState *state, uint8_t wheel_mode);

/**
 * @brief Reports whether the attached wheel exposes the tuning menu.
 *
 * Returns true for inherently supported wheel modes and otherwise consults the retained capability
 * state only for modes ten, nineteen, twenty, and twenty-one.
 *
 * @param[in] state Persistent attached-wheel capability state.
 * @param[in] wheel_mode Negotiated attached-wheel mode.
 * @return true for inherently supported modes, or for modes ten, nineteen, twenty, and twenty-one
 * when state is nonnull and its flag is set; false otherwise.
 */
bool wheel_capability_tuning_menu_available(const WheelCapabilityState *state, uint8_t wheel_mode);

/**
 * @brief Applies a multi-position reporting override command.
 *
 * Accepts device-control selector 0x16 and stores the requested mode; the automatic tuning value
 * is represented internally by the automatic override sentinel.
 *
 * @param[in,out] state Attached-wheel capability state.
 * @param[in] command Decoded F8 09 operating-mode command.
 * @return true when command selects the multi-position override and both pointers are nonnull;
 * false otherwise.
 */
bool wheel_capability_apply_multi_position_command(WheelCapabilityState *state,
                                                   const UsbOperatingModeCommand *command);

/**
 * @brief Tests whether a wheel mode supplies multi-position input.
 *
 * Modes four, six, twelve, and twenty-one depend on input_active; modes nine, ten, eleven,
 * fifteen, twenty-three, twenty-seven, twenty-eight, and twenty-nine are always supported.
 *
 * @param[in] wheel_mode Negotiated attached-wheel mode.
 * @param[in] input_active true when the attached-wheel input transport is active.
 * @return true when multi-position input is available for the mode and activity state; false
 * otherwise.
 */
bool wheel_capability_multi_position_supported(uint8_t wheel_mode, bool input_active);

/**
 * @brief Resolves the effective multi-position reporting mode.
 *
 * Returns encoder mode when state is null, input is unavailable, or the configured selector is
 * unsupported; otherwise applies an explicit profile mode, host override, or automatic wheel-mode
 * selection.
 *
 * @param[in] state Attached-wheel capability and override state.
 * @param[in] configured_mode Multi-position mode from the active tuning profile.
 * @param[in] wheel_mode Negotiated attached-wheel mode.
 * @param[in] input_active true when the attached-wheel input transport is active.
 * @return Effective multi-position reporting-mode value.
 */
uint8_t wheel_capability_multi_position_mode(const WheelCapabilityState *state,
                                             TuningMultiPositionMode configured_mode,
                                             uint8_t wheel_mode, bool input_active);

#endif
