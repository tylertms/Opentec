#ifndef OPENTEC_BASE_USB_TRANSFER_RESPONSE_H
#define OPENTEC_BASE_USB_TRANSFER_RESPONSE_H

#include <stdbool.h>
#include <stdint.h>

#include "usb/device.h"

/** @brief Maximum logical payload bytes retained by the transfer response queue. */
enum {
    USB_TRANSFER_RESPONSE_PAYLOAD_CAPACITY = 124 /**< Maximum logical payload size, in bytes. */
};

/**
 * @brief Retains one pedal response while it is emitted as USB reports.
 *
 * Tracks payload progress separately from a prepared report so endpoint retries cannot skip or
 * duplicate committed fragments.
 */
typedef struct {
    uint8_t
        payload[USB_TRANSFER_RESPONSE_PAYLOAD_CAPACITY]; /**< Retained logical response bytes. */
    uint16_t length;         /**< Number of valid bytes in @ref payload. */
    uint16_t cursor;         /**< Number of payload bytes already committed. */
    uint8_t sequence;        /**< Sequence number for the next continuation or final report. */
    uint8_t prepared_length; /**< Payload bytes included in the prepared report. */
    bool prepared;           /**< True while @ref prepared_length awaits commit. */
} UsbTransferResponse;

/**
 * @brief Initializes a USB transfer response queue.
 *
 * Clears the retained payload, progress cursor, sequence, and prepared-report state.
 *
 * @param[out] response Response queue to initialize.
 */
void usb_transfer_response_init(UsbTransferResponse *response);

/**
 * @brief Reports whether the response queue accepts a new payload.
 *
 * A new payload is accepted only after the previous payload has completed and no report is waiting
 * for commit.
 *
 * @param[in] response Response queue to inspect.
 * @return True when the queue accepts a new payload; otherwise false.
 */
bool usb_transfer_response_accepting(const UsbTransferResponse *response);

/**
 * @brief Queues one payload for USB transfer responses.
 *
 * Copies a nonempty payload of at most 124 bytes and resets fragmentation progress for its first
 * report.
 *
 * @param[in,out] response Response queue receiving the payload.
 * @param[in] payload Source payload bytes.
 * @param[in] length Number of payload bytes to copy.
 * @return True when the payload was accepted; otherwise false.
 */
bool usb_transfer_response_queue(UsbTransferResponse *response, const uint8_t *payload,
                                 uint8_t length);

/**
 * @brief Reports whether a transfer response report is pending.
 *
 * Tests whether uncommitted payload bytes remain without changing queue state.
 *
 * @param[in] response Response queue to inspect.
 * @return True when another report can be prepared; otherwise false.
 */
bool usb_transfer_response_pending(const UsbTransferResponse *response);

/**
 * @brief Prepares the next padded USB transfer report.
 *
 * Encodes a single report for short payloads or a length-bearing first report followed by
 * sequence-bearing continuation and final reports. Repeated preparation before commit reproduces
 * the same report state.
 *
 * @param[in,out] response Response queue whose next report is prepared.
 * @param[out] output Destination for the complete USB report.
 * @return True when a report was prepared; otherwise false.
 */
bool usb_transfer_response_prepare(UsbTransferResponse *response,
                                   uint8_t output[USB_DEVICE_REPORT_SIZE]);

/**
 * @brief Commits the prepared USB transfer report.
 *
 * Advances payload and sequence progress only after the endpoint accepts the prepared report.
 * Completing the payload returns the queue to its accepting state.
 *
 * @param[in,out] response Response queue containing the prepared report.
 */
void usb_transfer_response_commit(UsbTransferResponse *response);

#endif
