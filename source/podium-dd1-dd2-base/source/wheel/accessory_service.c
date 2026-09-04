#include "wheel/accessory_service.h"

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "platform/time.h"

enum {
    WHEEL_ACCESSORY_SERVICE_OWNER = 0x44,
    WHEEL_ACCESSORY_TARGET = 0xf0,
    WHEEL_ACCESSORY_STATUS_OFFSET = 0,
    WHEEL_ACCESSORY_VERSION_OFFSET = 1,
    WHEEL_ACCESSORY_HANDSHAKE_OFFSET = 3,
    WHEEL_ACCESSORY_STATUS_RESPONSE_OFFSET = 4,
    WHEEL_ACCESSORY_MOTOR_COMMAND_OFFSET = 5,
    WHEEL_ACCESSORY_CALIBRATION_OFFSET = 6,
    WHEEL_ACCESSORY_TYPE_OFFSET = 7,
    WHEEL_ACCESSORY_RUNTIME_OFFSET = 0x11,
    WHEEL_ACCESSORY_MOTOR_TEMPERATURE_OFFSET = 0x12,
    WHEEL_ACCESSORY_DRIVER_TEMPERATURE_OFFSET = 0x13,
    WHEEL_ACCESSORY_SENSITIVITY_OFFSET = 0x20,
    WHEEL_ACCESSORY_FORCE_STRENGTH_OFFSET = 0x21,
    WHEEL_ACCESSORY_FORCE_SCALE_OFFSET = 0x22,
    WHEEL_ACCESSORY_NATURAL_DAMPER_OFFSET = 0x23,
    WHEEL_ACCESSORY_NATURAL_FRICTION_OFFSET = 0x24,
    WHEEL_ACCESSORY_NATURAL_INERTIA_OFFSET = 0x25,
    WHEEL_ACCESSORY_INTERPOLATION_OFFSET = 0x26,
    WHEEL_ACCESSORY_FORCE_EFFECT_INTENSITY_OFFSET = 0x27,
    WHEEL_ACCESSORY_FORCE_EFFECT_STRENGTH_OFFSET = 0x28,
    WHEEL_ACCESSORY_SPRING_EFFECT_STRENGTH_OFFSET = 0x29,
    WHEEL_ACCESSORY_DAMPER_EFFECT_STRENGTH_OFFSET = 0x2a,
    WHEEL_ACCESSORY_SYNC_PARAMETER_COUNT = 13,
    WHEEL_ACCESSORY_CALIBRATION_REQUEST = 1u << 0,
    WHEEL_ACCESSORY_ERASE_REQUEST = 1u << 1,
    WHEEL_ACCESSORY_CALIBRATION_VALUE = 0xaa,
    WHEEL_ACCESSORY_ERASE_VALUE = 0xbb,
    WHEEL_ACCESSORY_HANDSHAKE_VALUE = 0x05fa,
    WHEEL_ACCESSORY_AUTO_SENSITIVITY = 0x7e,
    WHEEL_ACCESSORY_OUTPUT_OVERRIDE_TUNING_VALUE = 0x64,
    WHEEL_ACCESSORY_OUTPUT_OVERRIDE_VALUE = 0xff,
    WHEEL_ACCESSORY_POSITION_MODULUS_LOW = 0x5c7f,
    WHEEL_ACCESSORY_POSITION_MODULUS_DEFAULT = 0x5d2b,
    WHEEL_ACCESSORY_WAIT_CYCLE_MS = 200, /**< Official delay after a routed composite cycle. */
};

typedef enum {
    WHEEL_ACCESSORY_TRANSFER_NONE,
    WHEEL_ACCESSORY_TRANSFER_PROBE_STATUS,
    WHEEL_ACCESSORY_TRANSFER_PROBE_VERSION,
    WHEEL_ACCESSORY_TRANSFER_ACCESSORY_TYPE,
    WHEEL_ACCESSORY_TRANSFER_NATURAL_DAMPER_READ,
    WHEEL_ACCESSORY_TRANSFER_MOTOR_TEMPERATURE,
    WHEEL_ACCESSORY_TRANSFER_DRIVER_TEMPERATURE,
    WHEEL_ACCESSORY_TRANSFER_RUNTIME,
    WHEEL_ACCESSORY_TRANSFER_CALIBRATION_READ,
    WHEEL_ACCESSORY_TRANSFER_CALIBRATION_WRITE,
    WHEEL_ACCESSORY_TRANSFER_MOTOR_COMMAND_READ,
    WHEEL_ACCESSORY_TRANSFER_MOTOR_COMMAND_WRITE,
    WHEEL_ACCESSORY_TRANSFER_PARAMETER_WRITE,
    WHEEL_ACCESSORY_TRANSFER_STATUS_WRITE,
    WHEEL_ACCESSORY_TRANSFER_STATUS_READ,
    WHEEL_ACCESSORY_TRANSFER_HANDSHAKE_WRITE,
    WHEEL_ACCESSORY_TRANSFER_OUTPUT_OVERRIDE_WRITE,
} WheelAccessoryTransfer;

typedef struct {
    uint8_t offset;
    uint8_t data_offset;
    uint8_t length;
} WheelAccessoryParameter;

static const WheelAccessoryParameter parameters[WHEEL_ACCESSORY_SYNC_PARAMETER_COUNT] = {
    {WHEEL_ACCESSORY_STATUS_RESPONSE_OFFSET, 0, 1},
    {WHEEL_ACCESSORY_HANDSHAKE_OFFSET, 1, 2},
    {WHEEL_ACCESSORY_SENSITIVITY_OFFSET, 3, 1},
    {WHEEL_ACCESSORY_FORCE_STRENGTH_OFFSET, 4, 1},
    {WHEEL_ACCESSORY_FORCE_SCALE_OFFSET, 5, 1},
    {WHEEL_ACCESSORY_NATURAL_DAMPER_OFFSET, 6, 1},
    {WHEEL_ACCESSORY_NATURAL_FRICTION_OFFSET, 7, 2},
    {WHEEL_ACCESSORY_NATURAL_INERTIA_OFFSET, 9, 1},
    {WHEEL_ACCESSORY_INTERPOLATION_OFFSET, 10, 1},
    {WHEEL_ACCESSORY_FORCE_EFFECT_INTENSITY_OFFSET, 11, 1},
    {WHEEL_ACCESSORY_FORCE_EFFECT_STRENGTH_OFFSET, 12, 1},
    {WHEEL_ACCESSORY_SPRING_EFFECT_STRENGTH_OFFSET, 13, 1},
    {WHEEL_ACCESSORY_DAMPER_EFFECT_STRENGTH_OFFSET, 14, 1},
};

static void reset_mirrored_parameters(WheelAccessoryService *service) {
    memset(service->mirrored_parameters, 0, sizeof(service->mirrored_parameters));
    service->mirrored_parameters[12] = UINT8_MAX;
    service->mirrored_parameters[13] = UINT8_MAX;
    service->mirrored_parameters[14] = UINT8_MAX;
}

/**
 * @brief Returns the steering-position modulus for a mirrored accessory model.
 *
 * The first extended probe uses the model retained by the previous completed probe. The current
 * model is latched only after its modulus has been selected.
 *
 * @param[in] model Mirrored accessory model byte.
 * @return Steering-position modulus encoded by the model family.
 */
static uint32_t position_modulus_for_model(uint8_t model) {
    return (model & 0x02u) == 0 ? WHEEL_ACCESSORY_POSITION_MODULUS_LOW
                                : WHEEL_ACCESSORY_POSITION_MODULUS_DEFAULT;
}

static uint16_t decode_u16(const uint8_t data[2]) {
    return (uint16_t)data[0] | (uint16_t)data[1] << 8;
}

static uint32_t decode_u32(const uint8_t data[4]) {
    return (uint32_t)data[0] | (uint32_t)data[1] << 8 | (uint32_t)data[2] << 16 |
           (uint32_t)data[3] << 24;
}

