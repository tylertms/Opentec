#include "usb/xbox_gip_response.h"

#include <stdint.h>
#include <string.h>

#include "usb/console_descriptor.h"

/** @brief Private wire values used by Xbox GIP response encoders. */
enum {
    XBOX_GIP_DIGEST_RESPONSE = 2,                    /**< Digest response type. */
    XBOX_GIP_TRANSFER_STATUS_RESPONSE = 1,           /**< Transfer-status response type. */
    XBOX_GIP_CAPABILITY_RESPONSE = 0x21,             /**< Capability response type. */
    XBOX_GIP_READY_RESPONSE = 3,                     /**< Ready response type. */
    XBOX_GIP_INPUT_RESPONSE = 0x20,                  /**< Input response type. */
    XBOX_GIP_SEQUENCE_SUBCOMMAND = 0x20,             /**< Sequence subcommand value. */
    XBOX_GIP_DIGEST_PAYLOAD_SIZE = 0x1c,             /**< Digest payload size. */
    XBOX_GIP_READY_PAYLOAD_SIZE = 4,                 /**< Ready payload size. */
    XBOX_GIP_TRANSFER_STATUS_PAYLOAD_SIZE = 9,       /**< Transfer-status payload size. */
    XBOX_GIP_TRANSFER_STATUS_ERROR = 2,              /**< Transfer-status error value. */
    XBOX_GIP_CAPABILITY_PAYLOAD_SIZE = 0x33,         /**< Capability payload size. */
    XBOX_GIP_CAPABILITY_CLASS = 0x10,                /**< Capability class value. */
    XBOX_GIP_CAPABILITY_AXIS_COUNT = 8,              /**< Number of capability axes. */
    XBOX_GIP_CAPABILITY_RANGE_UPPER = 0x5a,          /**< Capability range upper byte. */
    XBOX_GIP_CAPABILITY_RANGE_LOWER = 0x38,          /**< Capability range lower byte. */
    XBOX_GIP_CAPABILITY_PEDAL_COUNT = 4,             /**< Number of capability pedals. */
    XBOX_GIP_CAPABILITY_FLAGS = 0x48,                /**< Capability flags value. */
    XBOX_GIP_EXTENDED_STATUS_RESPONSE = 0x11,        /**< Extended-status response type. */
    XBOX_GIP_EXTENDED_STATUS_PAYLOAD_SIZE = 0x0d,    /**< Extended-status payload size. */
    XBOX_GIP_EXTENDED_STATUS_READY = 1 << 0,         /**< H-pattern-ready status bit. */
    XBOX_GIP_EXTENDED_STATUS_LEGACY_PEDALS = 1 << 1, /**< Legacy-pedal status bit. */
    XBOX_GIP_EXTENDED_STATUS_AUXILIARY_CALIBRATION = 1
                                                     << 2, /**< Auxiliary-calibration status bit. */
    XBOX_GIP_EXTENDED_STATUS_PEDAL_RECOVERY = 1 << 3,      /**< Pedal-recovery status bit. */
    XBOX_GIP_EXTENDED_STATUS_THERMAL_LIMIT = 1 << 4,       /**< Thermal-limit status bit. */
    XBOX_GIP_EXTENDED_STATUS_PEDAL_CALIBRATION = 1
                                                 << 5, /**< Primary pedal-calibration status bit. */
    XBOX_GIP_EXTENDED_STATUS_WHEEL_CALIBRATION = 1 << 6, /**< Wheel-calibration status bit. */
    XBOX_GIP_EXTENDED_STATUS_WHEEL_INPUT = 1 << 7,       /**< Wheel-input status bit. */
    XBOX_GIP_EXTENDED_STATUS_SECONDARY_PEDAL_CALIBRATION =
        1 << 4, /**< Protocol value for secondary pedal calibration. */
    XBOX_GIP_EXTENDED_STATUS_PROFILE_VERSION = 3,  /**< Extended-status profile version. */
    XBOX_GIP_EXTENDED_STATUS_PROTOCOL_VERSION = 9, /**< Extended-status protocol version. */
    XBOX_GIP_EXTENDED_STATUS_WHEEL_MODE =
        0x1d,                           /**< Wheel mode selecting extended-status version 5. */
    XBOX_GIP_INPUT_PAYLOAD_SIZE = 0x32, /**< Input payload size. */
    XBOX_GIP_INPUT_AXIS_MODE = 0x66,    /**< Encoded active axis mode value. */
    XBOX_GIP_INPUT_AXIS_FLAGS = 0x08,   /**< Base input-axis flags. */
    XBOX_GIP_INPUT_AUXILIARY_AXIS_FLAG = 1 << 4, /**< Auxiliary-axis active bit. */
    XBOX_GIP_INPUT_THIRD_PEDAL_FLAG = 1 << 5,    /**< Third-pedal active bit. */
    XBOX_GIP_INPUT_SECOND_PEDAL_FLAG = 1 << 6,   /**< Second-pedal active bit. */
    XBOX_GIP_INPUT_FIRST_PEDAL_FLAG = 1 << 7,    /**< First-pedal active bit. */
};

