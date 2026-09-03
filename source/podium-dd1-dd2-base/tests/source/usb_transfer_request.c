#include <assert.h>
#include <stdint.h>
#include <string.h>

#include "usb/transfer_request.h"

static UsbVendorCommand make_command(uint8_t *arguments, uint8_t length) {
    return (UsbVendorCommand){
        .kind = USB_VENDOR_COMMAND_TUNING_MENU,
        .arguments = arguments,
        .length = length,
    };
}

static UsbVendorCommand make_direct_command(uint8_t opcode, uint8_t *arguments, uint8_t length) {
    return (UsbVendorCommand){
        .kind = USB_VENDOR_COMMAND_TRANSFER_REQUEST,
        .opcode = opcode,
        .arguments = arguments,
        .length = length,
    };
}

static void test_decodes_single_request(void) {
    UsbTransferRequest request;
    uint8_t arguments[] = {0x10, 0, 3, 0xaa, 0xbb, 0xcc, 0xdd, 0xee};
    UsbVendorCommand command = make_command(arguments, sizeof(arguments));
    usb_transfer_request_init(&request);

    assert(usb_transfer_request_apply(&request, &command));
    const UsbTransferRequestPayload *payload = usb_transfer_request_payload(&request);
    assert(payload != NULL && payload->length == 6);
    assert(memcmp(payload->data, arguments + 2, payload->length) == 0);
    usb_transfer_request_release(&request);
    assert(usb_transfer_request_payload(&request) == NULL);
}

static void test_reassembles_first_and_final_requests(void) {
    UsbTransferRequest request;
    uint8_t first[62] = {0x11, 0, 100};
    uint8_t final[62] = {0x13, 7};
    for (uint8_t index = 0; index < 59; index++) {
        first[index + 3] = index;
    }
    for (uint8_t index = 0; index < 41; index++) {
        final[index + 2] = (uint8_t)(index + 59);
    }
    UsbVendorCommand command = make_command(first, sizeof(first));
    usb_transfer_request_init(&request);

    assert(usb_transfer_request_apply(&request, &command));
    assert(usb_transfer_request_payload(&request) == NULL);
    command = make_command(final, sizeof(final));
    assert(usb_transfer_request_apply(&request, &command));
    const UsbTransferRequestPayload *payload = usb_transfer_request_payload(&request);
    assert(payload != NULL && payload->length == 100);
    for (uint8_t index = 0; index < payload->length; index++) {
        assert(payload->data[index] == index);
    }
}

static void test_decodes_direct_single_request(void) {
    UsbTransferRequest request;
    uint8_t arguments[] = {3, 0xaa, 0xbb, 0xcc, 0xdd, 0xee};
    UsbVendorCommand command = make_direct_command(0x10, arguments, sizeof(arguments));
    usb_transfer_request_init(&request);

    assert(usb_transfer_request_apply(&request, &command));
    const UsbTransferRequestPayload *payload = usb_transfer_request_payload(&request);
    assert(payload != NULL && payload->length == sizeof(arguments));
    assert(memcmp(payload->data, arguments, sizeof(arguments)) == 0);
}

static void test_reassembles_direct_first_and_final_requests(void) {
    UsbTransferRequest request;
    uint8_t first[62] = {0, 121};
    uint8_t final[62] = {7};
    for (uint8_t index = 0; index < 60; index++) {
        first[index + 2] = index;
    }
    for (uint8_t index = 0; index < 61; index++) {
        final[index + 1] = (uint8_t)(index + 60);
    }
    usb_transfer_request_init(&request);
    UsbVendorCommand command = make_direct_command(0x11, first, sizeof(first));

    assert(usb_transfer_request_apply(&request, &command));
    assert(usb_transfer_request_payload(&request) == NULL);
    command = make_direct_command(0x13, final, sizeof(final));
    assert(usb_transfer_request_apply(&request, &command));
    const UsbTransferRequestPayload *payload = usb_transfer_request_payload(&request);
    assert(payload != NULL && payload->length == 121);
    for (uint8_t index = 0; index < payload->length; index++) {
        assert(payload->data[index] == index);
    }
}