static uint8_t calibration_request_bit(MotorCalibrationOperation operation) {
    return operation == MOTOR_CALIBRATION_OPERATION_CALIBRATE ? WHEEL_ACCESSORY_CALIBRATION_REQUEST
                                                              : WHEEL_ACCESSORY_ERASE_REQUEST;
}

static bool accessory_supports_composite(const WheelAccessoryService *service) {
    return service->accessory.kind != WHEEL_ACCESSORY_DISCONNECTED;
}

static bool accessory_supports_extended(const WheelAccessoryService *service) {
    return service->accessory.kind == WHEEL_ACCESSORY_EXTENDED;
}

static bool accessory_supports_output_override(const WheelAccessoryService *service) {
    return service->accessory.kind == WHEEL_ACCESSORY_STANDARD ||
           service->accessory.kind == WHEEL_ACCESSORY_EXTENDED;
}

static bool queue_sync_state(WheelAccessoryService *service, CommandTransport *transport,
                             uint32_t now_ms);

static bool parameter_needs_sync(const WheelAccessoryService *service, uint8_t index) {
    const WheelAccessoryParameter *parameter = &parameters[index];
    return (service->dirty_parameters & (uint16_t)(1u << index)) != 0 ||
           memcmp(service->desired_parameters + parameter->data_offset,
                  service->mirrored_parameters + parameter->data_offset, parameter->length) != 0;
}

static uint8_t encode_automatic_sensitivity(uint32_t wheel_travel_limit) {
    uint32_t sensitivity = wheel_travel_limit / 10u;
    sensitivity = (sensitivity * 2520u) / 82880u;
    return (uint8_t)(sensitivity + 0x81u);
}

static void refresh_sensitivity(WheelAccessoryService *service) {
    uint8_t encoded = service->requested_sensitivity == WHEEL_ACCESSORY_AUTO_SENSITIVITY
                          ? encode_automatic_sensitivity(service->wheel_travel_limit)
                          : service->requested_sensitivity;
    if (service->desired_parameters[3] != encoded) {
        service->desired_parameters[3] = encoded;
        service->dirty_parameters |= (uint16_t)(1u << 2);
    }
}

static void set_next_after_parameter(WheelAccessoryService *service,
                                     WheelAccessorySyncState state) {
    switch (state) {
    case WHEEL_ACCESSORY_SYNC_WRITE_NATURAL_FRICTION:
        service->sync_state = WHEEL_ACCESSORY_SYNC_PREPARE_INTERPOLATION_FILTER;
        break;
    case WHEEL_ACCESSORY_SYNC_WRITE_INTERPOLATION_FILTER:
        service->sync_state = WHEEL_ACCESSORY_SYNC_READ_MOTOR_COMMAND;
        break;
    case WHEEL_ACCESSORY_SYNC_WRITE_MOTOR_START:
        service->sync_state = WHEEL_ACCESSORY_SYNC_PREPARE_NATURAL_DAMPER;
        break;
    case WHEEL_ACCESSORY_SYNC_WRITE_NATURAL_DAMPER:
        service->sync_state = WHEEL_ACCESSORY_SYNC_PREPARE_NATURAL_INERTIA;
        break;
    case WHEEL_ACCESSORY_SYNC_WRITE_NATURAL_INERTIA:
        service->sync_state = WHEEL_ACCESSORY_SYNC_PREPARE_FORCE_FEEDBACK_SCALE;
        break;
    case WHEEL_ACCESSORY_SYNC_WRITE_FORCE_FEEDBACK_SCALE:
        service->sync_state = WHEEL_ACCESSORY_SYNC_ROUTE_STATUS;
        break;
    case WHEEL_ACCESSORY_SYNC_WRITE_SENSITIVITY:
        service->sync_state = WHEEL_ACCESSORY_SYNC_PREPARE_FORCE_FEEDBACK_STRENGTH;
        break;
    case WHEEL_ACCESSORY_SYNC_WRITE_FORCE_FEEDBACK_STRENGTH:
        service->sync_state = WHEEL_ACCESSORY_SYNC_PREPARE_FORCE_EFFECT_INTENSITY;
        break;
    case WHEEL_ACCESSORY_SYNC_WRITE_FORCE_EFFECT_INTENSITY:
        service->sync_state = WHEEL_ACCESSORY_SYNC_PREPARE_FORCE_EFFECT_STRENGTH;
        break;
    case WHEEL_ACCESSORY_SYNC_WRITE_FORCE_EFFECT_STRENGTH:
        service->sync_state = WHEEL_ACCESSORY_SYNC_PREPARE_SPRING_EFFECT_STRENGTH;
        break;
    case WHEEL_ACCESSORY_SYNC_WRITE_SPRING_EFFECT_STRENGTH:
        service->sync_state = WHEEL_ACCESSORY_SYNC_PREPARE_DAMPER_EFFECT_STRENGTH;
        break;
    case WHEEL_ACCESSORY_SYNC_WRITE_DAMPER_EFFECT_STRENGTH:
        service->sync_state = WHEEL_ACCESSORY_SYNC_WAIT_CYCLE;
        break;
    default:
        break;
    }
}

static uint8_t parameter_index_for_state(WheelAccessorySyncState state) {
    switch (state) {
    case WHEEL_ACCESSORY_SYNC_PREPARE_NATURAL_FRICTION:
    case WHEEL_ACCESSORY_SYNC_WRITE_NATURAL_FRICTION:
        return 6;
    case WHEEL_ACCESSORY_SYNC_PREPARE_INTERPOLATION_FILTER:
    case WHEEL_ACCESSORY_SYNC_WRITE_INTERPOLATION_FILTER:
        return 8;
    case WHEEL_ACCESSORY_SYNC_PREPARE_NATURAL_DAMPER:
    case WHEEL_ACCESSORY_SYNC_WRITE_NATURAL_DAMPER:
        return 5;
    case WHEEL_ACCESSORY_SYNC_PREPARE_NATURAL_INERTIA:
    case WHEEL_ACCESSORY_SYNC_WRITE_NATURAL_INERTIA:
        return 7;
    case WHEEL_ACCESSORY_SYNC_PREPARE_FORCE_FEEDBACK_SCALE:
    case WHEEL_ACCESSORY_SYNC_WRITE_FORCE_FEEDBACK_SCALE:
        return 4;
    case WHEEL_ACCESSORY_SYNC_PREPARE_SENSITIVITY:
    case WHEEL_ACCESSORY_SYNC_WRITE_SENSITIVITY:
        return 2;
    case WHEEL_ACCESSORY_SYNC_PREPARE_FORCE_FEEDBACK_STRENGTH:
    case WHEEL_ACCESSORY_SYNC_WRITE_FORCE_FEEDBACK_STRENGTH:
        return 3;
    case WHEEL_ACCESSORY_SYNC_PREPARE_FORCE_EFFECT_INTENSITY:
    case WHEEL_ACCESSORY_SYNC_WRITE_FORCE_EFFECT_INTENSITY:
        return 9;
    case WHEEL_ACCESSORY_SYNC_PREPARE_FORCE_EFFECT_STRENGTH:
    case WHEEL_ACCESSORY_SYNC_WRITE_FORCE_EFFECT_STRENGTH:
        return 10;
    case WHEEL_ACCESSORY_SYNC_PREPARE_SPRING_EFFECT_STRENGTH:
    case WHEEL_ACCESSORY_SYNC_WRITE_SPRING_EFFECT_STRENGTH:
        return 11;
    case WHEEL_ACCESSORY_SYNC_PREPARE_DAMPER_EFFECT_STRENGTH:
    case WHEEL_ACCESSORY_SYNC_WRITE_DAMPER_EFFECT_STRENGTH:
        return 12;
    default:
        return 0;
    }
}

