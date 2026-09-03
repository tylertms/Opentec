#include <assert.h>
#include <stdint.h>
#include <string.h>

#include "transfer/command.h"
#include "wheel/accessory.h"
#include "wheel/accessory_service.h"

static void submit_request(CommandTransport *transport, const uint8_t *expected,
                           uint16_t expected_length) {
    const uint8_t *request;
    uint16_t length;
    assert(command_transport_request(transport, &request, &length));
    assert(length == expected_length);
    assert(memcmp(request, expected, length) == 0);
    assert(command_transport_request_sent(transport));
}

static void complete_read(CommandTransport *transport, const uint8_t *data, uint8_t length) {
    uint8_t response[6] = {1, 0};
    memcpy(response + 2, data, length);
    command_transport_receive(transport, response, (uint16_t)length + 2);
}

static void complete_write(CommandTransport *transport) {
    static const uint8_t accepted[] = {1};
    command_transport_receive(transport, accepted, sizeof(accepted));
}

static void run_read(WheelAccessoryService *service, CommandTransport *transport,
                     const uint8_t *request, uint8_t request_length, const uint8_t *response,
                     uint8_t response_length, uint32_t now_ms) {
    wheel_accessory_service_run_at(service, transport, now_ms);
    submit_request(transport, request, request_length);
    complete_read(transport, response, response_length);
    wheel_accessory_service_run_at(service, transport, now_ms);
}

static void run_write(WheelAccessoryService *service, CommandTransport *transport,
                      const uint8_t *request, uint8_t request_length, uint32_t now_ms) {
    wheel_accessory_service_run_at(service, transport, now_ms);
    submit_request(transport, request, request_length);
    complete_write(transport);
    wheel_accessory_service_run_at(service, transport, now_ms);
}

static void run_prepare(WheelAccessoryService *service, CommandTransport *transport,
                        uint32_t now_ms) {
    wheel_accessory_service_run_at(service, transport, now_ms);
    assert(transport->owner == 0);
    assert(!service->request_pending);
}

static void configure_service(WheelAccessoryService *service) {
    static const WheelAccessorySyncParameters parameters = {
        .sensitivity = 0x7e,
        .force_feedback_strength = 0x22,
        .force_feedback_scale = 0x33,
        .natural_damper = 0x44,
        .natural_friction = 0x5566,
        .natural_inertia = 0x77,
        .interpolation_filter = 0x88,
        .force_effect_intensity = 0x99,
        .force_effect_strength = 0xaa,
        .spring_effect_strength = 0xbb,
        .damper_effect_strength = 0xcc,
    };
    wheel_accessory_service_set_wheel_travel(service, 35520);
    wheel_accessory_service_configure(service, &parameters);
    assert(service->desired_parameters[3] == 0xed);
}

static void initializes_official_parameter_mirror_defaults(void) {
    WheelAccessoryService service;
    wheel_accessory_service_init(&service);
    for (uint8_t index = 0; index < 12; index++) {
        assert(service.mirrored_parameters[index] == 0);
    }
    assert(service.mirrored_parameters[12] == UINT8_MAX);
    assert(service.mirrored_parameters[13] == UINT8_MAX);
    assert(service.mirrored_parameters[14] == UINT8_MAX);
    assert(service.mirrored_model == 0);
}

