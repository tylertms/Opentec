#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "motor/calibration.h"
#include "platform/aux_bus.h"

typedef enum {
    TRANSFER_NONE,
    TRANSFER_READ,
    TRANSFER_WRITE,
} TransferKind;

static PlatformAuxBusStatus bus_status;
static TransferKind transfer_kind;
static uint8_t bus_address;
static uint16_t register_address;
static uint8_t transfer_data[2];
static uint8_t *read_destination;
static uint16_t transfer_length;
static uint8_t start_count;

bool platform_aux_bus_start_write(uint8_t address, uint16_t address_register, const uint8_t *data,
                                  uint16_t length) {
    if (bus_status != PLATFORM_AUX_BUS_IDLE) {
        return false;
    }
    transfer_kind = TRANSFER_WRITE;
    bus_address = address;
    register_address = address_register;
    transfer_length = length;
    transfer_data[0] = data[0];
    transfer_data[1] = data[1];
    start_count++;
    bus_status = PLATFORM_AUX_BUS_BUSY;
    return true;
}

bool platform_aux_bus_start_read(uint8_t address, uint16_t address_register, uint8_t *data,
                                 uint16_t length) {
    if (bus_status != PLATFORM_AUX_BUS_IDLE) {
        return false;
    }
    transfer_kind = TRANSFER_READ;
    bus_address = address;
    register_address = address_register;
    read_destination = data;
    transfer_length = length;
    start_count++;
    bus_status = PLATFORM_AUX_BUS_BUSY;
    return true;
}

PlatformAuxBusStatus platform_aux_bus_status(void) { return bus_status; }

void platform_aux_bus_clear(void) { bus_status = PLATFORM_AUX_BUS_IDLE; }

static void reset_bus(void) {
    bus_status = PLATFORM_AUX_BUS_IDLE;
    transfer_kind = TRANSFER_NONE;
    bus_address = 0;
    register_address = 0;
    transfer_data[0] = 0;
    transfer_data[1] = 0;
    read_destination = NULL;
    transfer_length = 0;
    start_count = 0;
}

static UsbOutputCommand short_command(const uint8_t *payload, uint8_t length) {
    return (UsbOutputCommand){
        .kind = USB_OUTPUT_COMMAND_SHORT,
        .payload = payload,
        .length = length,
    };
}

static MotorTelemetry available_accessory(void) {
    return (MotorTelemetry){
        .accessory_type = 1,
        .accessory_type_valid = true,
    };
}

static void finish_write(bool succeeded) {
    bus_status = succeeded ? PLATFORM_AUX_BUS_SUCCEEDED : PLATFORM_AUX_BUS_FAILED;
}

static void finish_read(uint16_t response, bool succeeded) {
    read_destination[0] = (uint8_t)response;
    read_destination[1] = (uint8_t)(response >> 8);
    bus_status = succeeded ? PLATFORM_AUX_BUS_SUCCEEDED : PLATFORM_AUX_BUS_FAILED;
}

static void test_decodes_host_signatures(void) {
    const uint8_t calibrate_payload[7] = {0xf9, 4, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa};
    const uint8_t erase_payload[7] = {0xf9, 4, 0xbb, 0xbb, 0xbb, 0xbb, 0xbb};
    UsbOutputCommand calibrate = short_command(calibrate_payload, sizeof(calibrate_payload));
    UsbOutputCommand erase = short_command(erase_payload, sizeof(erase_payload));
    MotorCalibrationOperation operation;

    assert(motor_calibration_command_decode(&calibrate, &operation));
    assert(operation == MOTOR_CALIBRATION_OPERATION_CALIBRATE);
    assert(motor_calibration_command_decode(&erase, &operation));
    assert(operation == MOTOR_CALIBRATION_OPERATION_ERASE);
}

static void test_rejects_partial_or_malformed_signatures(void) {
    uint8_t payload[7] = {0xf9, 4, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa};
    UsbOutputCommand command = short_command(payload, sizeof(payload));
    MotorCalibrationOperation operation;

    payload[6] = 0xbb;
    assert(!motor_calibration_command_decode(&command, &operation));
    payload[6] = 0xaa;
    command.length--;
    assert(!motor_calibration_command_decode(&command, &operation));
    command.length++;
    payload[0] = 0xf8;
    assert(!motor_calibration_command_decode(&command, &operation));
    payload[0] = 0xf9;
    payload[1] = 5;
    assert(!motor_calibration_command_decode(&command, &operation));
    payload[1] = 4;
    command.kind = USB_OUTPUT_COMMAND_VENDOR_TRANSFER;
    assert(!motor_calibration_command_decode(&command, &operation));
    command.kind = USB_OUTPUT_COMMAND_SHORT;
    command.payload = NULL;
    assert(!motor_calibration_command_decode(&command, &operation));
    assert(!motor_calibration_command_decode(NULL, &operation));
    assert(!motor_calibration_command_decode(&command, NULL));
}

