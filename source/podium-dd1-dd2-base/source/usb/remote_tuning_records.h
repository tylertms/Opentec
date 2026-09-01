#ifndef OPENTEC_BASE_USB_REMOTE_TUNING_RECORDS_H
#define OPENTEC_BASE_USB_REMOTE_TUNING_RECORDS_H

#include <stdbool.h>
#include <stdint.h>

#include "remote_tuning/response.h"
#include "remote_tuning/telemetry.h"
#include "usb/vendor_command.h"

/** @brief Limits for retained remote-tuning records and forwarded batches. */
enum {
    USB_REMOTE_TUNING_RECORD_COUNT =
        32, /**< Maximum number of records retained in arrival order. */
    USB_REMOTE_TUNING_RECORD_PAYLOAD_SIZE =
        15, /**< Maximum payload bytes retained in one logical record. */
    USB_REMOTE_TUNING_FORWARD_BATCH_SIZE =
        61, /**< Maximum serialized bytes emitted in one generic forwarding batch. */
};

/**
 * @brief One logical remote-tuning control record.
 *
 * Stores the decoded five-byte record header and its unpadded payload.
 */
typedef struct {
    uint8_t type;           /**< Remote-tuning route or record type. */
    uint8_t selector;       /**< Selector and selector-bank bits from the record header. */
    uint16_t value;         /**< Little-endian 16-bit value from the record header. */
    uint8_t payload_length; /**< Number of valid bytes in @ref payload. */
    uint8_t payload[USB_REMOTE_TUNING_RECORD_PAYLOAD_SIZE]; /**< Record payload, zero-filled beyond
                                                               @ref payload_length. */
} UsbRemoteTuningRecord;

/**
 * @brief Retained remote-tuning control records.
 *
 * Records are retained in arrival order until they are routed to a consumer or the fixed store is
 * full.
 */
typedef struct {
    UsbRemoteTuningRecord
        records[USB_REMOTE_TUNING_RECORD_COUNT]; /**< Retained records in arrival order. */
    uint8_t count;                               /**< Number of valid entries in @ref records. */
} UsbRemoteTuningRecords;

/**
 * @brief Initializes a remote-tuning record store.
 *
 * Clears every retained record and resets the valid-record count to zero.
 *
 * @param[out] records Record store to initialize.
 */
void usb_remote_tuning_records_init(UsbRemoteTuningRecords *records);

/**
 * @brief Applies one remote-tuning record packet.
 *
 * Parses packet types one and three, retaining each complete record in arrival order until the
 * fixed store is full. Malformed or incomplete trailing records stop parsing without rejecting the
 * packet type.
 *
 * @param[in,out] records Record store receiving decoded records.
 * @param[in] command Decoded vendor command containing the record packet.
 * @return True when the command is a remote-tuning record packet; otherwise false.
 */
bool usb_remote_tuning_records_apply(UsbRemoteTuningRecords *records,
                                     const UsbVendorCommand *command);

/**
 * @brief Takes the next attached-wheel record response.
 *
 * Serializes records for the requested wheel link into a bounded response and removes only the
 * records emitted by that response. Extended mode drains the standard selector bank before the
 * alternate bank.
 *
 * @param[in,out] records Record store containing retained records.
 * @param[in] link Attached-wheel response link to serve.
 * @param[out] response Destination for the encoded response.
 * @return True when at least one record was emitted; otherwise false.
 */
bool usb_remote_tuning_records_take_response(UsbRemoteTuningRecords *records, RemoteTuningLink link,
                                             RemoteTuningResponse *response);

/**
 * @brief Takes the next generic attached-device command batch.
 *
 * Serializes route-three records from both selector banks into the fixed 61-byte forwarding area,
 * taking matching records from newest to oldest while retaining the order of records left behind.
 *
 * @param[in,out] records Record store containing retained records.
 * @param[out] output Destination for serialized records.
 * @param[out] length Number of serialized bytes written.
 * @return True when at least one record was emitted; otherwise false.
 */
bool usb_remote_tuning_records_take_forward_batch(
    UsbRemoteTuningRecords *records, uint8_t output[USB_REMOTE_TUNING_FORWARD_BATCH_SIZE],
    uint8_t *length);

/**
 * @brief Applies and consumes locally routed telemetry records.
 *
 * Applies route-two records to the selected telemetry state. Extended mode retains ignored records
 * and reports ignored primary records through @p reset_requested; legacy mode consumes the first
 * ignored record and stops the pass.
 *
 * @param[in,out] records Record store containing retained records.
 * @param[in,out] telemetry Telemetry state receiving decoded values.
 * @param[in] extended_mode Selects extended-mode recovery semantics when true.
 * @param[out] reset_requested Set when an ignored primary record requests recovery; may be null.
 * @return Number of route-two records consumed; zero when no record was consumed.
 */
uint8_t usb_remote_tuning_records_consume_telemetry(UsbRemoteTuningRecords *records,
                                                    RemoteTelemetry *telemetry, bool extended_mode,
                                                    bool *reset_requested);

#endif