static void selects_position_modulus_from_previous_mirrored_model(void) {
    WheelAccessoryService service;
    CommandTransport transport;
    wheel_accessory_service_init(&service);
    command_transport_init(&transport);

    const uint8_t status_request[] = {2, 0xe1, 0, 1, 0};
    const uint8_t version_request[] = {2, 0xe1, 1, 4, 0};
    const uint8_t status[] = {0x8a};
    const uint8_t model_two_version[] = {0x34, 0x12, 0x56, 0x78};
    run_read(&service, &transport, status_request, sizeof(status_request), status, sizeof(status),
             0);
    run_read(&service, &transport, version_request, sizeof(version_request), model_two_version,
             sizeof(model_two_version), 0);
    assert(service.mirrored_model == 2);
    assert(wheel_accessory_service_position_modulus(&service) == UINT32_C(0x5c7f));

    wheel_accessory_init(&service.accessory);
    service.probe_requested = true;
    service.version_stage = false;
    service.sync_initialized = false;
    const uint8_t model_zero_status[] = {0x82};
    run_read(&service, &transport, status_request, sizeof(status_request), model_zero_status,
             sizeof(model_zero_status), 0);
    run_read(&service, &transport, version_request, sizeof(version_request), model_two_version,
             sizeof(model_two_version), 0);
    assert(service.mirrored_model == 0);
    assert(wheel_accessory_service_position_modulus(&service) == UINT32_C(0x5d2b));

    service.probe_requested = true;
    service.version_stage = false;
    service.sync_initialized = false;
    const uint8_t unsupported_status[] = {0x83};
    run_read(&service, &transport, status_request, sizeof(status_request), unsupported_status,
             sizeof(unsupported_status), 0);
    run_read(&service, &transport, version_request, sizeof(version_request), model_two_version,
             sizeof(model_two_version), 0);
    assert(wheel_accessory_service_position_modulus(&service) == UINT32_C(0x5d2b));
}