static bool is_parameter_write_state(WheelAccessorySyncState state) {
    switch (state) {
    case WHEEL_ACCESSORY_SYNC_WRITE_NATURAL_FRICTION:
    case WHEEL_ACCESSORY_SYNC_WRITE_INTERPOLATION_FILTER:
    case WHEEL_ACCESSORY_SYNC_WRITE_NATURAL_DAMPER:
    case WHEEL_ACCESSORY_SYNC_WRITE_NATURAL_INERTIA:
    case WHEEL_ACCESSORY_SYNC_WRITE_FORCE_FEEDBACK_SCALE:
    case WHEEL_ACCESSORY_SYNC_WRITE_SENSITIVITY:
    case WHEEL_ACCESSORY_SYNC_WRITE_FORCE_FEEDBACK_STRENGTH:
    case WHEEL_ACCESSORY_SYNC_WRITE_FORCE_EFFECT_INTENSITY:
    case WHEEL_ACCESSORY_SYNC_WRITE_FORCE_EFFECT_STRENGTH:
    case WHEEL_ACCESSORY_SYNC_WRITE_SPRING_EFFECT_STRENGTH:
    case WHEEL_ACCESSORY_SYNC_WRITE_DAMPER_EFFECT_STRENGTH:
        return true;
    default:
        return false;
    }
}

static void clear_parameter_if_unsupported(WheelAccessoryService *service, uint8_t index) {
    service->dirty_parameters &= (uint16_t)~(1u << index);
}

static bool queue_read(WheelAccessoryService *service, CommandTransport *transport, uint8_t offset,
                       uint8_t *output, uint16_t length, WheelAccessoryTransfer transfer) {
    if (command_transport_queue_read_from(transport, WHEEL_ACCESSORY_SERVICE_OWNER,
                                          WHEEL_ACCESSORY_TARGET, offset, output,
                                          length) != COMMAND_TRANSPORT_COMPLETE) {
        return false;
    }
    service->request_pending = true;
    service->transfer = (uint8_t)transfer;
    return true;
}

static bool queue_write(WheelAccessoryService *service, CommandTransport *transport, uint8_t offset,
                        const uint8_t *data, uint16_t length, WheelAccessoryTransfer transfer) {
    const uint8_t *queued_data = data;
    if (transfer == WHEEL_ACCESSORY_TRANSFER_PARAMETER_WRITE) {
        memcpy(service->sync_write_data, data, length);
        queued_data = service->sync_write_data;
    }
    if (command_transport_queue_write_to(transport, WHEEL_ACCESSORY_SERVICE_OWNER,
                                         WHEEL_ACCESSORY_TARGET, offset, queued_data,
                                         length) != COMMAND_TRANSPORT_COMPLETE) {
        return false;
    }
    service->request_pending = true;
    service->transfer = (uint8_t)transfer;
    if (transfer == WHEEL_ACCESSORY_TRANSFER_PARAMETER_WRITE) {
        service->sync_request = true;
    }
    return true;
}

static void set_calibration_event(WheelAccessoryService *service, MotorCalibrationEvent event) {
    if (service->calibration_event == MOTOR_CALIBRATION_EVENT_NONE) {
        service->calibration_event = event;
    }
}

static void set_motor_event(WheelAccessoryService *service, MotorStatusEvent event) {
    if (service->motor_event == MOTOR_STATUS_EVENT_NONE) {
        service->motor_event = event;
    }
}

static bool select_calibration(WheelAccessoryService *service) {
    if (service->calibration_requests == 0) {
        return false;
    }
    service->calibration_operation =
        (service->calibration_requests & WHEEL_ACCESSORY_CALIBRATION_REQUEST) != 0
            ? MOTOR_CALIBRATION_OPERATION_CALIBRATE
            : MOTOR_CALIBRATION_OPERATION_ERASE;
    return true;
}

static void finish_calibration_read(WheelAccessoryService *service) {
    uint16_t response = decode_u16(service->calibration_data);
    if (response != 0) {
        service->sync_state = WHEEL_ACCESSORY_SYNC_PREPARE_NATURAL_FRICTION;
        return;
    }
    if (service->calibration_command_sent) {
        service->calibration_requests = 0;
        service->calibration_command_sent = false;
        set_calibration_event(service, service->calibration_operation ==
                                               MOTOR_CALIBRATION_OPERATION_CALIBRATE
                                           ? MOTOR_CALIBRATION_EVENT_COMPLETED
                                           : MOTOR_CALIBRATION_EVENT_ERASED);
        service->sync_state = WHEEL_ACCESSORY_SYNC_PREPARE_NATURAL_FRICTION;
        return;
    }
    if (!select_calibration(service)) {
        service->sync_state = WHEEL_ACCESSORY_SYNC_PREPARE_NATURAL_FRICTION;
        return;
    }
    service->calibration_data[0] =
        service->calibration_operation == MOTOR_CALIBRATION_OPERATION_CALIBRATE
            ? WHEEL_ACCESSORY_CALIBRATION_VALUE
            : WHEEL_ACCESSORY_ERASE_VALUE;
    service->calibration_data[1] = service->calibration_data[0];
    service->sync_state = WHEEL_ACCESSORY_SYNC_WRITE_CALIBRATION_COMMAND;
}

static void finish_motor_command_read(WheelAccessoryService *service) {
    uint16_t response = decode_u16(service->motor_command_data);
    if (response == 0xbbbb) {
        set_motor_event(service, MOTOR_STATUS_EVENT_POSITION_SENSOR_TEST_FAILED);
        service->output_inhibited = true;
        service->motor_start_pending = false;
        service->motor_command_sent = false;
        service->sync_state = WHEEL_ACCESSORY_SYNC_PREPARE_NATURAL_DAMPER;
    } else if (response == 0) {
        if (service->motor_command_sent) {
            set_motor_event(service, MOTOR_STATUS_EVENT_POSITION_SENSOR_TEST_SUCCEEDED);
            service->motor_command_sent = false;
            service->motor_start_pending = false;
            service->sync_state = WHEEL_ACCESSORY_SYNC_PREPARE_NATURAL_DAMPER;
        } else if (service->motor_start_pending) {
            set_motor_event(service, MOTOR_STATUS_EVENT_POSITION_SENSOR_TEST_STARTED);
            service->sync_state = WHEEL_ACCESSORY_SYNC_WRITE_MOTOR_START;
        } else {
            service->sync_state = WHEEL_ACCESSORY_SYNC_PREPARE_NATURAL_DAMPER;
        }
    } else if (response == 0xaaaa || response == 0xffff) {
        service->sync_state = WHEEL_ACCESSORY_SYNC_PREPARE_NATURAL_DAMPER;
    }
}

