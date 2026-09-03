#ifndef OPENTEC_BASE_USB_XBOX_GIP_RESPONSE_H
#define OPENTEC_BASE_USB_XBOX_GIP_RESPONSE_H

#include <stdbool.h>
#include <stdint.h>

#include "usb/console_descriptor.h"

/** @brief Xbox GIP response sizes and input-count constants. */
enum {
    USB_XBOX_GIP_DIGEST_RESPONSE_SIZE = 32, /**< Number of bytes in a digest response. */
    USB_XBOX_GIP_READY_RESPONSE_SIZE = 8,   /**< Number of bytes in a ready response. */
    USB_XBOX_GIP_TRANSFER_STATUS_RESPONSE_SIZE =
        13, /**< Number of bytes in a transfer-status response. */
    USB_XBOX_GIP_EXTENDED_STATUS_RESPONSE_SIZE =
        17, /**< Number of bytes in an extended-status response. */
    USB_XBOX_GIP_CAPABILITY_RESPONSE_SIZE = 55, /**< Number of bytes in a capability response. */
    USB_XBOX_GIP_INPUT_RESPONSE_SIZE = 54,      /**< Number of bytes in an input-state response. */
    USB_XBOX_GIP_INPUT_PEDAL_COUNT = 3,         /**< Number of pedal axes in an input snapshot. */
    USB_XBOX_GIP_INPUT_SELECTOR_COUNT = 6, /**< Number of selector values in an input snapshot. */
    USB_XBOX_GIP_INPUT_AUXILIARY_COUNT =
        3, /**< Number of auxiliary button values in an input snapshot. */
};

/** @brief Logical wheel input values encoded into an Xbox GIP input response. */
typedef struct {
    uint8_t buttons[2];                              /**< Primary Xbox button bytes. */
    uint16_t steering;                               /**< Current steering axis value. */
    uint16_t pedals[USB_XBOX_GIP_INPUT_PEDAL_COUNT]; /**< Current pedal axis values. */
    uint8_t auxiliary_pedal;                         /**< Current auxiliary-pedal value. */
    uint8_t axis_mode;                               /**< Current axis mode. */
    uint8_t led_state;                               /**< Current wheel LED state. */
    uint16_t steering_range_units;                   /**< Steering range in protocol units. */
    uint8_t force_feedback_level; /**< Force-feedback level in the protocol's 0-to-255 scale. */
    bool pedal_active[USB_XBOX_GIP_INPUT_PEDAL_COUNT]; /**< Whether each pedal axis is active. */
    bool auxiliary_pedal_active; /**< Whether the auxiliary pedal axis is active. */
    uint8_t clutch_paddles[2];   /**< Current clutch-paddle values. */
    uint8_t selectors[USB_XBOX_GIP_INPUT_SELECTOR_COUNT]; /**< Rotary selector values. */
    uint8_t button_flags;                                 /**< Secondary button flags. */
    uint8_t packed_buttons; /**< Packed mode-specific button values. */
    uint8_t auxiliary_buttons[USB_XBOX_GIP_INPUT_AUXILIARY_COUNT]; /**< Auxiliary button values. */
    uint8_t extended_button; /**< Dedicated extended button value. */
} UsbXboxGipInputSnapshot;

/** @brief Logical attached-device state encoded into an Xbox GIP extended-status response. */
typedef struct {
    BoardVariant board_variant;       /**< Base hardware variant. */
    uint8_t wheel_mode;               /**< Attached-wheel protocol mode. */
    uint8_t pedal_connection_flags;   /**< Encoded pedal connection flags. */
    uint8_t auxiliary_axis_active;    /**< Auxiliary-axis activity value. */
    uint8_t axis_mode;                /**< Current axis mode. */
    uint8_t transfer_code;            /**< Current transfer status code. */
    uint8_t multi_position_mode;      /**< Current multi-position mode. */
    bool hardware_option;             /**< Whether the hardware option is present. */
    bool h_pattern_available;         /**< Whether an H-pattern shifter is available. */
    bool legacy_pedal_mode;           /**< Whether legacy pedal mode is active. */
    bool primary_pedal_calibration;   /**< Whether primary pedal calibration is available. */
    bool secondary_pedal_calibration; /**< Whether secondary pedal calibration is available. */
    bool pedal_handshake_active;      /**< Whether the official pedal handshake latch is set. */
    bool resistance_profile_active;   /**< Whether the resistance-profile effect limit is active. */
    bool wheel_calibration_available; /**< Whether wheel calibration is available. */
    bool wheel_axis_report_enabled;   /**< Whether the wheel axis report is enabled. */
    bool multi_position_supported;    /**< Whether multi-position mode is supported. */
    bool adapter_connected; /**< Whether the attached wheel is connected through an adapter. */
} UsbXboxGipExtendedStatus;

