#ifndef OPENTEC_BASE_USB_TRANSFER_REQUEST_H
#define OPENTEC_BASE_USB_TRANSFER_REQUEST_H

#include <stdbool.h>
#include <stdint.h>

#include "usb/vendor_command.h"

/** @brief Maximum logical payload bytes retained by the transfer reassembler. */
enum {
    USB_TRANSFER_REQUEST_PAYLOAD_CAPACITY = 124 /**< Maximum logical payload size, in bytes. */
};

/**
 * @brief Stores one complete logical request received through USB transfer reports.
 *
 * Holds the logical payload copied from accepted transfer forms and records its valid length for
 * forwarding to the attached-wheel transfer service.
 */
typedef struct {
    uint8_t data[USB_TRANSFER_REQUEST_PAYLOAD_CAPACITY]; /**< Reassembled logical request bytes. */
    uint8_t length;                                      /**< Number of valid bytes in @ref data. */
} UsbTransferRequestPayload;

/**
 * @brief Reassembles tuning-menu transfer request reports.
 *
 * Supports single, first, and continuation/final command forms while retaining an incomplete
 * request across reports and exposing it only after the declared logical length is complete. A
 * first fragment contributes at most 60 payload bytes and each continuation or final fragment
 * contributes at most 61 payload bytes.
 */
typedef struct {
    UsbTransferRequestPayload payload; /**< Reassembled payload and its valid length. */
    uint8_t cursor;                    /**< Number of payload bytes already copied. */
    bool active;                       /**< True while a segmented request is being reassembled. */
    bool ready;                        /**< True while @ref payload contains a completed request. */
} UsbTransferRequest;

/**
 * @brief Initializes a USB transfer request reassembler.
 *
 * Clears the retained payload, progress cursor, and active or ready state.
 *
 * @param[out] request Reassembler state to initialize.
 */
void usb_transfer_request_init(UsbTransferRequest *request);

/**
 * @brief Applies one USB transfer request report.
 *
 * Accepts single, first, and continuation/final forms from the tuning-menu or direct transfer
 * command routes, retaining complete payloads and progressing segmented payloads toward their
 * declared length.
 *
 * @param[in,out] request Reassembler state to update.
 * @param[in] command Decoded command containing one transfer fragment.
 * @return True when the command uses a supported transfer form; otherwise false.
 */
bool usb_transfer_request_apply(UsbTransferRequest *request, const UsbVendorCommand *command);

/**
 * @brief Returns the completed logical transfer request.
 *
 * Provides a stable read-only view until the caller releases the completed request.
 *
 * @param[in] request Reassembler state to inspect.
 * @return Completed payload, or null when no request is ready.
 */
const UsbTransferRequestPayload *usb_transfer_request_payload(const UsbTransferRequest *request);

/**
 * @brief Releases the completed logical transfer request.
 *
 * Clears the completed payload length and ready latch without changing segmented progress fields.
 * Callers release a request after consuming its completed payload.
 *
 * @param[in,out] request Reassembler state to release.
 */
void usb_transfer_request_release(UsbTransferRequest *request);

#endif
