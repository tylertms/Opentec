#include "usb/remote_tuning_records.h"

#include <stddef.h>
#include <string.h>

enum {
    REMOTE_TUNING_PACKET_RECORDS = 1,
    REMOTE_TUNING_PACKET_ALTERNATE_RECORDS = 3,
    REMOTE_TUNING_RECORD_HEADER_SIZE = 5,
};

/**
 * @brief Retains one remote-tuning record.
 *
 * Selects the highest-numbered free slot from the 32-record store and copies the record fields and
 * payload. A slot is free when both its type and selector are zero. A full store drops the record.
 *
 * @param[in,out] records Remote-tuning record store.
 * @param[in] input Complete record header followed by its payload.
 */
static void store_record(UsbRemoteTuningRecords *records, const uint8_t *input) {
    for (uint8_t remaining = USB_REMOTE_TUNING_RECORD_COUNT; remaining != 0; remaining--) {
        UsbRemoteTuningRecord *record = &records->records[remaining - 1];
        if (record->type != 0 || record->selector != 0) {
            continue;
        }

        record->type = input[0];
        record->selector = input[1];
        record->value = (uint16_t)input[2] | (uint16_t)input[3] << 8;
        record->payload_length = input[4];
        memset(record->payload, 0, sizeof(record->payload));
        memcpy(record->payload, input + REMOTE_TUNING_RECORD_HEADER_SIZE, record->payload_length);
        return;
    }
}

/**
 * @brief Initializes the remote-tuning record store.
 *
 * Clears all 32 logical record slots.
 *
 * @param[out] records Remote-tuning record store to initialize.
 */
void usb_remote_tuning_records_init(UsbRemoteTuningRecords *records) {
    memset(records, 0, sizeof(*records));
}

/**
 * @brief Applies a remote-tuning record packet.
 *
 * Accepts packet types one and three and reads consecutive five-byte record headers with payloads
 * up to 15 bytes. Parsing stops at a zero type-and-selector pair, an oversized payload, or an
 * incomplete record. Complete records are retained from slot 31 downward until the store is full.
 *
 * @param[in,out] records Remote-tuning record store.
 * @param[in] command Decoded vendor command containing a remote-tuning packet.
 * @return True when the command selects either remote-tuning record packet type.
 */
bool usb_remote_tuning_records_apply(UsbRemoteTuningRecords *records,
                                     const UsbVendorCommand *command) {
    if (records == NULL || command == NULL || command->kind != USB_VENDOR_COMMAND_REMOTE_TUNING ||
        command->arguments == NULL || command->length == 0) {
        return false;
    }

    uint8_t packet_type = command->arguments[0];
    if (packet_type != REMOTE_TUNING_PACKET_RECORDS &&
        packet_type != REMOTE_TUNING_PACKET_ALTERNATE_RECORDS) {
        return false;
    }

    size_t offset = 1;
    while (offset + REMOTE_TUNING_RECORD_HEADER_SIZE <= command->length) {
        const uint8_t *record = command->arguments + offset;
        if (record[0] == 0 && record[1] == 0) {
            break;
        }

        uint8_t payload_length = record[4];
        size_t record_size = REMOTE_TUNING_RECORD_HEADER_SIZE + payload_length;
        if (payload_length > USB_REMOTE_TUNING_RECORD_PAYLOAD_SIZE ||
            record_size > command->length - offset) {
            break;
        }

        store_record(records, record);
        offset += record_size;
    }
    return true;
}