static void polls_identity_and_official_composite_order(void) {
    WheelAccessoryService service;
    CommandTransport transport;
    wheel_accessory_service_init(&service);
    command_transport_init(&transport);
    configure_service(&service);

    const uint8_t status_request[] = {2, 0xe1, 0, 1, 0};
    const uint8_t status[] = {0x8a};
    run_read(&service, &transport, status_request, sizeof(status_request), status, sizeof(status),
             0);
    assert(service.version_stage);

    const uint8_t version_request[] = {2, 0xe1, 1, 4, 0};
    const uint8_t version[] = {0x34, 0x12, 0x56, 0x78};
    run_read(&service, &transport, version_request, sizeof(version_request), version,
             sizeof(version), 0);
    const WheelAccessory *identity = wheel_accessory_service_identity(&service);
    assert(identity->kind == WHEEL_ACCESSORY_EXTENDED);
    assert(identity->model == 2);
    assert(identity->version == UINT32_C(0x78561234));
    assert(wheel_accessory_service_input_transfer_code(&service) == 0x34);
    assert(wheel_accessory_service_position_modulus(&service) == UINT32_C(0x5c7f));

    const uint8_t natural_damper_request[] = {2, 0xe1, 0x23, 1, 0};
    const uint8_t natural_damper[] = {0x80};
    run_read(&service, &transport, natural_damper_request, sizeof(natural_damper_request),
             natural_damper, sizeof(natural_damper), 0);
    const uint8_t motor_temperature_request[] = {2, 0xe1, 0x12, 2, 0};
    const uint8_t motor_temperature[] = {0x9c, 0xff};
    run_read(&service, &transport, motor_temperature_request, sizeof(motor_temperature_request),
             motor_temperature, sizeof(motor_temperature), 0);
    const uint8_t driver_temperature_request[] = {2, 0xe1, 0x13, 2, 0};
    const uint8_t driver_temperature[] = {25, 0};
    run_read(&service, &transport, driver_temperature_request, sizeof(driver_temperature_request),
             driver_temperature, sizeof(driver_temperature), 0);
    const uint8_t runtime_request[] = {2, 0xe1, 0x11, 4, 0};
    const uint8_t runtime_response[] = {0x78, 0x56, 0x34, 0x12};
    run_read(&service, &transport, runtime_request, sizeof(runtime_request), runtime_response,
             sizeof(runtime_response), 0);
    const uint8_t type_request[] = {2, 0xe1, 7, 1, 0};
    const uint8_t type[] = {0x23};
    run_read(&service, &transport, type_request, sizeof(type_request), type, sizeof(type), 0);

    const uint8_t friction_write[] = {2, 0xe0, 0x24, 0x66, 0x55};
    run_write(&service, &transport, friction_write, sizeof(friction_write), 0);
    const uint8_t interpolation_write[] = {2, 0xe0, 0x26, 0x88};
    run_write(&service, &transport, interpolation_write, sizeof(interpolation_write), 0);
    const uint8_t motor_command_request[] = {2, 0xe1, 5, 2, 0};
    const uint8_t motor_command[] = {0xff, 0xff};
    run_read(&service, &transport, motor_command_request, sizeof(motor_command_request),
             motor_command, sizeof(motor_command), 0);
    const uint8_t natural_damper_write[] = {2, 0xe0, 0x23, 0x44};
    run_write(&service, &transport, natural_damper_write, sizeof(natural_damper_write), 0);
    const uint8_t inertia_write[] = {2, 0xe0, 0x25, 0x77};
    run_write(&service, &transport, inertia_write, sizeof(inertia_write), 0);
    const uint8_t scale_write[] = {2, 0xe0, 0x22, 0x33};
    run_write(&service, &transport, scale_write, sizeof(scale_write), 0);

    run_prepare(&service, &transport, 0);
    const uint8_t status_write[] = {2, 0xe0, 4, 0};
    run_write(&service, &transport, status_write, sizeof(status_write), 0);
    const uint8_t synchronized_status_request[] = {2, 0xe1, 4, 1, 0};
    const uint8_t synchronized_status[] = {0xaa};
    run_read(&service, &transport, synchronized_status_request, sizeof(synchronized_status_request),
             synchronized_status, sizeof(synchronized_status), 0);
    run_prepare(&service, &transport, 0);
    assert(wheel_accessory_service_output_inhibited(&service));
    run_prepare(&service, &transport, 0);
    const uint8_t sensitivity_write[] = {2, 0xe0, 0x20, 0xed};
    run_write(&service, &transport, sensitivity_write, sizeof(sensitivity_write), 0);
    const uint8_t strength_write[] = {2, 0xe0, 0x21, 0x22};
    run_write(&service, &transport, strength_write, sizeof(strength_write), 0);
    const uint8_t intensity_write[] = {2, 0xe0, 0x27, 0x99};
    run_write(&service, &transport, intensity_write, sizeof(intensity_write), 0);
    const uint8_t effect_strength_write[] = {2, 0xe0, 0x28, 0xaa};
    run_write(&service, &transport, effect_strength_write, sizeof(effect_strength_write), 0);
    const uint8_t spring_write[] = {2, 0xe0, 0x29, 0xbb};
    run_write(&service, &transport, spring_write, sizeof(spring_write), 0);
    const uint8_t damper_effect_write[] = {2, 0xe0, 0x2a, 0xcc};
    run_write(&service, &transport, damper_effect_write, sizeof(damper_effect_write), 0);
    assert(service.sync_state == WHEEL_ACCESSORY_SYNC_WAIT_CYCLE);
    assert(service.dirty_parameters == 0);
    int16_t temperature;
    assert(wheel_accessory_service_motor_temperature(&service, &temperature));
    assert(temperature == -100);
    assert(wheel_accessory_service_driver_temperature(&service, &temperature));
    assert(temperature == 25);
    uint32_t runtime_seconds;
    assert(wheel_accessory_service_runtime(&service, &runtime_seconds));
    assert(runtime_seconds == UINT32_C(0x12345678));

    run_prepare(&service, &transport, 199);
    wheel_accessory_service_run_at(&service, &transport, 200);
    const uint8_t *request;
    uint16_t request_length;
    assert(service.sync_state == WHEEL_ACCESSORY_SYNC_READ_NATURAL_DAMPER);
    assert(!command_transport_request(&transport, &request, &request_length));
    wheel_accessory_service_run_at(&service, &transport, 200);
    assert(command_transport_request(&transport, &request, &request_length));
    assert(request_length == sizeof(natural_damper_request));
    assert(memcmp(request, natural_damper_request, request_length) == 0);
}

