#ifndef OPENTEC_BASE_USB_FEATURE_UPLOAD_H
#define OPENTEC_BASE_USB_FEATURE_UPLOAD_H

#include <stdbool.h>
#include <stdint.h>

/** @brief Segmented feature-upload packet size in bytes. */
enum {
    USB_FEATURE_UPLOAD_PACKET_SIZE = 64, /**< Feature-upload packet size. */
};

/** @brief Result of accepting one segmented feature-upload packet. */
typedef enum {
    USB_FEATURE_UPLOAD_INVALID,         /**< Packet is invalid or out of sequence. */
    USB_FEATURE_UPLOAD_WAITING,         /**< Packet advanced an incomplete upload. */
    USB_FEATURE_UPLOAD_ACKNOWLEDGEMENT, /**< Packet requires an acknowledgement. */
    USB_FEATURE_UPLOAD_COMPLETE,        /**< Terminal packet completed the upload. */
} UsbFeatureUploadResult;

/** @brief Data and status returned after accepting one feature-upload packet. */
typedef struct {
    UsbFeatureUploadResult result; /**< Packet-acceptance result. */
    const uint8_t *data;           /**< Completed assembled data, when available. */
    uint16_t length;               /**< Completed assembled data length. */
} UsbFeatureUploadEvent;

/** @brief Segmented feature-upload assembly buffer and progress state. */
typedef struct {
    uint8_t *data;         /**< Caller-owned assembly storage. */
    uint16_t capacity;     /**< Assembly storage capacity in bytes. */
    uint16_t total_length; /**< Declared upload length in bytes. */
    uint16_t offset;       /**< Assembled bytes currently stored. */
    uint8_t report_id;     /**< Accepted feature report identifier. */
    uint8_t sequence;      /**< Most recently received packet sequence. */
    bool active;           /**< True after the initial packet starts an upload. */
    bool complete;         /**< True after the terminal packet completes an upload. */
} UsbFeatureUpload;

/**
 * @brief Initializes a segmented USB feature upload.
 *
 * Attaches caller-owned assembly storage, selects the accepted report identifier, and resets upload
 * progress.
 *
 * @param[out] upload Upload state to initialize.
 * @param[in] report_id Accepted feature report identifier.
 * @param[in] data Caller-owned upload assembly storage.
 * @param[in] capacity Available assembly byte count.
 * @return True when upload state, storage, and capacity are usable; otherwise false.
 */
bool usb_feature_upload_init(UsbFeatureUpload *upload, uint8_t report_id, uint8_t *data,
                             uint16_t capacity);

/**
 * @brief Accepts one segmented USB feature-upload packet.
 *
 * Reads the initial total length, assembles continuation and final payloads, and reports when an
 * acknowledgement or completed upload is ready.
 *
 * @param[in,out] upload Active feature upload.
 * @param[in] packet Sixty-four-byte feature packet.
 * @param[in] length Received packet byte count.
 * @return Invalid input, continuation progress, acknowledgement request, or completed upload.
 */
UsbFeatureUploadEvent
usb_feature_upload_accept(UsbFeatureUpload *upload,
                          const uint8_t packet[USB_FEATURE_UPLOAD_PACKET_SIZE], uint8_t length);

#endif
