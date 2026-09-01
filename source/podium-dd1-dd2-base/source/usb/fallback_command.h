#ifndef OPENTEC_BASE_USB_FALLBACK_COMMAND_H
#define OPENTEC_BASE_USB_FALLBACK_COMMAND_H

#include <stdbool.h>
#include <stdint.h>

#include "usb/output_command.h"

/** @brief Supported short fallback command operations. */
typedef enum {
    USB_FALLBACK_STEERING_RANGE_LOW,      /**< Lower steering-range setting. */
    USB_FALLBACK_STEERING_RANGE_HIGH,     /**< Upper steering-range setting. */
    USB_FALLBACK_DISPLAY_FLAGS,           /**< Display-flags setting. */
    USB_FALLBACK_SENSITIVITY,             /**< Steering sensitivity setting. */
    USB_FALLBACK_FORCE_FEEDBACK_STRENGTH, /**< Force-feedback strength setting. */
    USB_FALLBACK_FORCE_SCALE,             /**< Force-scale selector. */
    USB_FALLBACK_NATURAL_DAMPER,          /**< Natural damper setting. */
    USB_FALLBACK_NATURAL_FRICTION,        /**< Natural friction setting. */
    USB_FALLBACK_NATURAL_INERTIA,         /**< Natural inertia setting. */
    USB_FALLBACK_INTERPOLATION,           /**< Interpolation setting. */
    USB_FALLBACK_FORCE_EFFECT_INTENSITY,  /**< Force-effect intensity setting. */
    USB_FALLBACK_FORCE_EFFECT_STRENGTH,   /**< Force-effect strength setting. */
    USB_FALLBACK_SPRING_EFFECT_STRENGTH,  /**< Spring-effect strength setting. */
    USB_FALLBACK_DAMPER_EFFECT_STRENGTH,  /**< Damper-effect strength setting. */
    USB_FALLBACK_VIBRATION_STRENGTH,      /**< Vibration strength setting. */
    USB_FALLBACK_STEERING_LIMIT,          /**< Steering-limit setting. */
    USB_FALLBACK_COOLING_OVERRIDE,        /**< Cooling override setting. */
    USB_FALLBACK_SECURITY_DISABLE,        /**< Security-disable request. */
} UsbFallbackCommandKind;

/** @brief Decoded short fallback command and its raw parameters. */
typedef struct {
    UsbFallbackCommandKind kind; /**< Classified fallback operation. */
    uint16_t value;              /**< Little-endian value from command bytes two and three. */
    uint8_t parameters[4];       /**< Raw command parameters from bytes two through five. */
} UsbFallbackCommand;

/**
 * @brief Decodes one official short fallback command.
 *
 * Accepts the direct F8 tuning and steering commands, the F9 cooling override, and the guarded F9
 * security-disable request. F8 09 remains owned by the operating-mode decoder.
 *
 * @param[in] output Classified seven-byte short output report.
 * @param[out] command Destination for the command kind and parameters.
 * @return True when the report is a supported fallback command; otherwise false.
 */
bool usb_fallback_command_decode(const UsbOutputCommand *output, UsbFallbackCommand *command);

#endif