static void preserves_tuning_updates_during_an_inflight_write(void) {
    WheelAccessoryService service;
    CommandTransport transport;
    wheel_accessory_service_init(&service);
    command_transport_init(&transport);
    assert(wheel_accessory_apply_probe(&service.accessory, (int8_t)0x81, 1));
    service.sync_initialized = true;
    service.probe_requested = false;
    service.sync_state = WHEEL_ACCESSORY_SYNC_PREPARE_NATURAL_FRICTION;
    service.desired_parameters[7] = 0x11;
    service.desired_parameters[8] = 0x22;
    service.dirty_parameters = 1u << 6;

    wheel_accessory_service_run(&service, &transport);
    const uint8_t first_write[] = {2, 0xe0, 0x24, 0x11, 0x22};
    submit_request(&transport, first_write, sizeof(first_write));
    assert(service.sync_request);

    wheel_accessory_service_configure(
        &service, &(WheelAccessorySyncParameters){.natural_friction = 0x4433});
    complete_write(&transport);
    wheel_accessory_service_run(&service, &transport);

    assert(service.mirrored_parameters[7] == 0x11);
    assert(service.mirrored_parameters[8] == 0x22);
    assert(service.desired_parameters[7] == 0x33);
    assert(service.desired_parameters[8] == 0x44);
    assert((service.dirty_parameters & (1u << 6)) != 0);
    assert(!service.sync_request);
}

static void retries_calibration_and_override_without_losing_state(void) {
    WheelAccessoryService service;
    CommandTransport transport;
    wheel_accessory_service_init(&service);
    command_transport_init(&transport);
    assert(wheel_accessory_apply_probe(&service.accessory, (int8_t)0x81, 1));
    service.sync_initialized = true;
    service.probe_requested = false;
    service.sync_state = WHEEL_ACCESSORY_SYNC_READ_NATURAL_DAMPER;

    wheel_accessory_service_run(&service, &transport);
    assert(command_transport_request_sent(&transport));
    command_transport_fail(&transport);
    wheel_accessory_service_run(&service, &transport);
    assert(service.sync_state == WHEEL_ACCESSORY_SYNC_READ_NATURAL_DAMPER);
    wheel_accessory_service_run(&service, &transport);
    const uint8_t *request;
    uint16_t request_length;
    assert(command_transport_request(&transport, &request, &request_length));
    assert(request_length == 5);
    assert(request[2] == 0x23);
    const uint8_t natural_damper[] = {0x44};
    complete_read(&transport, natural_damper, sizeof(natural_damper));
    wheel_accessory_service_run(&service, &transport);

    wheel_accessory_service_set_wheel_mode(&service, 0);
    wheel_accessory_service_request_calibration(&service, MOTOR_CALIBRATION_OPERATION_CALIBRATE);
    wheel_accessory_service_request_calibration(&service, MOTOR_CALIBRATION_OPERATION_ERASE);
    service.accessory.accessory_type = 1;
    service.sync_state = WHEEL_ACCESSORY_SYNC_READ_CALIBRATION_COMMAND;
    wheel_accessory_service_run(&service, &transport);
    const uint8_t calibration_read[] = {2, 0xe1, 6, 2, 0};
    submit_request(&transport, calibration_read, sizeof(calibration_read));
    const uint8_t idle[] = {0, 0};
    complete_read(&transport, idle, sizeof(idle));
    wheel_accessory_service_run(&service, &transport);
    wheel_accessory_service_run(&service, &transport);
    const uint8_t calibration_write[] = {2, 0xe0, 6, 0xaa, 0xaa};
    submit_request(&transport, calibration_write, sizeof(calibration_write));
    complete_write(&transport);
    wheel_accessory_service_run(&service, &transport);
    assert(wheel_accessory_service_take_calibration_event(&service) == MOTOR_CALIBRATION_EVENT_NONE);
    service.sync_state = WHEEL_ACCESSORY_SYNC_READ_CALIBRATION_COMMAND;
    wheel_accessory_service_run(&service, &transport);
    submit_request(&transport, calibration_read, sizeof(calibration_read));
    complete_read(&transport, idle, sizeof(idle));
    wheel_accessory_service_run(&service, &transport);
    assert(wheel_accessory_service_take_calibration_event(&service) ==
           MOTOR_CALIBRATION_EVENT_COMPLETED);
    assert(service.calibration_requests == 0);

    wheel_accessory_service_configure(
        &service, &(WheelAccessorySyncParameters){.natural_damper = 0x44, .sensitivity = 0});
    service.sync_initialized = false;
    wheel_accessory_service_set_output_override(&service, true);
    wheel_accessory_service_run(&service, &transport);
    const uint8_t override_write[] = {2, 0xe0, 0x23, 0xff};
    submit_request(&transport, override_write, sizeof(override_write));
    complete_write(&transport);
    wheel_accessory_service_run(&service, &transport);
    assert(wheel_accessory_service_output_override_active(&service));
    assert(wheel_accessory_service_output_override_complete(&service));
    wheel_accessory_service_set_output_override(&service, false);
    wheel_accessory_service_run(&service, &transport);
    const uint8_t restore_write[] = {2, 0xe0, 0x23, 0x44};
    submit_request(&transport, restore_write, sizeof(restore_write));
    complete_write(&transport);
    wheel_accessory_service_run(&service, &transport);
    assert(!wheel_accessory_service_output_override_active(&service));
}

