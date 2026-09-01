#include "usb/remote_tuning_records.h"

#include <stddef.h>
#include <string.h>

/** @brief Remote-tuning record packet and route constants. */
enum {
    REMOTE_TUNING_PACKET_RECORDS = 1, /**< Standard remote-tuning record packet type. */
    REMOTE_TUNING_PACKET_ALTERNATE_RECORDS =
        3,                                /**< Alternate-bank remote-tuning record packet type. */
    REMOTE_TUNING_RECORD_HEADER_SIZE = 5, /**< Number of bytes in a serialized record header. */
    REMOTE_TUNING_RECORD_ROUTE_TWO = 2,   /**< Local telemetry record route. */
    REMOTE_TUNING_RECORD_ROUTE_THREE = 3, /**< Generic attached-device forwarding route. */
    REMOTE_TUNING_RECORD_ROUTE_FOUR = 4,  /**< Attached-wheel response route. */
    REMOTE_TUNING_ALTERNATE_RECORD_FLAG =
        0x80, /**< Selector bit identifying an alternate record bank. */
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
 * @brief Tests whether a record belongs to a selected route and selector bank.
 *
 * Matches the route exactly and compares only the selector bits selected by the mask.
 *
 * @param[in] record Retained remote-tuning record.
 * @param[in] route Required record route.
 * @param[in] selector_mask Selector bits to compare.
 * @param[in] selector_value Required value of the selected bits.
 * @return True when the route and selected selector bits match; otherwise false.
 */
static bool record_matches(const UsbRemoteTuningRecord *record, uint8_t route,
                           uint8_t selector_mask, uint8_t selector_value) {
    return record->type == route && (record->selector & selector_mask) == selector_value;
}

/**
 * @brief Appends one complete record to a serialized record batch.
 *
 * Writes the type, selector, little-endian value, payload length, and payload without padding.
 *
 * @param[in] record Logical record to serialize.
 * @param[out] output Serialized record destination.
 * @param[in,out] length Number of bytes already stored and updated result length.
 */
static void append_record(const UsbRemoteTuningRecord *record, uint8_t *output, uint8_t *length) {
    uint8_t *destination = output + *length;
    destination[0] = record->type;
    destination[1] = record->selector;
    destination[2] = (uint8_t)record->value;
    destination[3] = (uint8_t)(record->value >> 8);
    destination[4] = record->payload_length;
    memcpy(destination + REMOTE_TUNING_RECORD_HEADER_SIZE, record->payload, record->payload_length);
    *length += REMOTE_TUNING_RECORD_HEADER_SIZE + record->payload_length;
}

/**
 * @brief Takes one bounded batch of matching records.
 *
 * Serializes complete matching records from newest to oldest. The first matching record that does
 * not fit ends the batch. Consumed records are removed while every retained record keeps its order.
 *
 * @param[in,out] records Arrival-order record store.
 * @param[in] route Record route to select.
 * @param[in] selector_mask Selector bits used to choose a bank.
 * @param[in] selector_value Required value of the selected bits.
 * @param[out] output Serialized record destination.
 * @param[in] capacity Maximum serialized byte count.
 * @param[out] length Produced serialized byte count.
 * @return True when at least one record was consumed; otherwise false.
 */
static bool take_matching(UsbRemoteTuningRecords *records, uint8_t route, uint8_t selector_mask,
                          uint8_t selector_value, uint8_t *output, uint8_t capacity,
                          uint8_t *length) {
    *length = 0;
    bool consumed[USB_REMOTE_TUNING_RECORD_COUNT] = {false};
    bool full = false;

    for (uint8_t cursor = records->count; cursor > 0; cursor--) {
        uint8_t index = cursor - 1;
        UsbRemoteTuningRecord *record = &records->records[index];
        uint8_t record_size = REMOTE_TUNING_RECORD_HEADER_SIZE + record->payload_length;
        bool consume = !full && record_matches(record, route, selector_mask, selector_value);
        if (consume && record_size <= capacity - *length) {
            append_record(record, output, length);
            consumed[index] = true;
            continue;
        }
        if (consume) {
            full = true;
        }
    }

    if (*length == 0) {
        return false;
    }
    uint8_t retained = 0;
    for (uint8_t index = 0; index < records->count; index++) {
        if (!consumed[index]) {
            records->records[retained++] = records->records[index];
        }
    }
    memset(records->records + retained, 0,
           (records->count - retained) * sizeof(records->records[0]));
    records->count = retained;
    return true;
}

/**
 * @brief Takes one bounded response from a selected record channel.
 *
 * Serializes complete matching records from newest to oldest until the next record would exceed
 * the 30-byte response area. Consumed records are removed while all other records retain their
 * order.
 *
 * @param[in,out] records Arrival-order record store.
 * @param[in] link Attached-wheel response link.
 * @param[in] alternate Extended selector-bank choice.
 * @param[out] response Response containing zero or more complete serialized records.
 * @return True when at least one record was consumed; otherwise false.
 */
static bool take_channel(UsbRemoteTuningRecords *records, RemoteTuningLink link, bool alternate,
                         RemoteTuningResponse *response) {
    memset(response, 0, sizeof(*response));
    response->link = link;
    response->code =
        alternate ? REMOTE_TUNING_RESPONSE_ALTERNATE_RECORDS : REMOTE_TUNING_RESPONSE_RECORDS;
    uint8_t route = link == REMOTE_TUNING_LINK_LEGACY ? REMOTE_TUNING_RECORD_ROUTE_FOUR
                                                      : REMOTE_TUNING_RECORD_ROUTE_THREE;
    uint8_t selector_mask =
        link == REMOTE_TUNING_LINK_EXTENDED ? REMOTE_TUNING_ALTERNATE_RECORD_FLAG : 0;
    uint8_t selector_value = alternate ? REMOTE_TUNING_ALTERNATE_RECORD_FLAG : 0;
    return take_matching(records, route, selector_mask, selector_value, response->record_data,
                         sizeof(response->record_data), &response->record_data_length);
}

void usb_remote_tuning_records_init(UsbRemoteTuningRecords *records) {
    memset(records, 0, sizeof(*records));
}

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

bool usb_remote_tuning_records_take_forward_batch(
    UsbRemoteTuningRecords *records, uint8_t output[USB_REMOTE_TUNING_FORWARD_BATCH_SIZE],
    uint8_t *length) {
    if (records == NULL || output == NULL || length == NULL) {
        return false;
    }
    return take_matching(records, REMOTE_TUNING_RECORD_ROUTE_THREE, 0, 0, output,
                         USB_REMOTE_TUNING_FORWARD_BATCH_SIZE, length);
}

uint8_t usb_remote_tuning_records_consume_telemetry(UsbRemoteTuningRecords *records,
                                                    RemoteTelemetry *telemetry, bool extended_mode,
                                                    bool *reset_requested) {
    if (records == NULL || telemetry == NULL) {
        return 0;
    }

    bool consumed_records[USB_REMOTE_TUNING_RECORD_COUNT] = {false};
    uint8_t consumed = 0;
    if (reset_requested != NULL) {
        *reset_requested = false;
    }
    for (uint8_t cursor = records->count; cursor > 0; cursor--) {
        uint8_t index = cursor - 1;
        UsbRemoteTuningRecord *record = &records->records[index];
        if (record->type != REMOTE_TUNING_RECORD_ROUTE_TWO) {
            continue;
        }

        uint8_t channel = record->selector & 0x0f;
        RemoteTelemetryRecordResult result;
        if ((record->selector & REMOTE_TUNING_ALTERNATE_RECORD_FLAG) != 0) {
            result = remote_telemetry_apply_overlay(telemetry, channel, record->value,
                                                    record->payload, record->payload_length);
        } else {
            result = remote_telemetry_apply_primary(telemetry, channel, record->value,
                                                    record->payload, record->payload_length);
        }
        if (result != REMOTE_TELEMETRY_RECORD_IGNORED) {
            consumed_records[index] = true;
            consumed++;
            continue;
        }
        if (extended_mode) {
            if ((record->selector & REMOTE_TUNING_ALTERNATE_RECORD_FLAG) == 0 &&
                reset_requested != NULL) {
                *reset_requested = true;
            }
            continue;
        }
        consumed_records[index] = true;
        consumed++;
        break;
    }

    uint8_t retained = 0;
    for (uint8_t index = 0; index < records->count; index++) {
        if (!consumed_records[index]) {
            records->records[retained++] = records->records[index];
        }
    }
    memset(records->records + retained, 0,
           (records->count - retained) * sizeof(records->records[0]));
    records->count = retained;
    return consumed;
}
