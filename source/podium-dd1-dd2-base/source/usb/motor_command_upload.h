#ifndef OPENTEC_BASE_USB_MOTOR_COMMAND_UPLOAD_H
#define OPENTEC_BASE_USB_MOTOR_COMMAND_UPLOAD_H

#include <stdbool.h>
#include <stdint.h>

#include "motor/command_packet.h"
#include "usb/feature_upload.h"

/** @brief Report identifiers used by the motor-command upload protocol. */
enum {
    USB_MOTOR_COMMAND_REPORT_ID = 6, /**< Motor-command feature report identifier. */
    USB_MOTOR_COMMAND_SEGMENT_ACKNOWLEDGEMENT_REPORT_ID =
        0xfd, /**< Segmented-upload acknowledgement report identifier. */
    USB_MOTOR_COMMAND_COMPACT_ACKNOWLEDGEMENT_REPORT_ID =
        0xfe, /**< Compact-upload acknowledgement report identifier. */
};

/** @brief Motor-command upload storage limits. */
enum {
    USB_MOTOR_COMMAND_UPLOAD_WRAPPER_SIZE = 9, /**< Bytes surrounding the command payload. */
    USB_MOTOR_COMMAND_UPLOAD_ASSEMBLY_SIZE =
        MOTOR_COMMAND_PACKET_MAX_PACKET_SIZE, /**< Storage for one maximum feature upload. */
};

/** @brief Result of accepting a motor-command upload packet. */
typedef enum {
    USB_MOTOR_COMMAND_UPLOAD_INVALID,         /**< Packet is invalid or not accepted. */
    USB_MOTOR_COMMAND_UPLOAD_WAITING,         /**< A segmented upload packet was accepted without an
                                                 acknowledgement. */
    USB_MOTOR_COMMAND_UPLOAD_ACKNOWLEDGEMENT, /**< An upload acknowledgement should be sent. */
    USB_MOTOR_COMMAND_UPLOAD_COMMAND,         /**< A complete motor command payload is available. */
    USB_MOTOR_COMMAND_UPLOAD_RESTART,         /**< The host requested a motor-channel restart. */
    USB_MOTOR_COMMAND_UPLOAD_RELEASE, /**< The host requested release of the motor channel. */
} UsbMotorCommandUploadResult;

/** @brief Result and derived data from one motor-command upload packet. */
typedef struct {
    UsbMotorCommandUploadResult result; /**< Packet-acceptance result. */
    const uint8_t
        *payload; /**< Command payload in packet or assembly storage when result is COMMAND. */
    uint16_t payload_length; /**< Number of bytes in payload. */
    uint8_t sequence; /**< Sequence value copied from a packet with a valid report ID and length. */
    uint8_t acknowledgement_report_id; /**< Report identifier for the required acknowledgement, if
                                          any. */
    bool segmented; /**< True when a complete command resides in the segmented assembly buffer. */
} UsbMotorCommandUploadEvent;

/** @brief State for assembling motor-command upload packets. */
typedef struct {
    UsbFeatureUpload feature; /**< Shared segmented feature-upload state. */
} UsbMotorCommandUpload;

/**
 * @brief Initializes motor-command upload state.
 *
 * Attaches caller-owned segmented assembly storage and configures the upload to accept feature
 * report USB_MOTOR_COMMAND_REPORT_ID.
 *
 * @param[out] upload Upload state to initialize.
 * @param[out] assembly Caller-owned storage for a segmented command.
 * @param[in] assembly_capacity Capacity of assembly in bytes; use
 * USB_MOTOR_COMMAND_UPLOAD_ASSEMBLY_SIZE for the maximum feature upload.
 * @return True when upload and assembly are non-null and the capacity is nonzero; otherwise false.
 */
bool usb_motor_command_upload_init(UsbMotorCommandUpload *upload, uint8_t *assembly,
                                   uint16_t assembly_capacity);

/**
 * @brief Resets completed or in-progress motor-command upload state.
 *
 * Keeps the attached assembly storage and report identifier while discarding upload progress.
 * A completed segmented command keeps its bytes in the attached storage, but callers must consume
 * or copy those bytes before accepting another segmented upload.
 *
 * @param[in,out] upload Upload state to reset.
 */
void usb_motor_command_upload_reset(UsbMotorCommandUpload *upload);

/**
 * @brief Accepts one motor-command feature packet.
 *
 * Decodes compact controls and commands, forwards segmented packets to the shared upload state, and
 * reports upload progress, acknowledgement requests, or a completed command payload. A completed
 * segmented command resets upload progress while leaving its payload in assembly storage for the
 * consumer.
 *
 * @param[in,out] upload Active motor-command upload state.
 * @param[in] packet Received packet, including its report identifier.
 * @param[in] length Number of received packet bytes; it must equal USB_FEATURE_UPLOAD_PACKET_SIZE.
 * @return Event describing the accepted packet, with INVALID for rejected input.
 */
UsbMotorCommandUploadEvent
usb_motor_command_upload_accept(UsbMotorCommandUpload *upload,
                                const uint8_t packet[USB_FEATURE_UPLOAD_PACKET_SIZE],
                                uint8_t length);

#endif