static void releases_calibration_ownership_after_failed_transfer(void) {
    WheelAccessoryService service;
    CommandTransport transport;
    wheel_accessory_service_init(&service);
    command_transport_init(&transport);
    assert(wheel_accessory_apply_probe(&service.accessory, (int8_t)0x81, 1));
    service.sync_initialized = true;
    service.probe_requested = false;
    service.accessory.accessory_type = 1;
    service.sync_state = WHEEL_ACCESSORY_SYNC_READ_CALIBRATION_COMMAND;
    wheel_accessory_service_request_calibration(&service,
                                                MOTOR_CALIBRATION_OPERATION_CALIBRATE);

    wheel_accessory_service_run(&service, &transport);
    assert(wheel_accessory_service_calibration_owns_transport(&service));
    assert(command_transport_request_sent(&transport));
    command_transport_fail(&transport);
    wheel_accessory_service_run(&service, &transport);

    assert(!wheel_accessory_service_calibration_owns_transport(&service));
    assert(!wheel_accessory_service_calibration_active(&service));
    assert(wheel_accessory_service_calibration_pending(&service));
    assert(service.transfer == 0);
}

static void applies_output_override_gate_and_state_sequence(void) {
    WheelAccessoryService service;
    wheel_accessory_service_init(&service);
    service.desired_parameters[6] = 0x44;
    service.drift_mode = 0x12;
    wheel_accessory_service_set_output_override(&service, true);
    assert(!service.output_override_requested);
    assert(service.desired_parameters[6] == 0x44);
    assert(service.drift_mode == 0x12);

    service.accessory.kind = WHEEL_ACCESSORY_STANDARD;
    service.sync_initialized = true;
    service.probe_requested = false;
    service.remote_effects_enabled = true;
    wheel_accessory_service_set_output_override(&service, true);
    assert(service.output_override_requested);
    assert(service.saved_natural_damper == 0x44);
    assert(service.saved_drift_mode == 0x12);
    assert(service.drift_mode == 0xfb);
    assert(service.desired_parameters[6] == UINT8_MAX);
    assert(!wheel_accessory_service_remote_effects_enabled(&service));

    wheel_accessory_service_set_output_override(&service, false);
    assert(!service.output_override_requested);
    assert(service.output_override_restore_pending);
    assert(service.desired_parameters[6] == 0x44);

    service.accessory.kind = WHEEL_ACCESSORY_LEGACY;
    service.output_override_active = true;
    service.output_override_complete = true;
    service.output_override_restore_pending = false;
    wheel_accessory_service_set_output_override(&service, false);
    assert(service.output_override_active);
    assert(service.output_override_complete);
}

