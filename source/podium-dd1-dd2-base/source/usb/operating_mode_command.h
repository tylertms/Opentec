#ifndef OPENTEC_BASE_USB_OPERATING_MODE_COMMAND_H
#define OPENTEC_BASE_USB_OPERATING_MODE_COMMAND_H

#include <stdbool.h>
#include <stdint.h>

#include "usb/output_command.h"

/** @brief Number of parameter bytes in a decoded operating-mode command. */
enum {
    USB_OPERATING_MODE_PARAMETER_COUNT = 4 /**< Decoded parameter count. */
};

/** @brief Decoded operating-mode opcode and parameter bytes. */
typedef struct {
    uint8_t opcode;                                         /**< Operating-mode command opcode. */
    uint8_t parameters[USB_OPERATING_MODE_PARAMETER_COUNT]; /**< Opcode-specific parameter bytes. */
} UsbOperatingModeCommand;

/**
 * @brief Runtime services selected by operating-mode commands.
 *
 * Identifies the normal application, auxiliary paths, bridge paths, recovery paths, and reset
 * transition without exposing their internal service layouts.
 */
typedef enum {
    USB_RUNTIME_MODE_NORMAL = 0,             /**< Normal application runtime. */
    USB_RUNTIME_MODE_AUXILIARY = 1,          /**< Auxiliary interface runtime. */
    USB_RUNTIME_MODE_AUXILIARY_RECOVERY = 2, /**< Auxiliary recovery runtime. */
    USB_RUNTIME_MODE_STATUS_BRIDGE = 3,      /**< Status bridge runtime. */
    USB_RUNTIME_MODE_USB_BRIDGE = 4,         /**< USB bridge runtime. */
    USB_RUNTIME_MODE_PROTOCOL_BRIDGE = 5,    /**< Protocol bridge runtime. */
    USB_RUNTIME_MODE_PROTOCOL_RECOVERY = 6,  /**< Protocol recovery runtime. */
    USB_RUNTIME_MODE_RESET = 7,              /**< Reset into the bootloader. */
} UsbRuntimeMode;

/**
 * @brief Accepted runtime transition and its required persistence action.
 *
 * Carries the selected runtime service and whether settings must be stored before entering it.
 */
typedef struct {
    UsbRuntimeMode mode; /**< Runtime mode selected by the command. */
    bool save_settings;  /**< True when settings must be saved before the transition. */
} UsbRuntimeModeTransition;

/**
 * @brief Decodes an operating-mode command envelope.
 *
 * Accepts a classified seven-byte short output command with family F8 and format 09, then copies
 * its opcode and four parameter bytes into command.
 *
 * @param[in] output Classified USB short output command.
 * @param[out] command Destination for the decoded opcode and parameters.
 * @return True when output and command are valid and the command envelope is accepted; otherwise
 * false.
 */
bool usb_operating_mode_command_decode(const UsbOutputCommand *output,
                                       UsbOperatingModeCommand *command);

/**
 * @brief Tests for the native-mode reset request.
 *
 * Matches opcode 1 with first parameter 1. The caller performs the resulting input-mode change and
 * any related service transition.
 *
 * @param[in] command Decoded operating-mode command.
 * @return True when command is non-null and requests a native-mode reset; otherwise false.
 */
bool usb_operating_mode_command_requests_native_reset(const UsbOperatingModeCommand *command);

/**
 * @brief Tests whether a decoded command is an official no-op.
 *
 * Type two is accepted by the official dispatcher but exits without changing runtime state.
 *
 * @param[in] command Decoded operating-mode command.
 * @return True when the command is the official type-two no-op.
 */
bool usb_operating_mode_command_is_noop(const UsbOperatingModeCommand *command);

/**
 * @brief Tests for an accepted LED-pattern request.
 *
 * Accepts opcode 0x10 only when its first parameter is zero, or accepts opcode 0x11 without that
 * condition. Both forms carry the pattern in their second parameter.
 *
 * @param[in] command Decoded operating-mode command.
 * @return True when command is non-null and carries an accepted LED-pattern request; otherwise
 * false.
 */
bool usb_operating_mode_command_requests_led_pattern(const UsbOperatingModeCommand *command);

/**
 * @brief Decodes an allowed runtime-mode transition.
 *
 * Accepts reset independently of interface mode. Other runtime requests require interface mode zero
 * or one; auxiliary targets require auxiliary state two or three, and USB bridge targets require a
 * supported wheel mode.
 *
 * @param[in] command Decoded operating-mode command.
 * @param[in] interface_mode Active host interface mode.
 * @param[in] wheel_mode Active attached-wheel mode.
 * @param[in] auxiliary_state Active auxiliary interface state.
 * @param[out] transition Destination for the selected runtime mode and save action.
 * @return True when the command requests an allowed transition and transition is populated;
 * otherwise false.
 */
bool usb_operating_mode_command_decode_runtime(const UsbOperatingModeCommand *command,
                                               uint8_t interface_mode, uint8_t wheel_mode,
                                               uint8_t auxiliary_state,
                                               UsbRuntimeModeTransition *transition);

#endif