uint8_t usb_xbox_gip_sequence_take(uint8_t *next_sequence) {
    uint8_t sequence = *next_sequence;
    if (sequence == UINT8_MAX) {
        *next_sequence = 1;
        return 1;
    }

    *next_sequence = sequence + 1;
    return sequence;
}

void usb_xbox_gip_digest_response_encode(BoardVariant variant, uint8_t wheel_mode, uint8_t sequence,
                                         const uint8_t digest[USB_XBOX_GIP_DIGEST_SIZE],
                                         uint8_t output[USB_XBOX_GIP_DIGEST_RESPONSE_SIZE]) {
    memset(output, 0, USB_XBOX_GIP_DIGEST_RESPONSE_SIZE);
    output[0] = XBOX_GIP_DIGEST_RESPONSE;
    output[1] = XBOX_GIP_SEQUENCE_SUBCOMMAND;
    output[2] = sequence;
    output[3] = XBOX_GIP_DIGEST_PAYLOAD_SIZE;
    memcpy(&output[4], digest, USB_XBOX_GIP_DIGEST_SIZE);
    output[12] = 0xb7;
    output[13] = 0x0e;

    uint8_t mode_code = usb_xbox_gip_mode_code(variant, wheel_mode);
    if (mode_code != 0) {
        output[14] = mode_code;
        output[15] = 0x0f;
    }

    /** @brief Protocol-version bytes included in a digest response. */
    static const uint8_t protocol_version[] = {3, 0, 9, 0, 0, 0, 1, 0};
    memcpy(&output[16], protocol_version, sizeof(protocol_version));
    output[24] = wheel_mode == XBOX_GIP_EXTENDED_STATUS_WHEEL_MODE ? 5 : 4;
    output[26] = 1;
    output[28] = 1;
    output[30] = 1;
}

void usb_xbox_gip_ready_response_encode(uint8_t sequence,
                                        uint8_t output[USB_XBOX_GIP_READY_RESPONSE_SIZE]) {
    memset(output, 0, USB_XBOX_GIP_READY_RESPONSE_SIZE);
    output[0] = XBOX_GIP_READY_RESPONSE;
    output[1] = XBOX_GIP_SEQUENCE_SUBCOMMAND;
    output[2] = sequence;
    output[3] = XBOX_GIP_READY_PAYLOAD_SIZE;
    output[4] = 0x80;
    output[5] = 1;
}

void usb_xbox_gip_transfer_status_response_encode(
    uint8_t sequence, const uint8_t request[2],
    uint8_t output[USB_XBOX_GIP_TRANSFER_STATUS_RESPONSE_SIZE]) {
    memset(output, 0, USB_XBOX_GIP_TRANSFER_STATUS_RESPONSE_SIZE);
    output[0] = XBOX_GIP_TRANSFER_STATUS_RESPONSE;
    output[1] = XBOX_GIP_SEQUENCE_SUBCOMMAND;
    output[2] = sequence;
    output[3] = XBOX_GIP_TRANSFER_STATUS_PAYLOAD_SIZE;
    output[4] = XBOX_GIP_TRANSFER_STATUS_ERROR;
    output[5] = request[0];
    output[6] = request[1];
}

void usb_xbox_gip_capability_response_encode(
    uint8_t sequence, uint8_t output[USB_XBOX_GIP_CAPABILITY_RESPONSE_SIZE]) {
    memset(output, 0, USB_XBOX_GIP_CAPABILITY_RESPONSE_SIZE);
    output[0] = XBOX_GIP_CAPABILITY_RESPONSE;
    output[2] = sequence;
    output[3] = XBOX_GIP_CAPABILITY_PAYLOAD_SIZE;
    output[4] = XBOX_GIP_CAPABILITY_CLASS;
    output[5] = XBOX_GIP_CAPABILITY_CLASS;
    output[6] = XBOX_GIP_CAPABILITY_CLASS;
    output[7] = XBOX_GIP_CAPABILITY_CLASS;
    output[8] = XBOX_GIP_CAPABILITY_AXIS_COUNT;
    output[9] = XBOX_GIP_CAPABILITY_RANGE_UPPER;
    output[11] = XBOX_GIP_CAPABILITY_RANGE_LOWER;
    output[12] = XBOX_GIP_CAPABILITY_PEDAL_COUNT;
    output[13] = 1;
    output[14] = XBOX_GIP_CAPABILITY_FLAGS;
}

