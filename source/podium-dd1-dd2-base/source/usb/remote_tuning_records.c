#include "usb/remote_tuning_records.h"

#include <stddef.h>
#include <string.h>

enum {
    REMOTE_TUNING_PACKET_RECORDS = 1,
    REMOTE_TUNING_PACKET_ALTERNATE_RECORDS = 3,
    REMOTE_TUNING_RECORD_HEADER_SIZE = 5,
    REMOTE_TUNING_RECORD_ROUTE_EXTENDED = 3,
    REMOTE_TUNING_RECORD_ROUTE_LEGACY = 4,
    REMOTE_TUNING_ALTERNATE_RECORD_FLAG = 0x80,
};

/**
 * @brief Retains one remote-tuning record.
 *
 * Appends the record to the 32-entry arrival-order store. A full store drops the record.
 *
 * @param[in,out] records Remote-tuning record store.
 * @param[in] input Complete record header followed by its payload.
 */
static void store_record(UsbRemoteTuningRecords *records, const uint8_t *input) {
    if (records->count >= USB_REMOTE_TUNING_RECORD_COUNT) {
        return;
    }

    UsbRemoteTuningRecord *record = &records->records[records->count++];
    record->type = input[0];
    record->selector = input[1];
    record->value = (uint16_t)input[2] | (uint16_t)input[3] << 8;
    record->payload_length = input[4];
    memset(record->payload, 0, sizeof(record->payload));
    memcpy(record->payload, input + REMOTE_TUNING_RECORD_HEADER_SIZE, record->payload_length);
}

/**
 * @brief Tests whether a record belongs to one attached-wheel response channel.
 *
 * Legacy transport accepts route-four records from both selector banks. Extended transport accepts
 * route-three records and separates selectors by bit 7.
 *
 * @param[in] record Retained remote-tuning record.
 * @param[in] link Attached-wheel response link.
 * @param[in] alternate Extended selector-bank choice.
 * @return True when the record belongs to the selected channel.
 */
static bool record_matches(const UsbRemoteTuningRecord *record, RemoteTuningLink link,
                           bool alternate) {
    if (link == REMOTE_TUNING_LINK_LEGACY) {
        return record->type == REMOTE_TUNING_RECORD_ROUTE_LEGACY;
    }
    return link == REMOTE_TUNING_LINK_EXTENDED &&
           record->type == REMOTE_TUNING_RECORD_ROUTE_EXTENDED &&
           ((record->selector & REMOTE_TUNING_ALTERNATE_RECORD_FLAG) != 0) == alternate;
}

/**
 * @brief Appends one complete record to attached-wheel response data.
 *
 * Writes the type, selector, little-endian value, payload length, and payload without padding.
 *
 * @param[in] record Logical record to serialize.
 * @param[in,out] response Response receiving the serialized record.
 */
static void append_record(const UsbRemoteTuningRecord *record, RemoteTuningResponse *response) {
    uint8_t *output = response->record_data + response->record_data_length;
    output[0] = record->type;
    output[1] = record->selector;
    output[2] = (uint8_t)record->value;
    output[3] = (uint8_t)(record->value >> 8);
    output[4] = record->payload_length;
    memcpy(output + REMOTE_TUNING_RECORD_HEADER_SIZE, record->payload, record->payload_length);
    response->record_data_length += REMOTE_TUNING_RECORD_HEADER_SIZE + record->payload_length;
}

/**
 * @brief Takes one bounded response from a selected record channel.
 *
 * Serializes complete matching records in arrival order until the next record would exceed the
 * 30-byte response area. Consumed records are removed while all other records retain their order.
 *
 * @param[in,out] records Arrival-order record store.
 * @param[in] link Attached-wheel response link.
 * @param[in] alternate Extended selector-bank choice.
 * @param[out] response Response containing zero or more complete serialized records.
 * @return True when at least one record was consumed.
 */
static bool take_channel(UsbRemoteTuningRecords *records, RemoteTuningLink link, bool alternate,
                         RemoteTuningResponse *response) {
    memset(response, 0, sizeof(*response));
    response->link = link;
    response->code =
        alternate ? REMOTE_TUNING_RESPONSE_ALTERNATE_RECORDS : REMOTE_TUNING_RESPONSE_RECORDS;
    uint8_t retained = 0;
    bool full = false;

    for (uint8_t index = 0; index < records->count; index++) {
        UsbRemoteTuningRecord *record = &records->records[index];
        uint8_t record_size = REMOTE_TUNING_RECORD_HEADER_SIZE + record->payload_length;
        bool consume = !full && record_matches(record, link, alternate);
        if (consume &&
            record_size <= REMOTE_TUNING_RECORD_DATA_SIZE - response->record_data_length) {
            append_record(record, response);
            continue;
        }
        if (consume) {
            full = true;
        }
        records->records[retained++] = *record;
    }

    if (response->record_data_length == 0) {
        return false;
    }
    memset(records->records + retained, 0,
           (records->count - retained) * sizeof(records->records[0]));
    records->count = retained;
    return true;
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

/**
 * @brief Takes the next attached-wheel record response.
 *
 * Legacy responses combine both selector banks from route four. Extended responses select route
 * three and drain the standard selector bank before the alternate bank. Other links retain every
 * record.
 *
 * @param[in,out] records Arrival-order record store.
 * @param[in] link Attached-wheel response link.
 * @param[out] response Next bounded record response.
 * @return True when a response was produced.
 */
bool usb_remote_tuning_records_take_response(UsbRemoteTuningRecords *records, RemoteTuningLink link,
                                             RemoteTuningResponse *response) {
    if (records == NULL || response == NULL) {
        return false;
    }
    if (link == REMOTE_TUNING_LINK_LEGACY) {
        return take_channel(records, link, false, response);
    }
    if (link == REMOTE_TUNING_LINK_EXTENDED) {
        return take_channel(records, link, false, response) ||
               take_channel(records, link, true, response);
    }
    return false;
}
