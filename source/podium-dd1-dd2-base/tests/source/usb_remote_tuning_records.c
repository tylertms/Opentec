#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "usb/remote_tuning_records.h"

static UsbVendorCommand command_for(uint8_t *arguments, uint8_t length) {
    return (UsbVendorCommand){
        .kind = USB_VENDOR_COMMAND_REMOTE_TUNING,
        .opcode = 5,
        .arguments = arguments,
        .length = length,
    };
}

static void stores_records_in_arrival_order(void) {
    UsbRemoteTuningRecords records;
    usb_remote_tuning_records_init(&records);
    uint8_t arguments[] = {
        1, 0x12, 0x34, 0x78, 0x56, 3, 0xaa, 0xbb, 0xcc, 0x45, 0x67, 0x21, 0x43, 1, 0xdd, 0, 0,
    };
    UsbVendorCommand command = command_for(arguments, sizeof(arguments));

    assert(usb_remote_tuning_records_apply(&records, &command));
    assert(records.count == 2);
    const UsbRemoteTuningRecord *first = &records.records[0];
    const UsbRemoteTuningRecord *second = &records.records[1];
    assert(first->type == 0x12);
    assert(first->selector == 0x34);
    assert(first->value == 0x5678);
    assert(first->payload_length == 3);
    assert(memcmp(first->payload, arguments + 6, 3) == 0);
    assert(second->type == 0x45);
    assert(second->selector == 0x67);
    assert(second->value == 0x4321);
    assert(second->payload_length == 1);
    assert(second->payload[0] == 0xdd);
}

static void accepts_alternate_record_packets(void) {
    UsbRemoteTuningRecords records;
    usb_remote_tuning_records_init(&records);
    uint8_t arguments[] = {3, 2, 0x80, 1, 0, 0};
    UsbVendorCommand command = command_for(arguments, sizeof(arguments));

    assert(usb_remote_tuning_records_apply(&records, &command));
    assert(records.count == 1);
    assert(records.records[0].type == 2);
    assert(records.records[0].selector == 0x80);
    assert(records.records[0].value == 1);
    assert(records.records[0].payload_length == 0);
}

static void stops_at_invalid_or_incomplete_records(void) {
    UsbRemoteTuningRecords records;
    usb_remote_tuning_records_init(&records);
    uint8_t oversized[] = {1, 1, 1, 0, 0, 16};
    UsbVendorCommand oversized_command = command_for(oversized, sizeof(oversized));
    assert(usb_remote_tuning_records_apply(&records, &oversized_command));
    assert(records.count == 0);

    uint8_t incomplete[] = {1, 1, 1, 0, 0, 2, 0xaa};
    UsbVendorCommand incomplete_command = command_for(incomplete, sizeof(incomplete));
    assert(usb_remote_tuning_records_apply(&records, &incomplete_command));
    assert(records.count == 0);
}

static void ignores_non_record_packets(void) {
    UsbRemoteTuningRecords records;
    usb_remote_tuning_records_init(&records);
    uint8_t arguments[] = {2, 1};
    UsbVendorCommand command = command_for(arguments, sizeof(arguments));
    assert(!usb_remote_tuning_records_apply(&records, &command));

    command.kind = USB_VENDOR_COMMAND_TUNING_MENU;
    arguments[0] = 1;
    assert(!usb_remote_tuning_records_apply(&records, &command));
}

static void drops_records_after_the_store_is_full(void) {
    UsbRemoteTuningRecords records;
    usb_remote_tuning_records_init(&records);
    uint8_t arguments[] = {1, 1, 1, 0, 0, 0};
    UsbVendorCommand command = command_for(arguments, sizeof(arguments));

    for (uint8_t index = 0; index < USB_REMOTE_TUNING_RECORD_COUNT; index++) {
        arguments[3] = index;
        assert(usb_remote_tuning_records_apply(&records, &command));
    }
    arguments[3] = UINT8_MAX;
    assert(usb_remote_tuning_records_apply(&records, &command));

    assert(records.count == USB_REMOTE_TUNING_RECORD_COUNT);
    assert(records.records[0].value == 0);
    assert(records.records[31].value == USB_REMOTE_TUNING_RECORD_COUNT - 1);
}

static void routes_records_by_link_and_selector_bank(void) {
    UsbRemoteTuningRecords records;
    usb_remote_tuning_records_init(&records);
    uint8_t arguments[] = {
        1, 3, 0x01, 0x34, 0x12, 2, 0xaa, 0xbb, 3,    0x80, 0x78, 0x56,
        0, 4, 0x82, 0xbc, 0x9a, 1, 0xcc, 2,    0x04, 0x11, 0x22, 0,
    };
    UsbVendorCommand command = command_for(arguments, sizeof(arguments));
    assert(usb_remote_tuning_records_apply(&records, &command));

    RemoteTuningResponse response;
    assert(
        usb_remote_tuning_records_take_response(&records, REMOTE_TUNING_LINK_EXTENDED, &response));
    assert(response.code == REMOTE_TUNING_RESPONSE_RECORDS);
    assert(response.record_data_length == 7);
    assert(memcmp(response.record_data, arguments + 1, 7) == 0);

    assert(
        usb_remote_tuning_records_take_response(&records, REMOTE_TUNING_LINK_EXTENDED, &response));
    assert(response.code == REMOTE_TUNING_RESPONSE_ALTERNATE_RECORDS);
    assert(response.record_data_length == 5);
    assert(memcmp(response.record_data, arguments + 8, 5) == 0);
    assert(
        !usb_remote_tuning_records_take_response(&records, REMOTE_TUNING_LINK_EXTENDED, &response));

    assert(usb_remote_tuning_records_take_response(&records, REMOTE_TUNING_LINK_LEGACY, &response));
    assert(response.code == REMOTE_TUNING_RESPONSE_RECORDS);
    assert(response.record_data_length == 6);
    assert(memcmp(response.record_data, arguments + 13, 6) == 0);
    assert(records.count == 1);
    assert(records.records[0].type == 2);
}

static void keeps_complete_records_within_each_response(void) {
    UsbRemoteTuningRecords records;
    usb_remote_tuning_records_init(&records);
    uint8_t arguments[61] = {1};
    for (uint8_t record = 0; record < 3; record++) {
        uint8_t offset = 1 + record * 20;
        arguments[offset] = 4;
        arguments[offset + 1] = record + 1;
        arguments[offset + 4] = 15;
        memset(arguments + offset + 5, 0xa0 + record, 15);
    }
    UsbVendorCommand command = command_for(arguments, sizeof(arguments));
    assert(usb_remote_tuning_records_apply(&records, &command));

    RemoteTuningResponse response;
    for (uint8_t record = 0; record < 3; record++) {
        assert(usb_remote_tuning_records_take_response(&records, REMOTE_TUNING_LINK_LEGACY,
                                                       &response));
        assert(response.record_data_length == 20);
        assert(response.record_data[1] == record + 1);
    }
    assert(records.count == 0);
}

int main(void) {
    stores_records_in_arrival_order();
    accepts_alternate_record_packets();
    stops_at_invalid_or_incomplete_records();
    ignores_non_record_packets();
    drops_records_after_the_store_is_full();
    routes_records_by_link_and_selector_bank();
    keeps_complete_records_within_each_response();
    return 0;
}