static void finish_request(WheelAccessoryService *service, CommandTransport *transport,
                           uint32_t now_ms) {
    CommandTransportResult result =
        command_transport_poll(transport, WHEEL_ACCESSORY_SERVICE_OWNER);
    if (result == COMMAND_TRANSPORT_BUSY) {
        return;
    }

    WheelAccessoryTransfer transfer = (WheelAccessoryTransfer)service->transfer;
    command_transport_release(transport, WHEEL_ACCESSORY_SERVICE_OWNER);
    service->request_pending = false;
    if (result != COMMAND_TRANSPORT_COMPLETE) {
        service->sync_request = false;
        service->status_read_request = false;
        service->motor_temperature_request = false;
        service->transfer = (uint8_t)WHEEL_ACCESSORY_TRANSFER_NONE;
        return;
    }

    switch (transfer) {
    case WHEEL_ACCESSORY_TRANSFER_PROBE_STATUS:
        service->version_stage = true;
        break;
    case WHEEL_ACCESSORY_TRANSFER_PROBE_VERSION: {
        WheelAccessoryKind previous_kind = service->accessory.kind;
        uint8_t previous_model = service->accessory.model;
        uint32_t previous_version = service->accessory.version;
        int8_t status = (int8_t)service->status_byte;
        uint8_t packet = status < 0 ? (uint8_t)status : 0;
        bool protocol_three = status < 0 && (packet & 0x03u) == 3u;
        bool identified = wheel_accessory_apply_probe(&service->accessory, status,
                                                      decode_u32(service->version_bytes));
        bool supported = accessory_supports_composite(service);
        bool identity_changed = previous_kind != service->accessory.kind ||
                                previous_model != service->accessory.model ||
                                previous_version != service->accessory.version;
        if (identity_changed && !protocol_three) {
            reset_mirrored_parameters(service);
            service->motor_temperature_valid = false;
            service->driver_temperature_valid = false;
            service->runtime_valid = false;
        }
        if (supported && (identified || protocol_three)) {
            if (identified && service->accessory.kind == WHEEL_ACCESSORY_EXTENDED) {
                service->position_modulus = position_modulus_for_model(service->mirrored_model);
            }
            service->motor_temperature_enabled = true;
            service->sync_initialized = true;
            service->sync_state = WHEEL_ACCESSORY_SYNC_READ_NATURAL_DAMPER;
            service->accessory_type_stage = service->accessory.kind == WHEEL_ACCESSORY_EXTENDED;
            if (!service->accessory_type_stage) {
                service->accessory.accessory_type = 0;
            }
        } else {
            service->probe_requested = false;
            service->sync_initialized = false;
            service->motor_temperature_enabled = false;
        }
        if (identified) {
            service->mirrored_model = service->accessory.model;
        }
        service->probe_requested = false;
        service->version_stage = false;
        break;
    }
    case WHEEL_ACCESSORY_TRANSFER_ACCESSORY_TYPE:
        service->accessory.accessory_type = service->accessory_type_byte;
        service->accessory_type_stage = false;
        if (service->calibration_requests != 0 && service->accessory_type_byte != UINT8_MAX) {
            service->sync_state = WHEEL_ACCESSORY_SYNC_READ_CALIBRATION_COMMAND;
        } else {
            service->sync_state = WHEEL_ACCESSORY_SYNC_PREPARE_NATURAL_FRICTION;
        }
        break;
    case WHEEL_ACCESSORY_TRANSFER_NATURAL_DAMPER_READ:
        service->mirrored_parameters[6] = service->natural_damper_response;
        service->sync_state = WHEEL_ACCESSORY_SYNC_READ_MOTOR_TEMPERATURE;
        break;
    case WHEEL_ACCESSORY_TRANSFER_MOTOR_TEMPERATURE: {
        service->motor_temperature_request = false;
        uint16_t response = decode_u16(service->motor_temperature_response);
        if (response != UINT16_MAX) {
            service->motor_temperature_c = (int16_t)response;
            service->motor_temperature_valid = true;
        }
        service->sync_state = WHEEL_ACCESSORY_SYNC_READ_DRIVER_TEMPERATURE;
        break;
    }
    case WHEEL_ACCESSORY_TRANSFER_DRIVER_TEMPERATURE: {
        uint16_t response = decode_u16(service->driver_temperature_response);
        if (response != UINT16_MAX) {
            service->driver_temperature_c = (int16_t)response;
            service->driver_temperature_valid = true;
        }
        service->sync_state = WHEEL_ACCESSORY_SYNC_READ_RUNTIME;
        break;
    }
    case WHEEL_ACCESSORY_TRANSFER_RUNTIME: {
        uint32_t response = decode_u32(service->runtime_response);
        if (response != UINT32_MAX) {
            service->runtime_seconds = response;
            service->runtime_valid = true;
        }
        service->sync_state = WHEEL_ACCESSORY_SYNC_READ_ACCESSORY_TYPE;
        break;
    }
    case WHEEL_ACCESSORY_TRANSFER_CALIBRATION_READ:
        finish_calibration_read(service);
        break;
    case WHEEL_ACCESSORY_TRANSFER_CALIBRATION_WRITE:
        service->calibration_command_sent = true;
        service->sync_state = WHEEL_ACCESSORY_SYNC_PREPARE_NATURAL_FRICTION;
        break;
    case WHEEL_ACCESSORY_TRANSFER_MOTOR_COMMAND_READ:
        finish_motor_command_read(service);
        break;
    case WHEEL_ACCESSORY_TRANSFER_MOTOR_COMMAND_WRITE:
        service->sync_state = WHEEL_ACCESSORY_SYNC_PREPARE_NATURAL_DAMPER;
        break;
    case WHEEL_ACCESSORY_TRANSFER_PARAMETER_WRITE: {
        const WheelAccessoryParameter *parameter = &parameters[service->sync_index];
        memcpy(service->mirrored_parameters + parameter->data_offset,
               service->sync_write_data, parameter->length);
        if (memcmp(service->desired_parameters + parameter->data_offset, service->sync_write_data,
                   parameter->length) == 0) {
            service->dirty_parameters &= (uint16_t)~(1u << service->sync_index);
        } else {
            service->dirty_parameters |= (uint16_t)(1u << service->sync_index);
        }
        service->sync_request = false;
        set_next_after_parameter(service, service->sync_state);
        break;
    }
    case WHEEL_ACCESSORY_TRANSFER_STATUS_WRITE:
        service->status_synchronized = true;
        service->status_read_pending = false;
        service->sync_state = WHEEL_ACCESSORY_SYNC_READ_STATUS;
        break;
    case WHEEL_ACCESSORY_TRANSFER_STATUS_READ:
        service->status_read_request = false;
        service->status_read_pending = false;
        service->sync_state = WHEEL_ACCESSORY_SYNC_APPLY_STATUS;
        break;
    case WHEEL_ACCESSORY_TRANSFER_HANDSHAKE_WRITE:
        service->handshake_requested = false;
        service->sync_deadline_ms = now_ms + WHEEL_ACCESSORY_WAIT_CYCLE_MS;
        service->sync_state = WHEEL_ACCESSORY_SYNC_WAIT_CYCLE;
        break;
    case WHEEL_ACCESSORY_TRANSFER_OUTPUT_OVERRIDE_WRITE:
        service->output_override_active = service->output_override_requested;
        service->mirrored_parameters[6] = WHEEL_ACCESSORY_OUTPUT_OVERRIDE_VALUE;
        if (service->desired_parameters[6] == WHEEL_ACCESSORY_OUTPUT_OVERRIDE_VALUE) {
            service->dirty_parameters &= (uint16_t)~(1u << 5);
        } else {
            service->dirty_parameters |= (uint16_t)(1u << 5);
        }
        break;
    default:
        break;
    }
    service->transfer = (uint8_t)WHEEL_ACCESSORY_TRANSFER_NONE;
}