static void test_accepts_direct_final_fragment_at_62_bytes(void) {
    UsbTransferRequest request;
    uint8_t first[62] = {0, 123};
    uint8_t final[63] = {0};
    for (uint8_t index = 0; index < 60; index++) {
        first[index + 2] = index;
    }
    for (uint8_t index = 0; index < 62; index++) {
        final[index + 1] = (uint8_t)(index + 60);
    }
    usb_transfer_request_init(&request);

    UsbVendorCommand command = make_direct_command(0x11, first, sizeof(first));
    assert(usb_transfer_request_apply(&request, &command));
    command = make_direct_command(0x13, final, sizeof(final));
    assert(usb_transfer_request_apply(&request, &command));
    assert(request.cursor == 122);
    assert(request.active);
    assert(usb_transfer_request_payload(&request) == NULL);
    for (uint8_t index = 0; index < 122; index++) {
        assert(request.payload.data[index] == index);
    }

    uint8_t last[] = {0, 122};
    command = make_direct_command(0x13, last, sizeof(last));
    assert(usb_transfer_request_apply(&request, &command));
    const UsbTransferRequestPayload *payload = usb_transfer_request_payload(&request);
    assert(payload != NULL && payload->length == 123);
    for (uint8_t index = 0; index < payload->length; index++) {
        assert(payload->data[index] == index);
    }
}

static void test_accepts_multiple_final_fragments(void) {
    UsbTransferRequest request;
    uint8_t first[62] = {0x11, 0, 124};
    uint8_t final[62] = {0x13, 1};
    uint8_t last[] = {0x13, 2, 119, 120, 121, 122, 123};
    for (uint8_t index = 0; index < 59; index++) {
        first[index + 3] = index;
    }
    for (uint8_t index = 0; index < 60; index++) {
        final[index + 2] = (uint8_t)(index + 59);
    }
    usb_transfer_request_init(&request);
    UsbVendorCommand command = make_command(first, sizeof(first));
    assert(usb_transfer_request_apply(&request, &command));
    command = make_command(final, sizeof(final));
    assert(usb_transfer_request_apply(&request, &command));
    assert(usb_transfer_request_payload(&request) == NULL);
    command = make_command(last, sizeof(last));
    assert(usb_transfer_request_apply(&request, &command));
    const UsbTransferRequestPayload *payload = usb_transfer_request_payload(&request);
    assert(payload != NULL && payload->length == 124);
    for (uint8_t index = 0; index < payload->length; index++) {
        assert(payload->data[index] == index);
    }
}

static void test_ignores_invalid_transfer_forms(void) {
    UsbTransferRequest request;
    uint8_t unsupported[] = {0x12};
    uint8_t oversized[] = {0x11, 0, 125};
    uint8_t incomplete_single[] = {0x10, 0, 10};
    UsbVendorCommand command = make_command(unsupported, sizeof(unsupported));
    usb_transfer_request_init(&request);

    assert(!usb_transfer_request_apply(&request, &command));
    command = make_command(oversized, sizeof(oversized));
    assert(usb_transfer_request_apply(&request, &command));
    command = make_command(incomplete_single, sizeof(incomplete_single));
    assert(usb_transfer_request_apply(&request, &command));
    assert(usb_transfer_request_payload(&request) == NULL);
}

int main(void) {
    test_decodes_single_request();
    test_decodes_direct_single_request();
    test_reassembles_first_and_final_requests();
    test_reassembles_direct_first_and_final_requests();
    test_accepts_direct_final_fragment_at_62_bytes();
    test_accepts_multiple_final_fragments();
    test_ignores_invalid_transfer_forms();
    return 0;
}