static void test_calibrates_after_idle_until_zero_response(void) {
    MotorCalibrationService service;
    MotorTelemetry telemetry = available_accessory();
    reset_bus();
    motor_calibration_service_init(&service);
    assert(motor_calibration_service_take_event(&service) == MOTOR_CALIBRATION_EVENT_NONE);
    motor_calibration_service_request(&service, MOTOR_CALIBRATION_OPERATION_CALIBRATE);

    motor_calibration_service_run(&service, 0, &telemetry);
    assert(motor_calibration_service_take_event(&service) == MOTOR_CALIBRATION_EVENT_NONE);
    assert(transfer_kind == TRANSFER_READ);
    assert(bus_address == 0x78);
    assert(register_address == 6);
    assert(transfer_length == 2);
    assert(motor_calibration_service_owns_bus(&service));
    assert(!motor_calibration_service_active(&service));

    finish_read(1, true);
    motor_calibration_service_run(&service, 0, &telemetry);
    assert(transfer_kind == TRANSFER_READ);
    assert(start_count == 2);
    assert(motor_calibration_service_take_event(&service) == MOTOR_CALIBRATION_EVENT_NONE);

    finish_read(0, true);
    motor_calibration_service_run(&service, 0, &telemetry);
    assert(transfer_kind == TRANSFER_WRITE);
    assert(transfer_data[0] == 0xaa && transfer_data[1] == 0xaa);
    assert(start_count == 3);
    assert(motor_calibration_service_active(&service));
    assert(motor_calibration_service_take_event(&service) == MOTOR_CALIBRATION_EVENT_STARTED);

    finish_write(true);
    motor_calibration_service_run(&service, 0, &telemetry);
    assert(transfer_kind == TRANSFER_READ);
    assert(start_count == 4);

    finish_read(1, true);
    motor_calibration_service_run(&service, 0, &telemetry);
    assert(transfer_kind == TRANSFER_READ);
    assert(start_count == 5);
    assert(motor_calibration_service_pending(&service));

    finish_read(0, true);
    motor_calibration_service_run(&service, 0, &telemetry);
    assert(!motor_calibration_service_pending(&service));
    assert(!motor_calibration_service_active(&service));
    assert(motor_calibration_service_take_event(&service) == MOTOR_CALIBRATION_EVENT_COMPLETED);
    assert(motor_calibration_service_take_event(&service) == MOTOR_CALIBRATION_EVENT_NONE);
}

static void test_erases_in_any_wheel_mode(void) {
    MotorCalibrationService service;
    MotorTelemetry telemetry = available_accessory();
    reset_bus();
    motor_calibration_service_init(&service);
    motor_calibration_service_request(&service, MOTOR_CALIBRATION_OPERATION_ERASE);

    motor_calibration_service_run(&service, 7, &telemetry);
    assert(transfer_kind == TRANSFER_READ);
    finish_read(0, true);
    motor_calibration_service_run(&service, 7, &telemetry);
    assert(transfer_kind == TRANSFER_WRITE);
    assert(transfer_data[0] == 0xbb && transfer_data[1] == 0xbb);
    finish_write(true);
    motor_calibration_service_run(&service, 7, &telemetry);
    finish_read(1, true);
    motor_calibration_service_run(&service, 7, &telemetry);
    finish_read(0, true);
    motor_calibration_service_run(&service, 7, &telemetry);
    assert(motor_calibration_service_take_event(&service) == MOTOR_CALIBRATION_EVENT_ERASED);
}

static void test_rejects_unavailable_operations(void) {
    MotorCalibrationService service;
    MotorTelemetry unavailable = {0};
    MotorTelemetry telemetry = available_accessory();
    reset_bus();
    motor_calibration_service_init(&service);
    motor_calibration_service_request(&service, MOTOR_CALIBRATION_OPERATION_CALIBRATE);
    motor_calibration_service_run(&service, 1, &telemetry);
    assert(!motor_calibration_service_pending(&service));
    assert(start_count == 0);
    assert(motor_calibration_service_take_event(&service) ==
           MOTOR_CALIBRATION_EVENT_DISCONNECT_WHEEL);

    motor_calibration_service_request(&service, MOTOR_CALIBRATION_OPERATION_ERASE);
    motor_calibration_service_run(&service, 0, &unavailable);
    assert(!motor_calibration_service_pending(&service));
    assert(start_count == 0);
    assert(motor_calibration_service_take_event(&service) == MOTOR_CALIBRATION_EVENT_UNSUPPORTED);
}

static void test_prioritizes_calibration_and_retries_failures(void) {
    MotorCalibrationService service;
    MotorTelemetry telemetry = available_accessory();
    reset_bus();
    motor_calibration_service_init(&service);
    motor_calibration_service_request(&service, MOTOR_CALIBRATION_OPERATION_ERASE);
    motor_calibration_service_request(&service, MOTOR_CALIBRATION_OPERATION_CALIBRATE);

    motor_calibration_service_run(&service, 0, &telemetry);
    assert(transfer_kind == TRANSFER_READ);
    finish_read(0, false);
    motor_calibration_service_run(&service, 0, &telemetry);
    assert(transfer_kind == TRANSFER_READ);
    assert(start_count == 2);

    finish_read(0, true);
    motor_calibration_service_run(&service, 0, &telemetry);
    assert(transfer_data[0] == 0xaa);
    assert(start_count == 3);

    finish_write(true);
    motor_calibration_service_run(&service, 0, &telemetry);
    finish_read(0, true);
    motor_calibration_service_run(&service, 0, &telemetry);
    assert(transfer_kind == TRANSFER_READ);
    assert(start_count == 5);

    finish_read(0, true);
    motor_calibration_service_run(&service, 0, &telemetry);
    assert(transfer_kind == TRANSFER_WRITE);
    assert(transfer_data[0] == 0xbb);
    assert(start_count == 6);
}

int main(void) {
    test_decodes_host_signatures();
    test_rejects_partial_or_malformed_signatures();
    test_calibrates_after_idle_until_zero_response();
    test_erases_in_any_wheel_mode();
    test_rejects_unavailable_operations();
    test_prioritizes_calibration_and_retries_failures();
    return 0;
}
