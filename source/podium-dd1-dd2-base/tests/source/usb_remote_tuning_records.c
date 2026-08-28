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

static void stores_records_from_the_highest_slot(void) {
    UsbRemoteTuningRecords records;
    usb_remote_tuning_records_init(&records);
    uint8_t arguments[] = {
        1, 0x12, 0x34, 0x78, 0x56, 3, 0xaa, 0xbb, 0xcc, 0x45, 0x67, 0x21, 0x43, 1, 0xdd, 0, 0,
    };
    UsbVendorCommand command = command_for(arguments, sizeof(arguments));

    assert(usb_remote_tuning_records_apply(&records, &command));
    const UsbRemoteTuningRecord *first = &records.records[31];
    const UsbRemoteTuningRecord *second = &records.records[30];
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
    assert(records.records[31].type == 2);
    assert(records.records[31].selector == 0x80);
    assert(records.records[31].value == 1);
    assert(records.records[31].payload_length == 0);
}

static void stops_at_invalid_or_incomplete_records(void) {
    UsbRemoteTuningRecords records;
    usb_remote_tuning_records_init(&records);
    uint8_t oversized[] = {1, 1, 1, 0, 0, 16};
    UsbVendorCommand oversized_command = command_for(oversized, sizeof(oversized));
    assert(usb_remote_tuning_records_apply(&records, &oversized_command));
    assert(records.records[31].type == 0);

    uint8_t incomplete[] = {1, 1, 1, 0, 0, 2, 0xaa};
    UsbVendorCommand incomplete_command = command_for(incomplete, sizeof(incomplete));
    assert(usb_remote_tuning_records_apply(&records, &incomplete_command));
    assert(records.records[31].type == 0);
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

    assert(records.records[0].value == USB_REMOTE_TUNING_RECORD_COUNT - 1);
    assert(records.records[31].value == 0);
}

int main(void) {
    stores_records_from_the_highest_slot();
    accepts_alternate_record_packets();
    stops_at_invalid_or_incomplete_records();
    ignores_non_record_packets();
    drops_records_after_the_store_is_full();
    return 0;
}
