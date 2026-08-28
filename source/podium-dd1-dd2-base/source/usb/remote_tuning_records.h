#ifndef OPENTEC_BASE_USB_REMOTE_TUNING_RECORDS_H
#define OPENTEC_BASE_USB_REMOTE_TUNING_RECORDS_H

#include <stdbool.h>
#include <stdint.h>

#include "remote_tuning/response.h"
#include "remote_tuning/telemetry.h"
#include "usb/vendor_command.h"

enum {
    USB_REMOTE_TUNING_RECORD_COUNT = 32,
    USB_REMOTE_TUNING_RECORD_PAYLOAD_SIZE = 15,
    USB_REMOTE_TUNING_FORWARD_BATCH_SIZE = 61,
};

/** @brief One logical remote-tuning control record. */
typedef struct {
    uint8_t type;
    uint8_t selector;
    uint16_t value;
    uint8_t payload_length;
    uint8_t payload[USB_REMOTE_TUNING_RECORD_PAYLOAD_SIZE];
} UsbRemoteTuningRecord;

/** @brief Retained remote-tuning control records. */
typedef struct {
    UsbRemoteTuningRecord records[USB_REMOTE_TUNING_RECORD_COUNT];
    uint8_t count;
} UsbRemoteTuningRecords;

void usb_remote_tuning_records_init(UsbRemoteTuningRecords *records);
bool usb_remote_tuning_records_apply(UsbRemoteTuningRecords *records,
                                     const UsbVendorCommand *command);
bool usb_remote_tuning_records_take_response(UsbRemoteTuningRecords *records, RemoteTuningLink link,
                                             RemoteTuningResponse *response);
bool usb_remote_tuning_records_take_forward_batch(
    UsbRemoteTuningRecords *records, uint8_t output[USB_REMOTE_TUNING_FORWARD_BATCH_SIZE],
    uint8_t *length);
uint8_t usb_remote_tuning_records_consume_telemetry(UsbRemoteTuningRecords *records,
                                                    RemoteTelemetry *telemetry);

#endif
