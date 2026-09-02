#ifndef OPENTEC_BASE_USB_XBOX_GIP_COMMAND_H
#define OPENTEC_BASE_USB_XBOX_GIP_COMMAND_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/** @brief Operation selected by a decoded Xbox GIP command packet. */
typedef enum {
    USB_XBOX_GIP_COMMAND_CAPABILITIES,            /**< Request wheel capability information. */
    USB_XBOX_GIP_COMMAND_STEERING_RANGE,          /**< Request or set the steering range. */
    USB_XBOX_GIP_COMMAND_FORCE_FEEDBACK_STRENGTH, /**< Request or set force-feedback strength. */
    USB_XBOX_GIP_COMMAND_REPORT_STATE,            /**< Update Xbox report-streaming state. */
    USB_XBOX_GIP_COMMAND_TRANSFER_STATUS,         /**< Request transfer status. */
    USB_XBOX_GIP_COMMAND_SCRIPT_SAMPLES,          /**< Request script sample data. */
    USB_XBOX_GIP_COMMAND_SCRIPT_SLOT,             /**< Request script slot data. */
    USB_XBOX_GIP_COMMAND_SCRIPT_STATUS,           /**< Request script status data. */
    USB_XBOX_GIP_COMMAND_SCRIPT_VALUES,           /**< Request script values data. */
    USB_XBOX_GIP_COMMAND_SCRIPT_AXES,             /**< Request script axes data. */
    USB_XBOX_GIP_COMMAND_EXTENDED_STATUS,         /**< Request extended wheel status. */
} UsbXboxGipCommandKind;

/** @brief Decoded Xbox GIP command operation and parameter. */
typedef struct {
    UsbXboxGipCommandKind kind; /**< Decoded command operation. */
    uint16_t parameter;         /**< Decoded command parameter; interpretation depends on #kind. */
} UsbXboxGipCommand;

/**
 * @brief Decodes an Xbox GIP application command.
 *
 * Accepts command packet 0x0A with supported control, script, and extended-status selectors and
 * the supported group-0x20 report-state and transfer-status selectors. Script sample selectors use
 * the complete little-endian parameter; the script-slot selector uses only its low parameter byte.
 *
 * @param[in] packet Received Xbox GIP endpoint packet.
 * @param[in] length Number of bytes available in @p packet.
 * @param[out] command Decoded command operation and parameter.
 * @return `true` when the packet contains a supported application command; otherwise `false`.
 */
bool usb_xbox_gip_command_decode(const uint8_t *packet, size_t length, UsbXboxGipCommand *command);

/**
 * @brief Normalizes an Xbox steering-range request.
 *
 * Interprets the request in tenths of a degree, clamps the effective range to 90 through 1080
 * degrees, and rounds intermediate values down to the nearest ten-degree step.
 *
 * @param[in] requested_degrees Host-requested lock-to-lock steering range in tenths of a degree.
 * @return Effective steering range in degrees.
 */
uint16_t usb_xbox_gip_steering_range_normalize(uint16_t requested_degrees);

/**
 * @brief Normalizes an Xbox force-feedback strength request.
 *
 * Converts the host's unsigned-byte scale to the truncated zero-through-100 percentage used by
 * the controller state.
 *
 * @param[in] requested_level Host-requested force-feedback level from 0 through 255.
 * @return Effective whole-number percentage from 0 through 100.
 */
uint8_t usb_xbox_gip_force_feedback_strength_normalize(uint8_t requested_level);

#endif