static void probes_before_pending_output_override(void) {
    WheelAccessoryService service;
    CommandTransport transport;
    wheel_accessory_service_init(&service);
    command_transport_init(&transport);
    service.accessory.kind = WHEEL_ACCESSORY_STANDARD;
    service.sync_initialized = true;
    service.output_override_requested = true;
    service.output_override_value = UINT8_MAX;

    wheel_accessory_service_run(&service, &transport);
    const uint8_t status_request[] = {2, 0xe1, 0, 1, 0};
    submit_request(&transport, status_request, sizeof(status_request));
}

static void holds_composite_sync_while_output_override_is_active(void) {
    WheelAccessoryService service;
    CommandTransport transport;
    wheel_accessory_service_init(&service);
    command_transport_init(&transport);
    service.accessory.kind = WHEEL_ACCESSORY_STANDARD;
    service.sync_initialized = true;
    service.probe_requested = false;
    service.sync_state = WHEEL_ACCESSORY_SYNC_PREPARE_NATURAL_FRICTION;
    service.desired_parameters[7] = 1;
    service.dirty_parameters = 1u << 6;
    service.output_override_requested = true;
    service.output_override_active = true;
    service.output_override_complete = true;

    wheel_accessory_service_run(&service, &transport);

    const uint8_t *request;
    uint16_t request_length;
    assert(!command_transport_request(&transport, &request, &request_length));
    assert(service.sync_state == WHEEL_ACCESSORY_SYNC_PREPARE_NATURAL_FRICTION);
    assert(transport.owner == 0);
}

static void retains_unavailable_calibrations_before_composite_sync(void) {
    WheelAccessoryService service;
    CommandTransport transport;
    wheel_accessory_service_init(&service);
    command_transport_init(&transport);
    assert(wheel_accessory_apply_probe(&service.accessory, (int8_t)0x8a, 1));
    service.sync_initialized = true;
    service.probe_requested = false;
    service.sync_state = WHEEL_ACCESSORY_SYNC_READ_ACCESSORY_TYPE;
    wheel_accessory_service_set_wheel_mode(&service, 0);
    wheel_accessory_service_request_calibration(&service, MOTOR_CALIBRATION_OPERATION_CALIBRATE);
    wheel_accessory_service_request_calibration(&service, MOTOR_CALIBRATION_OPERATION_ERASE);

    const uint8_t type_request[] = {2, 0xe1, 7, 1, 0};
    const uint8_t unavailable_type[] = {UINT8_MAX};
    run_read(&service, &transport, type_request, sizeof(type_request), unavailable_type,
             sizeof(unavailable_type), 0);
    assert(service.sync_state == WHEEL_ACCESSORY_SYNC_PREPARE_NATURAL_FRICTION);
    assert(service.calibration_requests == (1u << 0 | 1u << 1));
    assert(!service.calibration_command_sent);
    assert(wheel_accessory_service_take_calibration_event(&service) == MOTOR_CALIBRATION_EVENT_NONE);

    run_prepare(&service, &transport, 0);
    assert(service.calibration_requests == (1u << 0 | 1u << 1));
    assert(wheel_accessory_service_take_calibration_event(&service) == MOTOR_CALIBRATION_EVENT_NONE);

    assert(wheel_accessory_apply_probe(&service.accessory, (int8_t)0x88, 1));
    service.sync_state = WHEEL_ACCESSORY_SYNC_READ_ACCESSORY_TYPE;
    run_prepare(&service, &transport, 0);
    assert(service.sync_state == WHEEL_ACCESSORY_SYNC_PREPARE_NATURAL_FRICTION);
    assert(service.calibration_requests == (1u << 0 | 1u << 1));
    assert(wheel_accessory_service_take_calibration_event(&service) == MOTOR_CALIBRATION_EVENT_NONE);
}