static void advance_prepare_state(WheelAccessoryService *service, uint32_t now_ms) {
    uint8_t index;
    switch (service->sync_state) {
    case WHEEL_ACCESSORY_SYNC_READ_RUNTIME:
        if (!accessory_supports_extended(service)) {
            service->sync_state = WHEEL_ACCESSORY_SYNC_READ_ACCESSORY_TYPE;
        }
        break;
    case WHEEL_ACCESSORY_SYNC_READ_ACCESSORY_TYPE:
        if (!accessory_supports_extended(service)) {
            service->sync_state = WHEEL_ACCESSORY_SYNC_PREPARE_NATURAL_FRICTION;
        }
        break;
    case WHEEL_ACCESSORY_SYNC_READ_CALIBRATION_COMMAND:
        if (!accessory_supports_extended(service) ||
            service->accessory.accessory_type == UINT8_MAX || service->calibration_requests == 0) {
            service->sync_state = WHEEL_ACCESSORY_SYNC_PREPARE_NATURAL_FRICTION;
        }
        break;
    case WHEEL_ACCESSORY_SYNC_PREPARE_NATURAL_FRICTION:
        if (!parameter_needs_sync(service, 6)) {
            clear_parameter_if_unsupported(service, 6);
            service->sync_state = WHEEL_ACCESSORY_SYNC_PREPARE_INTERPOLATION_FILTER;
        } else {
            service->sync_index = 6;
            service->sync_state = WHEEL_ACCESSORY_SYNC_WRITE_NATURAL_FRICTION;
        }
        break;
    case WHEEL_ACCESSORY_SYNC_RESERVED:
        service->sync_state = WHEEL_ACCESSORY_SYNC_PREPARE_NATURAL_FRICTION;
        break;
    case WHEEL_ACCESSORY_SYNC_PREPARE_INTERPOLATION_FILTER:
        if (!accessory_supports_extended(service)) {
            clear_parameter_if_unsupported(service, 8);
            service->sync_state = WHEEL_ACCESSORY_SYNC_READ_MOTOR_COMMAND;
        } else if (!parameter_needs_sync(service, 8)) {
            clear_parameter_if_unsupported(service, 8);
            service->sync_state = WHEEL_ACCESSORY_SYNC_READ_MOTOR_COMMAND;
        } else {
            service->sync_state = WHEEL_ACCESSORY_SYNC_WRITE_INTERPOLATION_FILTER;
        }
        break;
    case WHEEL_ACCESSORY_SYNC_READ_MOTOR_COMMAND:
        if (!accessory_supports_extended(service)) {
            service->sync_state = WHEEL_ACCESSORY_SYNC_PREPARE_NATURAL_DAMPER;
        }
        break;
    case WHEEL_ACCESSORY_SYNC_PREPARE_NATURAL_DAMPER:
        if (!parameter_needs_sync(service, 5)) {
            clear_parameter_if_unsupported(service, 5);
            service->sync_state = WHEEL_ACCESSORY_SYNC_PREPARE_NATURAL_INERTIA;
        } else {
            service->sync_state = WHEEL_ACCESSORY_SYNC_WRITE_NATURAL_DAMPER;
        }
        break;
    case WHEEL_ACCESSORY_SYNC_PREPARE_NATURAL_INERTIA:
        if (!accessory_supports_extended(service)) {
            clear_parameter_if_unsupported(service, 7);
            service->sync_state = WHEEL_ACCESSORY_SYNC_PREPARE_FORCE_FEEDBACK_SCALE;
        } else if (!parameter_needs_sync(service, 7)) {
            clear_parameter_if_unsupported(service, 7);
            service->sync_state = WHEEL_ACCESSORY_SYNC_PREPARE_FORCE_FEEDBACK_SCALE;
        } else {
            service->sync_state = WHEEL_ACCESSORY_SYNC_WRITE_NATURAL_INERTIA;
        }
        break;
    case WHEEL_ACCESSORY_SYNC_PREPARE_FORCE_FEEDBACK_SCALE:
        if (!accessory_supports_extended(service)) {
            clear_parameter_if_unsupported(service, 4);
            service->sync_state = WHEEL_ACCESSORY_SYNC_ROUTE_STATUS;
        } else if (!parameter_needs_sync(service, 4)) {
            clear_parameter_if_unsupported(service, 4);
            service->sync_state = WHEEL_ACCESSORY_SYNC_ROUTE_STATUS;
        } else {
            service->sync_state = WHEEL_ACCESSORY_SYNC_WRITE_FORCE_FEEDBACK_SCALE;
        }
        break;
    case WHEEL_ACCESSORY_SYNC_ROUTE_STATUS:
        if (service->accessory.kind == WHEEL_ACCESSORY_LEGACY) {
            service->sync_deadline_ms = now_ms + WHEEL_ACCESSORY_WAIT_CYCLE_MS;
            service->sync_state = WHEEL_ACCESSORY_SYNC_PREPARE_SENSITIVITY;
        } else {
            service->sync_state = WHEEL_ACCESSORY_SYNC_WRITE_STATUS;
        }
        break;
    case WHEEL_ACCESSORY_SYNC_WRITE_STATUS:
        if (service->status_synchronized) {
            service->sync_state = WHEEL_ACCESSORY_SYNC_READ_STATUS;
        }
        break;
    case WHEEL_ACCESSORY_SYNC_APPLY_STATUS:
        if ((accessory_supports_extended(service) && service->status_response == 0xaa) ||
            (!accessory_supports_extended(service) && service->status_response == 0xff)) {
            service->output_inhibited = true;
        }
        service->sync_state = WHEEL_ACCESSORY_SYNC_ROUTE_HANDSHAKE;
        break;
    case WHEEL_ACCESSORY_SYNC_ROUTE_HANDSHAKE:
        if (service->handshake_requested) {
            service->sync_state = WHEEL_ACCESSORY_SYNC_WRITE_HANDSHAKE;
        } else {
            service->sync_deadline_ms = now_ms + WHEEL_ACCESSORY_WAIT_CYCLE_MS;
            service->sync_state = WHEEL_ACCESSORY_SYNC_PREPARE_SENSITIVITY;
        }
        break;
    case WHEEL_ACCESSORY_SYNC_WAIT_CYCLE:
        if (platform_time_reached(now_ms, service->sync_deadline_ms)) {
            service->sync_state = WHEEL_ACCESSORY_SYNC_READ_NATURAL_DAMPER;
        }
        break;
    case WHEEL_ACCESSORY_SYNC_PREPARE_SENSITIVITY:
        refresh_sensitivity(service);
        if (!parameter_needs_sync(service, 2)) {
            clear_parameter_if_unsupported(service, 2);
            service->sync_state = WHEEL_ACCESSORY_SYNC_PREPARE_FORCE_FEEDBACK_STRENGTH;
        } else {
            service->sync_state = WHEEL_ACCESSORY_SYNC_WRITE_SENSITIVITY;
        }
        break;
    case WHEEL_ACCESSORY_SYNC_PREPARE_FORCE_FEEDBACK_STRENGTH:
        if (!parameter_needs_sync(service, 3)) {
            clear_parameter_if_unsupported(service, 3);
            service->sync_state = WHEEL_ACCESSORY_SYNC_PREPARE_FORCE_EFFECT_INTENSITY;
        } else {
            service->sync_state = WHEEL_ACCESSORY_SYNC_WRITE_FORCE_FEEDBACK_STRENGTH;
        }
        break;
    case WHEEL_ACCESSORY_SYNC_PREPARE_FORCE_EFFECT_INTENSITY:
        if (!parameter_needs_sync(service, 9)) {
            clear_parameter_if_unsupported(service, 9);
            service->sync_state = WHEEL_ACCESSORY_SYNC_PREPARE_FORCE_EFFECT_STRENGTH;
        } else {
            service->sync_state = WHEEL_ACCESSORY_SYNC_WRITE_FORCE_EFFECT_INTENSITY;
        }
        break;
    case WHEEL_ACCESSORY_SYNC_PREPARE_FORCE_EFFECT_STRENGTH:
        if (!parameter_needs_sync(service, 10)) {
            clear_parameter_if_unsupported(service, 10);
            service->sync_state = WHEEL_ACCESSORY_SYNC_PREPARE_SPRING_EFFECT_STRENGTH;
        } else {
            service->sync_state = WHEEL_ACCESSORY_SYNC_WRITE_FORCE_EFFECT_STRENGTH;
        }
        break;
    case WHEEL_ACCESSORY_SYNC_PREPARE_SPRING_EFFECT_STRENGTH:
        if (!parameter_needs_sync(service, 11)) {
            clear_parameter_if_unsupported(service, 11);
            service->sync_state = WHEEL_ACCESSORY_SYNC_PREPARE_DAMPER_EFFECT_STRENGTH;
        } else {
            service->sync_state = WHEEL_ACCESSORY_SYNC_WRITE_SPRING_EFFECT_STRENGTH;
        }
        break;
    case WHEEL_ACCESSORY_SYNC_PREPARE_DAMPER_EFFECT_STRENGTH:
        if (!parameter_needs_sync(service, 12)) {
            clear_parameter_if_unsupported(service, 12);
            service->sync_state = WHEEL_ACCESSORY_SYNC_WAIT_CYCLE;
        } else {
            service->sync_state = WHEEL_ACCESSORY_SYNC_WRITE_DAMPER_EFFECT_STRENGTH;
        }
        break;
    case WHEEL_ACCESSORY_SYNC_RESERVED_20:
    case WHEEL_ACCESSORY_SYNC_RESERVED_21:
    case WHEEL_ACCESSORY_SYNC_RESERVED_22:
        service->sync_state = WHEEL_ACCESSORY_SYNC_ROUTE_STATUS;
        break;
    case WHEEL_ACCESSORY_SYNC_RESERVED_27:
        service->sync_state = WHEEL_ACCESSORY_SYNC_ROUTE_HANDSHAKE;
        break;
    default:
        break;
    }
    index = parameter_index_for_state(service->sync_state);
    if (index < WHEEL_ACCESSORY_SYNC_PARAMETER_COUNT) {
        service->sync_index = index;
    }
}

