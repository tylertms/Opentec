#include "wheel/capability.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "profile/tuning.h"
#include "usb/operating_mode_command.h"

/**
 * @brief Operating-mode values used by attached-wheel capability handling.
 *
 * The selector identifies the host multi-position override, while the sentinel and wheel mode
 * select automatic behavior.
 */
enum {
    DEVICE_CONTROL_OPCODE = 1,      /**< Operating-mode opcode for device-control commands. */
    MULTI_POSITION_SELECTOR = 0x16, /**< Device-control selector for multi-position reporting. */
    MULTI_POSITION_AUTOMATIC_OVERRIDE =
        UINT8_MAX, /**< Sentinel indicating automatic host selection. */
    MULTI_POSITION_AUTOMATIC_WHEEL_MODE =
        9, /**< Wheel mode whose automatic selection uses pulse mode. */
};

/**
 * @brief Tests whether an attached-wheel mode supplies multi-position input.
 *
 * Treats modes four, six, twelve, and twenty-one as dependent on current input activity. Modes
 * nine, ten, eleven, fifteen, twenty-three, twenty-seven, twenty-eight, and twenty-nine always
 * expose the input path.
 *
 * @param[in] wheel_mode Negotiated attached-wheel mode.
 * @param[in] input_active True when the attached-wheel input transport is active.
 * @return True when multi-position input is available.
 */
bool wheel_capability_multi_position_supported(uint8_t wheel_mode, bool input_active) {
    switch (wheel_mode) {
    case 4:
    case 6:
    case 12:
    case 21:
        return input_active;
    case 9:
    case 10:
    case 11:
    case 15:
    case 23:
    case 27:
    case 28:
    case 29:
        return true;
    default:
        return false;
    }
}

/**
 * @brief Tests whether calibration is inherent to a wheel mode.
 *
 * Matches the wheel modes that expose calibration without a capability flag.
 *
 * @param[in] wheel_mode Attached-wheel mode.
 * @return True when calibration must be available.
 */
static bool calibration_forced_available(uint8_t wheel_mode) {
    return wheel_mode == 5 || wheel_mode == 7 || wheel_mode == 8 || wheel_mode == 0x10 ||
           wheel_mode == 0x12;
}

/**
 * @brief Tests whether calibration is excluded by a wheel mode.
 *
 * Matches the wheel modes that suppress calibration regardless of their capability flag.
 *
 * @param[in] wheel_mode Attached-wheel mode.
 * @return True when calibration must be unavailable.
 */
static bool calibration_forced_unavailable(uint8_t wheel_mode) {
    return wheel_mode == 9 || wheel_mode == 0x0b || wheel_mode == 0x11 || wheel_mode == 0x15 ||
           wheel_mode == 0x16 || wheel_mode == 0x1d;
}

/**
 * @brief Initializes attached-wheel capability state.
 *
 * Clears report, feature, and input capabilities and selects automatic multi-position reporting.
 *
 * @param[out] state Attached-wheel capability state to initialize.
 */
void wheel_capability_init(WheelCapabilityState *state) {
    *state = (WheelCapabilityState){.multi_position_override = MULTI_POSITION_AUTOMATIC_OVERRIDE};
}

/**
 * @brief Updates attached-wheel report capability state.
 *
 * Caches the report mode and capability byte and maps capability bits 2 through 5 into report flag
 * bits 1 through 4 without changing calibration or tuning availability.
 *
 * @param[in,out] state Persistent attached-wheel capability state.
 * @param[in] report_mode Attached-wheel report mode byte.
 * @param[in] report_capabilities Attached-wheel status and capability byte.
 */
void wheel_capability_update_report(WheelCapabilityState *state, uint8_t report_mode,
                                    uint8_t report_capabilities) {
    state->capability_flags = (uint16_t)report_mode | (uint16_t)report_capabilities << 8;
    state->report_flags = (state->report_flags & 0xffe1u) | ((report_capabilities & 0x3cu) >> 1);
}

/**
 * @brief Updates shared attached-wheel capability state.
 *
 * Caches the report mode and capability byte, maps capability bits 2 through 5 into report flag
 * bits 1 through 4, and applies the wheel-mode defaults for calibration and tuning availability.
 *
 * @param[in,out] state Persistent attached-wheel capability state.
 * @param[in] wheel_mode Selected attached-wheel mode.
 * @param[in] report_mode Attached-wheel report mode byte.
 * @param[in] report_capabilities Attached-wheel status and capability byte.
 */
void wheel_capability_update(WheelCapabilityState *state, uint8_t wheel_mode, uint8_t report_mode,
                             uint8_t report_capabilities) {
    wheel_capability_update_report(state, report_mode, report_capabilities);
    if (calibration_forced_available(wheel_mode)) {
        state->calibration_available = true;
    } else if (calibration_forced_unavailable(wheel_mode)) {
        state->calibration_available = false;
    } else {
        state->calibration_available = (report_capabilities & 1u) != 0;
    }
    state->tuning_menu_available = (report_capabilities & 2u) != 0;
}

