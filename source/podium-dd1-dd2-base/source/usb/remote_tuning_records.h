#ifndef OPENTEC_BASE_USB_REMOTE_TUNING_RECORDS_H
#define OPENTEC_BASE_USB_REMOTE_TUNING_RECORDS_H

#include <stdbool.h>
#include <stdint.h>

#include "usb/vendor_command.h"

enum {
    USB_REMOTE_TUNING_RECORD_COUNT = 32,
    USB_REMOTE_TUNING_RECORD_PAYLOAD_SIZE = 15,
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
} UsbRemoteTuningRecords;

void usb_remote_tuning_records_init(UsbRemoteTuningRecords *records);
bool usb_remote_tuning_records_apply(UsbRemoteTuningRecords *records,
                                     const UsbVendorCommand *command);

#endif