static bool queue_prepared_write(WheelAccessoryService *service, CommandTransport *transport,
                                 uint32_t now_ms) {
    WheelAccessorySyncState previous_state = service->sync_state;
    advance_prepare_state(service, now_ms);
    if (service->sync_state == previous_state || !is_parameter_write_state(service->sync_state)) {
        return false;
    }
    return queue_sync_state(service, transport, now_ms);
}

static bool queue_sync_state(WheelAccessoryService *service, CommandTransport *transport,
                             uint32_t now_ms) {
    WheelAccessorySyncState state = service->sync_state;
    uint8_t index;
    switch (state) {
    case WHEEL_ACCESSORY_SYNC_READ_NATURAL_DAMPER:
        return queue_read(service, transport, WHEEL_ACCESSORY_NATURAL_DAMPER_OFFSET,
                          &service->natural_damper_response, 1,
                          WHEEL_ACCESSORY_TRANSFER_NATURAL_DAMPER_READ);
    case WHEEL_ACCESSORY_SYNC_READ_MOTOR_TEMPERATURE:
        service->motor_temperature_request = true;
        return queue_read(service, transport, WHEEL_ACCESSORY_MOTOR_TEMPERATURE_OFFSET,
                          service->motor_temperature_response,
                          sizeof(service->motor_temperature_response),
                          WHEEL_ACCESSORY_TRANSFER_MOTOR_TEMPERATURE);
    case WHEEL_ACCESSORY_SYNC_READ_DRIVER_TEMPERATURE:
        return queue_read(service, transport, WHEEL_ACCESSORY_DRIVER_TEMPERATURE_OFFSET,
                          service->driver_temperature_response,
                          sizeof(service->driver_temperature_response),
                          WHEEL_ACCESSORY_TRANSFER_DRIVER_TEMPERATURE);
    case WHEEL_ACCESSORY_SYNC_READ_RUNTIME:
        if (!accessory_supports_extended(service)) {
            advance_prepare_state(service, now_ms);
            return false;
        }
        return queue_read(service, transport, WHEEL_ACCESSORY_RUNTIME_OFFSET,
                          service->runtime_response, sizeof(service->runtime_response),
                          WHEEL_ACCESSORY_TRANSFER_RUNTIME);
    case WHEEL_ACCESSORY_SYNC_READ_ACCESSORY_TYPE:
        if (!accessory_supports_extended(service)) {
            advance_prepare_state(service, now_ms);
            return false;
        }
        return queue_read(service, transport, WHEEL_ACCESSORY_TYPE_OFFSET,
                          &service->accessory_type_byte, sizeof(service->accessory_type_byte),
                          WHEEL_ACCESSORY_TRANSFER_ACCESSORY_TYPE);
    case WHEEL_ACCESSORY_SYNC_READ_CALIBRATION_COMMAND:
        if (!accessory_supports_extended(service) ||
            service->accessory.accessory_type == UINT8_MAX || service->calibration_requests == 0) {
            advance_prepare_state(service, now_ms);
            return false;
        }
        memset(service->calibration_data, 0, sizeof(service->calibration_data));
        return queue_read(service, transport, WHEEL_ACCESSORY_CALIBRATION_OFFSET,
                          service->calibration_data, sizeof(service->calibration_data),
                          WHEEL_ACCESSORY_TRANSFER_CALIBRATION_READ);
    case WHEEL_ACCESSORY_SYNC_WRITE_CALIBRATION_COMMAND:
        return queue_write(service, transport, WHEEL_ACCESSORY_CALIBRATION_OFFSET,
                           service->calibration_data, sizeof(service->calibration_data),
                           WHEEL_ACCESSORY_TRANSFER_CALIBRATION_WRITE);
    case WHEEL_ACCESSORY_SYNC_PREPARE_NATURAL_FRICTION:
        return queue_prepared_write(service, transport, now_ms);
    case WHEEL_ACCESSORY_SYNC_WRITE_NATURAL_FRICTION:
        return queue_write(service, transport, parameters[6].offset,
                           service->desired_parameters + parameters[6].data_offset,
                           parameters[6].length, WHEEL_ACCESSORY_TRANSFER_PARAMETER_WRITE);
    case WHEEL_ACCESSORY_SYNC_PREPARE_INTERPOLATION_FILTER:
        return queue_prepared_write(service, transport, now_ms);
    case WHEEL_ACCESSORY_SYNC_WRITE_INTERPOLATION_FILTER:
        return queue_write(service, transport, parameters[8].offset,
                           service->desired_parameters + parameters[8].data_offset,
                           parameters[8].length, WHEEL_ACCESSORY_TRANSFER_PARAMETER_WRITE);
    case WHEEL_ACCESSORY_SYNC_READ_MOTOR_COMMAND:
        if (!accessory_supports_extended(service)) {
            advance_prepare_state(service, now_ms);
            return false;
        }
        memset(service->motor_command_data, 0, sizeof(service->motor_command_data));
        return queue_read(service, transport, WHEEL_ACCESSORY_MOTOR_COMMAND_OFFSET,
                          service->motor_command_data, sizeof(service->motor_command_data),
                          WHEEL_ACCESSORY_TRANSFER_MOTOR_COMMAND_READ);
    case WHEEL_ACCESSORY_SYNC_WRITE_MOTOR_START:
        service->motor_command_data[0] = 0xcd;
        service->motor_command_data[1] = 0xab;
        if (!queue_write(service, transport, WHEEL_ACCESSORY_MOTOR_COMMAND_OFFSET,
                         service->motor_command_data, sizeof(service->motor_command_data),
                         WHEEL_ACCESSORY_TRANSFER_MOTOR_COMMAND_WRITE)) {
            return false;
        }
        service->motor_command_sent = true;
        return true;
    case WHEEL_ACCESSORY_SYNC_PREPARE_NATURAL_DAMPER:
        return queue_prepared_write(service, transport, now_ms);
    case WHEEL_ACCESSORY_SYNC_WRITE_NATURAL_DAMPER:
        return queue_write(service, transport, parameters[5].offset,
                           service->desired_parameters + parameters[5].data_offset,
                           parameters[5].length, WHEEL_ACCESSORY_TRANSFER_PARAMETER_WRITE);
    case WHEEL_ACCESSORY_SYNC_PREPARE_NATURAL_INERTIA:
        return queue_prepared_write(service, transport, now_ms);
    case WHEEL_ACCESSORY_SYNC_WRITE_NATURAL_INERTIA:
        return queue_write(service, transport, parameters[7].offset,
                           service->desired_parameters + parameters[7].data_offset,
                           parameters[7].length, WHEEL_ACCESSORY_TRANSFER_PARAMETER_WRITE);
    case WHEEL_ACCESSORY_SYNC_PREPARE_FORCE_FEEDBACK_SCALE:
        return queue_prepared_write(service, transport, now_ms);
    case WHEEL_ACCESSORY_SYNC_WRITE_FORCE_FEEDBACK_SCALE:
        return queue_write(service, transport, parameters[4].offset,
                           service->desired_parameters + parameters[4].data_offset,
                           parameters[4].length, WHEEL_ACCESSORY_TRANSFER_PARAMETER_WRITE);
    case WHEEL_ACCESSORY_SYNC_ROUTE_STATUS:
    case WHEEL_ACCESSORY_SYNC_APPLY_STATUS:
    case WHEEL_ACCESSORY_SYNC_ROUTE_HANDSHAKE:
    case WHEEL_ACCESSORY_SYNC_WAIT_CYCLE:
        advance_prepare_state(service, now_ms);
        return false;
    case WHEEL_ACCESSORY_SYNC_WRITE_STATUS:
        if (service->status_synchronized) {
            service->sync_state = WHEEL_ACCESSORY_SYNC_READ_STATUS;
            return false;
        }
        service->status_response = service->desired_parameters[0];
        return queue_write(service, transport, WHEEL_ACCESSORY_STATUS_RESPONSE_OFFSET,
                           &service->status_response, 1, WHEEL_ACCESSORY_TRANSFER_STATUS_WRITE);
    case WHEEL_ACCESSORY_SYNC_READ_STATUS:
        service->status_read_request = true;
        return queue_read(service, transport, WHEEL_ACCESSORY_STATUS_RESPONSE_OFFSET,
                          &service->status_response, 1, WHEEL_ACCESSORY_TRANSFER_STATUS_READ);
    case WHEEL_ACCESSORY_SYNC_WRITE_HANDSHAKE:
        return queue_write(service, transport, WHEEL_ACCESSORY_HANDSHAKE_OFFSET,
                           service->desired_parameters + 1, 2,
                           WHEEL_ACCESSORY_TRANSFER_HANDSHAKE_WRITE);
    case WHEEL_ACCESSORY_SYNC_PREPARE_SENSITIVITY:
        return queue_prepared_write(service, transport, now_ms);
    case WHEEL_ACCESSORY_SYNC_WRITE_SENSITIVITY:
        return queue_write(service, transport, parameters[2].offset,
                           service->desired_parameters + parameters[2].data_offset,
                           parameters[2].length, WHEEL_ACCESSORY_TRANSFER_PARAMETER_WRITE);
    case WHEEL_ACCESSORY_SYNC_PREPARE_FORCE_FEEDBACK_STRENGTH:
        return queue_prepared_write(service, transport, now_ms);
    case WHEEL_ACCESSORY_SYNC_WRITE_FORCE_FEEDBACK_STRENGTH:
        return queue_write(service, transport, parameters[3].offset,
                           service->desired_parameters + parameters[3].data_offset,
                           parameters[3].length, WHEEL_ACCESSORY_TRANSFER_PARAMETER_WRITE);
    case WHEEL_ACCESSORY_SYNC_PREPARE_FORCE_EFFECT_INTENSITY:
        return queue_prepared_write(service, transport, now_ms);
    case WHEEL_ACCESSORY_SYNC_WRITE_FORCE_EFFECT_INTENSITY:
        return queue_write(service, transport, parameters[9].offset,
                           service->desired_parameters + parameters[9].data_offset,
                           parameters[9].length, WHEEL_ACCESSORY_TRANSFER_PARAMETER_WRITE);
    case WHEEL_ACCESSORY_SYNC_PREPARE_FORCE_EFFECT_STRENGTH:
        return queue_prepared_write(service, transport, now_ms);
    case WHEEL_ACCESSORY_SYNC_WRITE_FORCE_EFFECT_STRENGTH:
        return queue_write(service, transport, parameters[10].offset,
                           service->desired_parameters + parameters[10].data_offset,
                           parameters[10].length, WHEEL_ACCESSORY_TRANSFER_PARAMETER_WRITE);
    case WHEEL_ACCESSORY_SYNC_PREPARE_SPRING_EFFECT_STRENGTH:
        return queue_prepared_write(service, transport, now_ms);
    case WHEEL_ACCESSORY_SYNC_WRITE_SPRING_EFFECT_STRENGTH:
        return queue_write(service, transport, parameters[11].offset,
                           service->desired_parameters + parameters[11].data_offset,
                           parameters[11].length, WHEEL_ACCESSORY_TRANSFER_PARAMETER_WRITE);
    case WHEEL_ACCESSORY_SYNC_PREPARE_DAMPER_EFFECT_STRENGTH:
        return queue_prepared_write(service, transport, now_ms);
    case WHEEL_ACCESSORY_SYNC_WRITE_DAMPER_EFFECT_STRENGTH:
        return queue_write(service, transport, parameters[12].offset,
                           service->desired_parameters + parameters[12].data_offset,
                           parameters[12].length, WHEEL_ACCESSORY_TRANSFER_PARAMETER_WRITE);
    default:
        index = parameter_index_for_state(state);
        if (index < WHEEL_ACCESSORY_SYNC_PARAMETER_COUNT) {
            service->sync_index = index;
        }
        return false;
    }
}