/**
 * @brief Reports the effective attached-wheel input capability.
 *
 * Exposes the retained input-capability latch only for wheel modes that publish it through the
 * primary input report.
 *
 * @param[in] state Persistent attached-wheel capability state.
 * @param[in] wheel_mode Negotiated attached-wheel mode.
 * @return true when state is nonnull, its input latch is set, and the mode exposes that latch;
 * false otherwise.
 */
bool wheel_capability_input_available(const WheelCapabilityState *state, uint8_t wheel_mode) {
    if (state == NULL || !state->input_available) {
        return false;
    }

    switch (wheel_mode) {
    case 4:
    case 6:
    case 12:
    case 14:
    case 15:
    case 19:
    case 20:
    case 21:
    case 22:
    case 23:
    case 28:
        return true;
    default:
        return false;
    }
}

/**
 * @brief Reports whether the attached wheel exposes the tuning menu.
 *
 * Treats wheel modes 6, 7, 9, 11, 18, and 29 as inherently available. Modes 10, 19, 20, and 21
 * use the retained capability state, while every other mode suppresses the menu.
 *
 * @param[in] state Persistent attached-wheel capability state.
 * @param[in] wheel_mode Negotiated attached-wheel mode.
 * @return true for inherently supported modes, or for modes ten, nineteen, twenty, and twenty-one
 * when state is nonnull and its flag is set; false otherwise.
 */
bool wheel_capability_tuning_menu_available(const WheelCapabilityState *state, uint8_t wheel_mode) {
    switch (wheel_mode) {
    case 6:
    case 7:
    case 9:
    case 11:
    case 18:
    case 29:
        return true;
    case 10:
    case 19:
    case 20:
    case 21:
        return state != NULL && state->tuning_menu_available;
    default:
        return false;
    }
}

/**
 * @brief Applies a multi-position reporting override command.
 *
 * Accepts device-control selector 0x16. Value three restores automatic selection; every other
 * byte becomes the explicit reporting-mode override.
 *
 * @param[in,out] state Attached-wheel capability state.
 * @param[in] command Decoded F8 09 operating-mode command.
 * @return true when both pointers are nonnull and the command selects the multi-position override;
 * false otherwise.
 */
bool wheel_capability_apply_multi_position_command(WheelCapabilityState *state,
                                                   const UsbOperatingModeCommand *command) {
    if (state == NULL || command == NULL || command->opcode != DEVICE_CONTROL_OPCODE ||
        command->parameters[0] != MULTI_POSITION_SELECTOR) {
        return false;
    }

    uint8_t requested = command->parameters[1];
    state->multi_position_override = requested == TUNING_MULTI_POSITION_AUTOMATIC
                                         ? MULTI_POSITION_AUTOMATIC_OVERRIDE
                                         : requested;
    return true;
}

/**
 * @brief Resolves the effective multi-position reporting mode.
 *
 * Disables the feature when the attached-wheel mode has no input path. Explicit profile modes
 * zero through two take precedence. Automatic profiles use the host override, or select pulse
 * mode for wheel mode nine and encoder mode for every other wheel mode.
 *
 * @param[in] state Attached-wheel capability and override state.
 * @param[in] configured_mode Multi-position mode from the active tuning profile.
 * @param[in] wheel_mode Negotiated attached-wheel mode.
 * @param[in] input_active True when the attached-wheel input transport is active.
 * @return Effective reporting-mode value, or encoder mode when state is null, input is unavailable,
 * or the configured selector is unsupported.
 */
uint8_t wheel_capability_multi_position_mode(const WheelCapabilityState *state,
                                             TuningMultiPositionMode configured_mode,
                                             uint8_t wheel_mode, bool input_active) {
    if (state == NULL || !wheel_capability_multi_position_supported(wheel_mode, input_active)) {
        return TUNING_MULTI_POSITION_ENCODER;
    }
    if (configured_mode <= TUNING_MULTI_POSITION_CONSTANT) {
        return (uint8_t)configured_mode;
    }
    if (configured_mode != TUNING_MULTI_POSITION_AUTOMATIC) {
        return TUNING_MULTI_POSITION_ENCODER;
    }
    if (state->multi_position_override != MULTI_POSITION_AUTOMATIC_OVERRIDE) {
        return state->multi_position_override;
    }
    return wheel_mode == MULTI_POSITION_AUTOMATIC_WHEEL_MODE ? TUNING_MULTI_POSITION_PULSE
                                                             : TUNING_MULTI_POSITION_ENCODER;
}
