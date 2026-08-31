#include <assert.h>
#include <stdint.h>
#include <string.h>

#include "motor/command_application.h"
#include "motor/command_message.h"

static void test_applies_information(void) {
    static const uint8_t data[] = {0x12, 0x34};
    MotorCommandMessage message = {
        .kind = MOTOR_COMMAND_MESSAGE_INFORMATION,
        .selector = 3,
        .data = data,
        .data_length = sizeof(data),
    };
    MotorCommandApplication application;
    motor_command_application_init(&application);

    MotorCommandApplicationEvent event = motor_command_application_apply(&application, &message);

    assert(event.result == MOTOR_COMMAND_APPLICATION_INFORMATION);
    assert(application.information.selector_3 == 0x1234);
}

static void test_derives_calibration_digest(void) {
    static const uint8_t data[20] = {
        0x22, 0x44, 0x10, 0x80, 0x12, 0x34, 0x56, 0x78, 0x9a, 0xbc,
        0xde, 0xf0, 0x11, 0x22, 0x33, 0x44, 0,    0,    0,    0,
    };
    static const uint8_t expected[] = {0x99, 0xaa, 0xbb, 0xcc, 0x01, 0xa2, 0, 0};
    MotorCommandMessage message = {
        .kind = MOTOR_COMMAND_MESSAGE_CALIBRATION,
        .data = data,
        .data_length = sizeof(data),
    };
    MotorCommandApplication application;
    motor_command_application_init(&application);

    MotorCommandApplicationEvent event = motor_command_application_apply(&application, &message);

    assert(event.result == MOTOR_COMMAND_APPLICATION_CALIBRATION);
    assert(memcmp(application.digest, expected, sizeof(expected)) == 0);
}

static void test_exposes_forwarded_responses(void) {
    static const uint8_t information_payload[25] = {0x85, 0, 2, 0, 20};
    static const uint8_t vendor_data[] = {0xc2, 0x12, 0x34};
    MotorCommandApplication application;
    motor_command_application_init(&application);
    MotorCommandMessage message = {
        .kind = MOTOR_COMMAND_MESSAGE_INFORMATION,
        .selector = 2,
        .payload = information_payload,
        .payload_length = sizeof(information_payload),
        .data = information_payload + 5,
        .data_length = sizeof(information_payload) - 5,
    };

    MotorCommandApplicationEvent event = motor_command_application_apply(&application, &message);
    assert(event.result == MOTOR_COMMAND_APPLICATION_FORWARD);
    assert(event.forward_data == information_payload);
    assert(event.forward_length == sizeof(information_payload));

    message = (MotorCommandMessage){
        .kind = MOTOR_COMMAND_MESSAGE_VENDOR_FINAL,
        .payload = vendor_data,
        .payload_length = sizeof(vendor_data),
        .data = vendor_data,
        .data_length = sizeof(vendor_data),
    };
    event = motor_command_application_apply(&application, &message);
    assert(event.result == MOTOR_COMMAND_APPLICATION_FORWARD);
    assert(event.forward_data == vendor_data);
    assert(event.forward_length == sizeof(vendor_data));
}

static void test_rejects_invalid_messages(void) {
    static const uint8_t short_calibration[16] = {0};
    MotorCommandApplication application;
    motor_command_application_init(&application);
    MotorCommandMessage message = {
        .kind = MOTOR_COMMAND_MESSAGE_CALIBRATION,
        .data = short_calibration,
        .data_length = sizeof(short_calibration),
    };

    assert(motor_command_application_apply(&application, &message).result ==
           MOTOR_COMMAND_APPLICATION_INVALID);
    assert(motor_command_application_apply(NULL, &message).result ==
           MOTOR_COMMAND_APPLICATION_INVALID);
    assert(motor_command_application_apply(&application, NULL).result ==
           MOTOR_COMMAND_APPLICATION_INVALID);

    message.data = NULL;
    message.data_length = 20;
    assert(motor_command_application_apply(&application, &message).result ==
           MOTOR_COMMAND_APPLICATION_INVALID);

    message = (MotorCommandMessage){
        .kind = MOTOR_COMMAND_MESSAGE_INFORMATION,
        .selector = UINT8_MAX,
    };
    assert(motor_command_application_apply(&application, &message).result ==
           MOTOR_COMMAND_APPLICATION_INVALID);

    message = (MotorCommandMessage){
        .kind = MOTOR_COMMAND_MESSAGE_VENDOR_CONTINUATION,
    };
    assert(motor_command_application_apply(&application, &message).result ==
           MOTOR_COMMAND_APPLICATION_INVALID);

    message.kind = (MotorCommandMessageKind)UINT8_MAX;
    assert(motor_command_application_apply(&application, &message).result ==
           MOTOR_COMMAND_APPLICATION_INVALID);
}

int main(void) {
    test_applies_information();
    test_derives_calibration_digest();
    test_exposes_forwarded_responses();
    test_rejects_invalid_messages();
    return 0;
}