/**
 * @brief Takes the next Xbox GIP response sequence.
 *
 * Returns the current sequence and advances it; when the current value is 255, returns 1 and stores
 * 1 as the next value.
 *
 * @param[in,out] next_sequence Sequence state to consume and advance.
 * @return Sequence value for the current response.
 */
uint8_t usb_xbox_gip_sequence_take(uint8_t *next_sequence);

/**
 * @brief Encodes the Xbox GIP discovery digest response.
 *
 * Emits the 32-byte discovery identity, including the device digest, base and wheel mode code,
 * protocol version, status version, and feature versions.
 *
 * @param[in] variant DD1 or DD2 hardware variant.
 * @param[in] wheel_mode Attached-wheel mode used for mode and status versions.
 * @param[in] sequence Response sequence value.
 * @param[in] digest Eight-byte device digest.
 * @param[out] output Destination for the digest response.
 */
void usb_xbox_gip_digest_response_encode(BoardVariant variant, uint8_t wheel_mode, uint8_t sequence,
                                         const uint8_t digest[USB_XBOX_GIP_DIGEST_SIZE],
                                         uint8_t output[USB_XBOX_GIP_DIGEST_RESPONSE_SIZE]);

/**
 * @brief Encodes the Xbox GIP ready response.
 *
 * Emits the eight-byte response returned after accepted activation, pause, suspend, and reset
 * session commands.
 *
 * @param[in] sequence Response sequence value.
 * @param[out] output Destination for the ready response.
 */
void usb_xbox_gip_ready_response_encode(uint8_t sequence,
                                        uint8_t output[USB_XBOX_GIP_READY_RESPONSE_SIZE]);

/**
 * @brief Encodes the Xbox GIP transfer-status response.
 *
 * Echoes the first two request bytes and reports transfer status 2 with zero transferred and
 * remaining counts.
 *
 * @param[in] sequence Current response sequence value.
 * @param[in] request First two bytes of the triggering session request.
 * @param[out] output Destination for the transfer-status response.
 */
void usb_xbox_gip_transfer_status_response_encode(
    uint8_t sequence, const uint8_t request[2],
    uint8_t output[USB_XBOX_GIP_TRANSFER_STATUS_RESPONSE_SIZE]);

/**
 * @brief Encodes the Xbox GIP wheel capability response.
 *
 * Emits the 51-byte capability payload describing the wheel class, eight axes, fixed range
 * fields, four pedals, and capability flags. Reserved payload bytes are cleared.
 *
 * @param[in] sequence Response sequence value.
 * @param[out] output Destination for the capability response.
 */
void usb_xbox_gip_capability_response_encode(uint8_t sequence,
                                             uint8_t output[USB_XBOX_GIP_CAPABILITY_RESPONSE_SIZE]);

/**
 * @brief Encodes the Xbox GIP attached-device status response.
 *
 * Emits the wheel, pedal, shifter, resistance-profile, axis-report, accessory, multi-position,
 * adapter, and base-identity state in the 13-byte type-11 payload. The high bits of byte 13 and
 * byte 16 are preserved from the retained response workspace, matching the official encoder.
 *
 * @param[in] sequence Response sequence value.
 * @param[in] status Current logical attached-device status.
 * @param[in,out] output Retained response workspace and destination for the extended-status
 * response.
 */
void usb_xbox_gip_extended_status_response_encode(
    uint8_t sequence, const UsbXboxGipExtendedStatus *status,
    uint8_t output[USB_XBOX_GIP_EXTENDED_STATUS_RESPONSE_SIZE]);

/**
 * @brief Encodes the Xbox GIP input-state response.
 *
 * Emits the 50-byte controller payload with buttons, steering, pedals, wheel controls, active-axis
 * flags, auxiliary buttons, and the fixed extension marker. Reserved payload bytes are cleared.
 *
 * @param[in] sequence Response sequence value.
 * @param[in] snapshot Logical controller state to encode.
 * @param[out] output Destination for the input-state response.
 */
void usb_xbox_gip_input_response_encode(uint8_t sequence, const UsbXboxGipInputSnapshot *snapshot,
                                        uint8_t output[USB_XBOX_GIP_INPUT_RESPONSE_SIZE]);

#endif