static void starts_motor_only_after_an_idle_command(void) {
    WheelAccessoryService service;
    CommandTransport transport;
    wheel_accessory_service_init(&service);
    command_transport_init(&transport);
    assert(wheel_accessory_apply_probe(&service.accessory, (int8_t)0x81, 1));
    service.sync_initialized = true;
    service.probe_requested = false;
    service.sync_state = WHEEL_ACCESSORY_SYNC_READ_MOTOR_COMMAND;
    wheel_accessory_service_request_motor_start(&service);

    const uint8_t motor_command_request[] = {2, 0xe1, 5, 2, 0};
    const uint8_t idle[] = {0, 0};
    run_read(&service, &transport, motor_command_request, sizeof(motor_command_request), idle,
             sizeof(idle), 0);
    assert(service.sync_state == WHEEL_ACCESSORY_SYNC_WRITE_MOTOR_START);
    assert(wheel_accessory_service_take_motor_event(&service) ==
           MOTOR_STATUS_EVENT_POSITION_SENSOR_TEST_STARTED);

    const uint8_t motor_start_write[] = {2, 0xe0, 5, 0xcd, 0xab};
    run_write(&service, &transport, motor_start_write, sizeof(motor_start_write), 0);
    assert(service.motor_command_sent);
    assert(service.motor_start_pending);
    assert(service.sync_state == WHEEL_ACCESSORY_SYNC_PREPARE_NATURAL_DAMPER);

    service.sync_state = WHEEL_ACCESSORY_SYNC_READ_MOTOR_COMMAND;
    run_read(&service, &transport, motor_command_request, sizeof(motor_command_request), idle,
             sizeof(idle), 0);
    assert(!service.motor_command_sent);
    assert(!service.motor_start_pending);
    assert(wheel_accessory_service_take_motor_event(&service) ==
           MOTOR_STATUS_EVENT_POSITION_SENSOR_TEST_SUCCEEDED);
    assert(wheel_accessory_service_take_motor_event(&service) == MOTOR_STATUS_EVENT_NONE);

    service.sync_state = WHEEL_ACCESSORY_SYNC_READ_MOTOR_COMMAND;
    const uint8_t failed[] = {0xbb, 0xbb};
    run_read(&service, &transport, motor_command_request, sizeof(motor_command_request), failed,
             sizeof(failed), 0);
    assert(wheel_accessory_service_output_inhibited(&service));
    assert(wheel_accessory_service_take_motor_event(&service) ==
           MOTOR_STATUS_EVENT_POSITION_SENSOR_TEST_FAILED);
}

static void handles_unavailable_services(void) {
    WheelAccessoryService service;
    CommandTransport transport;
    wheel_accessory_service_init(&service);
    command_transport_init(&transport);
    wheel_accessory_service_run(NULL, &transport);
    wheel_accessory_service_run(&service, NULL);
    assert(wheel_accessory_service_identity(NULL) == NULL);
    service.accessory.version = UINT32_C(0x12345678);
    assert(wheel_accessory_service_input_transfer_code(&service) == 0);
    service.accessory.kind = WHEEL_ACCESSORY_LEGACY;
    assert(wheel_accessory_service_input_transfer_code(&service) == 0);
    assert(wheel_accessory_service_input_transfer_code(NULL) == 0);
    assert(!wheel_accessory_service_remote_effects_enabled(NULL));
    assert(!wheel_accessory_service_motor_temperature(NULL, NULL));
    assert(!wheel_accessory_service_output_override_active(NULL));
}

int main(void) {
    initializes_official_parameter_mirror_defaults();
    selects_position_modulus_from_previous_mirrored_model();
    polls_identity_and_official_composite_order();
    preserves_tuning_updates_during_an_inflight_write();
    retries_calibration_and_override_without_losing_state();
    releases_calibration_ownership_after_failed_transfer();
    applies_output_override_gate_and_state_sequence();
    probes_before_pending_output_override();
    holds_composite_sync_while_output_override_is_active();
    retains_unavailable_calibrations_before_composite_sync();
    starts_motor_only_after_an_idle_command();
    handles_unavailable_services();
    return 0;
}