static bool queue_probe(WheelAccessoryService *service, CommandTransport *transport) {
    if (!service->version_stage) {
        return queue_read(service, transport, WHEEL_ACCESSORY_STATUS_OFFSET, &service->status_byte,
                          sizeof(service->status_byte), WHEEL_ACCESSORY_TRANSFER_PROBE_STATUS);
    }
    return queue_read(service, transport, WHEEL_ACCESSORY_VERSION_OFFSET, service->version_bytes,
                      sizeof(service->version_bytes), WHEEL_ACCESSORY_TRANSFER_PROBE_VERSION);
}

static bool queue_output_override(WheelAccessoryService *service, CommandTransport *transport) {
    service->sync_write_data[0] = WHEEL_ACCESSORY_OUTPUT_OVERRIDE_VALUE;
    return queue_write(service, transport, WHEEL_ACCESSORY_NATURAL_DAMPER_OFFSET,
                       service->sync_write_data, 1,
                       WHEEL_ACCESSORY_TRANSFER_OUTPUT_OVERRIDE_WRITE);
}

/**
 * @brief Queues the next accessory request without bypassing composite wait state.
 *
 * Official tuning preparation states queue changed writes in their fall-through pass. The wait
 * state advances its deadline here; its resulting read is queued by the following service pass.
 *
 * @param[in,out] service Accessory state machine.
 * @param[in,out] transport Shared command transport.
 * @param[in] now_ms Current monotonic time.
 * @return True when a request is queued.
 */
static bool queue_next_request(WheelAccessoryService *service, CommandTransport *transport,
                               uint32_t now_ms) {
    if (service->probe_requested) {
        return queue_probe(service, transport);
    }
    if (service->output_override_requested) {
        if (!service->output_override_active) {
            return queue_output_override(service, transport);
        }
        return false;
    }
    if (!service->sync_initialized || !accessory_supports_composite(service)) {
        return false;
    }
    return queue_sync_state(service, transport, now_ms);
}

void wheel_accessory_service_init(WheelAccessoryService *service) {
    if (service == NULL) {
        return;
    }
    *service = (WheelAccessoryService){0};
    wheel_accessory_init(&service->accessory);
    reset_mirrored_parameters(service);
    service->desired_parameters[1] = 0xfa;
    service->desired_parameters[2] = 0x05;
    service->status_response = service->desired_parameters[0];
    service->motor_event = MOTOR_STATUS_EVENT_NONE;
    service->remote_effects_enabled = true;
    service->position_modulus = WHEEL_ACCESSORY_POSITION_MODULUS_DEFAULT;
    service->probe_requested = true;
    service->sync_state = WHEEL_ACCESSORY_SYNC_READ_NATURAL_DAMPER;
}

void wheel_accessory_service_run(WheelAccessoryService *service, CommandTransport *transport) {
    wheel_accessory_service_run_at(service, transport, 0);
}

void wheel_accessory_service_run_at(WheelAccessoryService *service, CommandTransport *transport,
                                    uint32_t now_ms) {
    if (service == NULL || transport == NULL) {
        return;
    }
    if (service->request_pending) {
        finish_request(service, transport, now_ms);
        return;
    }
    command_transport_claim(transport, WHEEL_ACCESSORY_SERVICE_OWNER);
    if (!command_transport_is_owner(transport, WHEEL_ACCESSORY_SERVICE_OWNER)) {
        return;
    }
    CommandTransportResult result =
        command_transport_poll(transport, WHEEL_ACCESSORY_SERVICE_OWNER);
    if (result != COMMAND_TRANSPORT_COMPLETE) {
        if (result != COMMAND_TRANSPORT_BUSY) {
            command_transport_release(transport, WHEEL_ACCESSORY_SERVICE_OWNER);
        }
        return;
    }
    if (!queue_next_request(service, transport, now_ms)) {
        command_transport_release(transport, WHEEL_ACCESSORY_SERVICE_OWNER);
    }
}