void usb_xbox_gip_extended_status_response_encode(
    uint8_t sequence, const UsbXboxGipExtendedStatus *status,
    uint8_t output[USB_XBOX_GIP_EXTENDED_STATUS_RESPONSE_SIZE]) {
    memset(output, 0, USB_XBOX_GIP_EXTENDED_STATUS_RESPONSE_SIZE);
    output[0] = XBOX_GIP_EXTENDED_STATUS_RESPONSE;
    output[2] = sequence;
    output[3] = XBOX_GIP_EXTENDED_STATUS_PAYLOAD_SIZE;
    output[4] =
        (status->h_pattern_available ? XBOX_GIP_EXTENDED_STATUS_READY : 0) |
        (status->legacy_pedal_mode ? XBOX_GIP_EXTENDED_STATUS_LEGACY_PEDALS : 0) |
        ((status->legacy_pedal_mode || status->secondary_pedal_calibration) &&
                 (status->pedal_connection_flags & 0xaau) == 0
             ? XBOX_GIP_EXTENDED_STATUS_AUXILIARY_CALIBRATION
             : 0) |
        (status->pedal_recovery_handshake ? XBOX_GIP_EXTENDED_STATUS_PEDAL_RECOVERY : 0) |
        (status->thermal_effect_limit ? XBOX_GIP_EXTENDED_STATUS_THERMAL_LIMIT : 0) |
        (status->primary_pedal_calibration ? XBOX_GIP_EXTENDED_STATUS_PEDAL_CALIBRATION : 0) |
        (status->wheel_calibration_available ? XBOX_GIP_EXTENDED_STATUS_WHEEL_CALIBRATION : 0) |
        (status->wheel_input_capability_available ? XBOX_GIP_EXTENDED_STATUS_WHEEL_INPUT : 0);
    output[5] = status->wheel_mode;
    output[6] =
        status->secondary_pedal_calibration
            ? XBOX_GIP_EXTENDED_STATUS_SECONDARY_PEDAL_CALIBRATION
            : (status->legacy_pedal_mode ? 1u : 0u) | (status->primary_pedal_calibration ? 2u : 0u);
    output[7] = status->auxiliary_axis_active;
    output[8] = status->axis_mode;
    output[9] = XBOX_GIP_EXTENDED_STATUS_PROFILE_VERSION;
    output[10] = XBOX_GIP_EXTENDED_STATUS_PROTOCOL_VERSION;
    output[11] = status->transfer_code;
    if (status->multi_position_supported) {
        /** @brief Protocol values corresponding to supported multi-position modes. */
        static const uint8_t variants[] = {1, 3, 2, 0};
        output[12] = status->multi_position_mode < sizeof(variants)
                         ? variants[status->multi_position_mode]
                         : 4;
    } else {
        output[12] = 4;
    }
    output[13] = status->adapter_connected ? 1 : 0;
    output[14] = status->board_variant == BOARD_VARIANT_DD2 ? 8 : status->hardware_option ? 7 : 6;
}

void usb_xbox_gip_input_response_encode(uint8_t sequence, const UsbXboxGipInputSnapshot *snapshot,
                                        uint8_t output[USB_XBOX_GIP_INPUT_RESPONSE_SIZE]) {
    memset(output, 0, USB_XBOX_GIP_INPUT_RESPONSE_SIZE);
    output[0] = XBOX_GIP_INPUT_RESPONSE;
    output[2] = sequence;
    output[3] = XBOX_GIP_INPUT_PAYLOAD_SIZE;
    memcpy(&output[4], snapshot->buttons, sizeof(snapshot->buttons));
    output[6] = (uint8_t)snapshot->steering;
    output[7] = (uint8_t)(snapshot->steering >> 8);
    for (uint8_t axis = 0; axis < USB_XBOX_GIP_INPUT_PEDAL_COUNT; axis++) {
        output[8 + axis * 2] = (uint8_t)snapshot->pedals[axis];
        output[9 + axis * 2] = (uint8_t)(snapshot->pedals[axis] >> 8);
    }
    output[14] = snapshot->auxiliary_pedal;
    output[15] = snapshot->axis_mode == 1 ? XBOX_GIP_INPUT_AXIS_MODE : 0;
    output[16] = snapshot->led_state;
    uint16_t steering_range_tenths = snapshot->steering_range_degrees * 10u;
    output[17] = (uint8_t)steering_range_tenths;
    output[18] = (uint8_t)(steering_range_tenths >> 8);
    output[19] = snapshot->force_feedback_level;
    output[20] = XBOX_GIP_INPUT_AXIS_FLAGS |
                 (snapshot->auxiliary_pedal_active ? XBOX_GIP_INPUT_AUXILIARY_AXIS_FLAG : 0) |
                 (snapshot->pedal_active[2] ? XBOX_GIP_INPUT_THIRD_PEDAL_FLAG : 0) |
                 (snapshot->pedal_active[1] ? XBOX_GIP_INPUT_SECOND_PEDAL_FLAG : 0) |
                 (snapshot->pedal_active[0] ? XBOX_GIP_INPUT_FIRST_PEDAL_FLAG : 0);
    memcpy(&output[21], snapshot->clutch_paddles, sizeof(snapshot->clutch_paddles));
    memcpy(&output[23], snapshot->selectors, sizeof(snapshot->selectors));
    output[29] = snapshot->button_flags;
    output[30] = snapshot->packed_buttons;
    memcpy(&output[33], snapshot->auxiliary_buttons, sizeof(snapshot->auxiliary_buttons));
    output[36] = UINT8_MAX;
    output[37] = snapshot->extended_button;
}