void wheel_accessory_service_configure(WheelAccessoryService *service,
                                       const WheelAccessorySyncParameters *parameters_value) {
    if (service == NULL || parameters_value == NULL) {
        return;
    }
    if (!service->output_override_requested && !service->output_override_active) {
        service->drift_mode = parameters_value->drift_mode;
    }
    service->requested_sensitivity = parameters_value->sensitivity;
    service->desired_parameters[4] = parameters_value->force_feedback_strength;
    service->desired_parameters[5] = parameters_value->force_feedback_scale;
    service->desired_parameters[6] = parameters_value->natural_damper;
    service->desired_parameters[7] = (uint8_t)parameters_value->natural_friction;
    service->desired_parameters[8] = (uint8_t)(parameters_value->natural_friction >> 8);
    service->desired_parameters[9] = parameters_value->natural_inertia;
    service->desired_parameters[10] = parameters_value->interpolation_filter;
    service->desired_parameters[11] = parameters_value->force_effect_intensity;
    service->desired_parameters[12] = parameters_value->force_effect_strength;
    service->desired_parameters[13] = parameters_value->spring_effect_strength;
    service->desired_parameters[14] = parameters_value->damper_effect_strength;
    if (service->output_override_requested) {
        service->desired_parameters[6] = WHEEL_ACCESSORY_OUTPUT_OVERRIDE_TUNING_VALUE;
    }
    refresh_sensitivity(service);
    for (uint8_t index = 2; index < WHEEL_ACCESSORY_SYNC_PARAMETER_COUNT; index++) {
        if (parameter_needs_sync(service, index)) {
            service->dirty_parameters |= (uint16_t)(1u << index);
        } else {
            service->dirty_parameters &= (uint16_t)~(1u << index);
        }
    }
}

void wheel_accessory_service_set_wheel_travel(WheelAccessoryService *service,
                                              uint32_t wheel_travel_limit) {
    if (service == NULL) {
        return;
    }
    service->wheel_travel_limit = wheel_travel_limit;
    refresh_sensitivity(service);
}

void wheel_accessory_service_set_wheel_mode(WheelAccessoryService *service, uint8_t wheel_mode) {
    if (service != NULL) {
        service->wheel_mode = wheel_mode;
    }
}

void wheel_accessory_service_request_calibration(WheelAccessoryService *service,
                                                 MotorCalibrationOperation operation) {
    if (service == NULL) {
        return;
    }
    if (operation == MOTOR_CALIBRATION_OPERATION_CALIBRATE && service->wheel_mode != 0) {
        set_calibration_event(service, MOTOR_CALIBRATION_EVENT_DISCONNECT_WHEEL);
        return;
    }
    service->calibration_requests |= calibration_request_bit(operation);
}

void wheel_accessory_service_request_motor_start(WheelAccessoryService *service) {
    if (service != NULL) {
        service->motor_start_pending = true;
    }
}

MotorStatusEvent wheel_accessory_service_take_motor_event(WheelAccessoryService *service) {
    if (service == NULL) {
        return MOTOR_STATUS_EVENT_NONE;
    }
    MotorStatusEvent event = service->motor_event;
    service->motor_event = MOTOR_STATUS_EVENT_NONE;
    return event;
}

void wheel_accessory_service_request_handshake(WheelAccessoryService *service) {
    if (service != NULL) {
        service->desired_parameters[1] = (uint8_t)WHEEL_ACCESSORY_HANDSHAKE_VALUE;
        service->desired_parameters[2] = (uint8_t)(WHEEL_ACCESSORY_HANDSHAKE_VALUE >> 8);
        service->handshake_requested = true;
    }
}

void wheel_accessory_service_set_output_override(WheelAccessoryService *service, bool enabled) {
    if (service == NULL || !accessory_supports_output_override(service)) {
        return;
    }
    if (enabled) {
        if (!service->output_override_requested && !service->output_override_active) {
            service->saved_drift_mode = service->drift_mode;
            service->saved_natural_damper = service->desired_parameters[6];
        }
        service->drift_mode = 0xfb;
        service->desired_parameters[6] = WHEEL_ACCESSORY_OUTPUT_OVERRIDE_TUNING_VALUE;
        service->output_override_requested = true;
    } else if (service->output_override_requested || service->output_override_active) {
        service->drift_mode = service->saved_drift_mode;
        service->desired_parameters[6] = service->saved_natural_damper;
        service->output_override_requested = false;
        service->output_override_active = false;
    }
    service->remote_effects_enabled = false;
}

bool wheel_accessory_service_calibration_pending(const WheelAccessoryService *service) {
    return service != NULL &&
           (service->calibration_requests != 0 || service->calibration_command_sent ||
            service->calibration_event != MOTOR_CALIBRATION_EVENT_NONE ||
            service->transfer == WHEEL_ACCESSORY_TRANSFER_CALIBRATION_READ ||
            service->transfer == WHEEL_ACCESSORY_TRANSFER_CALIBRATION_WRITE);
}

bool wheel_accessory_service_calibration_active(const WheelAccessoryService *service) {
    return service != NULL && (service->calibration_command_sent ||
                               service->transfer == WHEEL_ACCESSORY_TRANSFER_CALIBRATION_READ ||
                               service->transfer == WHEEL_ACCESSORY_TRANSFER_CALIBRATION_WRITE);
}

bool wheel_accessory_service_calibration_owns_transport(const WheelAccessoryService *service) {
    return service != NULL && (service->transfer == WHEEL_ACCESSORY_TRANSFER_CALIBRATION_READ ||
                               service->transfer == WHEEL_ACCESSORY_TRANSFER_CALIBRATION_WRITE);
}

MotorCalibrationEvent
wheel_accessory_service_take_calibration_event(WheelAccessoryService *service) {
    if (service == NULL) {
        return MOTOR_CALIBRATION_EVENT_NONE;
    }
    MotorCalibrationEvent event = service->calibration_event;
    service->calibration_event = MOTOR_CALIBRATION_EVENT_NONE;
    return event;
}

bool wheel_accessory_service_output_override_active(const WheelAccessoryService *service) {
    return service != NULL && service->output_override_active;
}

uint32_t wheel_accessory_service_position_modulus(const WheelAccessoryService *service) {
    return service == NULL ? 0 : service->position_modulus;
}

bool wheel_accessory_service_output_inhibited(const WheelAccessoryService *service) {
    return service != NULL && service->output_inhibited;
}

bool wheel_accessory_service_motor_temperature(const WheelAccessoryService *service,
                                               int16_t *temperature_c) {
    if (service == NULL || temperature_c == NULL || !service->motor_temperature_valid) {
        return false;
    }
    *temperature_c = service->motor_temperature_c;
    return true;
}

bool wheel_accessory_service_driver_temperature(const WheelAccessoryService *service,
                                                int16_t *temperature_c) {
    if (service == NULL || temperature_c == NULL || !service->driver_temperature_valid) {
        return false;
    }
    *temperature_c = service->driver_temperature_c;
    return true;
}

bool wheel_accessory_service_runtime(const WheelAccessoryService *service,
                                     uint32_t *runtime_seconds) {
    if (service == NULL || runtime_seconds == NULL || !service->runtime_valid) {
        return false;
    }
    *runtime_seconds = service->runtime_seconds;
    return true;
}

const WheelAccessory *wheel_accessory_service_identity(const WheelAccessoryService *service) {
    return service == NULL ? NULL : &service->accessory;
}

bool wheel_accessory_service_remote_effects_enabled(const WheelAccessoryService *service) {
    return service != NULL && service->remote_effects_enabled;
}

uint8_t wheel_accessory_service_input_transfer_code(const WheelAccessoryService *service) {
    if (service == NULL ||
        (service->accessory.kind != WHEEL_ACCESSORY_STANDARD &&
         service->accessory.kind != WHEEL_ACCESSORY_EXTENDED)) {
        return 0;
    }
    return wheel_accessory_transfer_code(&service->accessory);
}
