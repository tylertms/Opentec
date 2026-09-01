#include "wheel/accessory_service.h"

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "transfer/command.h"
#include "wheel/accessory.h"

/**
 * @brief Command-transport owner, target, and offsets for accessory identity reads.
 *
 * The service reads status, version, and extended type bytes from the fixed accessory target.
 */
enum {
    WHEEL_ACCESSORY_SERVICE_OWNER =
        0x44,                          /**< Command-transport owner identifier for this service. */
    WHEEL_ACCESSORY_TARGET = 0xf0,     /**< Remote target identifier for the accessory processor. */
    WHEEL_ACCESSORY_STATUS_OFFSET = 0, /**< Target offset of the signed status byte. */
    WHEEL_ACCESSORY_VERSION_OFFSET = 1, /**< Target offset of the four-byte version value. */
    WHEEL_ACCESSORY_TYPE_OFFSET = 7,    /**< Target offset of the extended accessory type byte. */
    WHEEL_ACCESSORY_SYNC_PARAMETER_COUNT = 13,
};

typedef struct {
    uint8_t offset;
    uint8_t data_offset;
    uint8_t length;
} WheelAccessorySyncParameter;

static const WheelAccessorySyncParameter sync_parameters[WHEEL_ACCESSORY_SYNC_PARAMETER_COUNT] = {
    {0x04, 0, 1},  {0x03, 1, 2},  {0x20, 3, 1},  {0x21, 4, 1},  {0x22, 5, 1},
    {0x23, 6, 1},  {0x24, 7, 2},  {0x25, 9, 1},  {0x26, 10, 1}, {0x27, 11, 1},
    {0x28, 12, 1}, {0x29, 13, 1}, {0x2a, 14, 1},
};

/**
 * @brief Decodes the accessory version response.
 *
 * Combines four consecutive response bytes with the least-significant byte first.
 *
 * @param[in] bytes Four-byte version response.
 * @return Decoded accessory version.
 */
static uint32_t decode_version(const uint8_t bytes[4]) {
    return (uint32_t)bytes[0] | (uint32_t)bytes[1] << 8 | (uint32_t)bytes[2] << 16 |
           (uint32_t)bytes[3] << 24;
}

/**
 * @brief Initializes attached wheel accessory polling.
 *
 * Clears retained response bytes, starts at the status request, and initializes the logical
 * accessory identity as disconnected.
 *
 * @param[out] service Accessory service to initialize.
 */
void wheel_accessory_service_init(WheelAccessoryService *service) {
    if (service == NULL) {
        return;
    }
    *service = (WheelAccessoryService){0};
    wheel_accessory_init(&service->accessory);
    memset(service->mirrored_parameters, UINT8_MAX, sizeof(service->mirrored_parameters));
    service->desired_parameters[1] = 0xfa;
    service->desired_parameters[2] = 0x05;
}

/**
 * @brief Completes the active accessory identity request.
 *
 * Advances successful status, version, and extended accessory-type reads. A successful version
 * request updates the logical identity. Failed transfers retry the same stage while preserving
 * the last accepted identity.
 *
 * @param[in,out] service Accessory service awaiting a result.
 * @param[in,out] transport Shared command transport carrying the request.
 */
static void finish_request(WheelAccessoryService *service, CommandTransport *transport) {
    CommandTransportResult result =
        command_transport_poll(transport, WHEEL_ACCESSORY_SERVICE_OWNER);
    if (result == COMMAND_TRANSPORT_BUSY) {
        return;
    }

    command_transport_release(transport, WHEEL_ACCESSORY_SERVICE_OWNER);
    service->request_pending = false;
    if (result != COMMAND_TRANSPORT_COMPLETE) {
        return;
    }
    if (service->status_read_request) {
        service->status_read_request = false;
        service->status_read_pending = false;
        service->output_inhibited = service->accessory.kind == WHEEL_ACCESSORY_EXTENDED
                                        ? service->status_response == 0xaa
                                        : service->status_response == UINT8_MAX;
    } else if (service->sync_request) {
        const WheelAccessorySyncParameter *parameter = &sync_parameters[service->sync_index];
        memcpy(service->mirrored_parameters + parameter->data_offset,
               service->desired_parameters + parameter->data_offset, parameter->length);
        service->dirty_parameters &= (uint16_t)~(1u << service->sync_index);
        service->sync_request = false;
        if (service->sync_index == 0) {
            service->status_read_pending = true;
        }
    } else if (service->accessory_type_stage) {
        service->accessory.accessory_type = service->accessory_type_byte;
        service->accessory_type_stage = false;
    } else if (service->version_stage) {
        wheel_accessory_apply_probe(&service->accessory, (int8_t)service->status_byte,
                                    decode_version(service->version_bytes));
        if (service->accessory.kind != WHEEL_ACCESSORY_DISCONNECTED && !service->sync_initialized) {
            memset(service->mirrored_parameters, UINT8_MAX, sizeof(service->mirrored_parameters));
            service->dirty_parameters = (1u << WHEEL_ACCESSORY_SYNC_PARAMETER_COUNT) - 1u;
            service->sync_initialized = true;
        }
        service->version_stage = false;
        if (service->accessory.kind == WHEEL_ACCESSORY_EXTENDED) {
            service->accessory_type_stage = true;
        } else {
            service->accessory.accessory_type = 0;
        }
    } else {
        service->version_stage = true;
    }
}

/**
 * @brief Queues the current accessory identity request.
 *
 * Reads one signed status byte from target 0xF0 offset zero, followed by four version bytes from
 * offset one and the extended accessory type from offset seven. The service waits when another
 * command owner is active.
 *
 * @param[in,out] service Accessory service selecting a request.
 * @param[in,out] transport Shared command transport accepting the request.
 */
static void start_request(WheelAccessoryService *service, CommandTransport *transport) {
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

    if (service->status_read_pending) {
        result = command_transport_queue_read_from(
            transport, WHEEL_ACCESSORY_SERVICE_OWNER, WHEEL_ACCESSORY_TARGET, 4,
            &service->status_response, sizeof(service->status_response));
        if (result == COMMAND_TRANSPORT_COMPLETE) {
            service->status_read_request = true;
        }
    } else if (service->accessory.kind != WHEEL_ACCESSORY_DISCONNECTED &&
               service->dirty_parameters != 0) {
        while ((service->dirty_parameters & (1u << service->sync_index)) == 0) {
            service->sync_index =
                (uint8_t)((service->sync_index + 1u) % WHEEL_ACCESSORY_SYNC_PARAMETER_COUNT);
        }
        const WheelAccessorySyncParameter *parameter = &sync_parameters[service->sync_index];
        result = command_transport_queue_write_to(
            transport, WHEEL_ACCESSORY_SERVICE_OWNER, WHEEL_ACCESSORY_TARGET, parameter->offset,
            service->desired_parameters + parameter->data_offset, parameter->length);
        if (result == COMMAND_TRANSPORT_COMPLETE) {
            service->sync_request = true;
        }
    } else if (service->accessory_type_stage) {
        result = command_transport_queue_read_from(
            transport, WHEEL_ACCESSORY_SERVICE_OWNER, WHEEL_ACCESSORY_TARGET,
            WHEEL_ACCESSORY_TYPE_OFFSET, &service->accessory_type_byte,
            sizeof(service->accessory_type_byte));
    } else if (!service->version_stage) {
        result = command_transport_queue_read_from(
            transport, WHEEL_ACCESSORY_SERVICE_OWNER, WHEEL_ACCESSORY_TARGET,
            WHEEL_ACCESSORY_STATUS_OFFSET, &service->status_byte, sizeof(service->status_byte));
    } else {
        result = command_transport_queue_read_from(
            transport, WHEEL_ACCESSORY_SERVICE_OWNER, WHEEL_ACCESSORY_TARGET,
            WHEEL_ACCESSORY_VERSION_OFFSET, service->version_bytes, sizeof(service->version_bytes));
    }
    if (result == COMMAND_TRANSPORT_COMPLETE) {
        service->request_pending = true;
    } else if (result != COMMAND_TRANSPORT_BUSY) {
        command_transport_release(transport, WHEEL_ACCESSORY_SERVICE_OWNER);
    }
}

/**
 * @brief Advances attached wheel accessory identity polling.
 *
 * Completes the active request or queues the next status, version, or accessory-type read using
 * the shared command transport.
 *
 * @param[in,out] service Accessory service to advance.
 * @param[in,out] transport Shared command transport used by the service.
 */
void wheel_accessory_service_run(WheelAccessoryService *service, CommandTransport *transport) {
    if (service == NULL || transport == NULL) {
        return;
    }
    if (service->request_pending) {
        finish_request(service, transport);
    } else {
        start_request(service, transport);
    }
}

/**
 * @brief Updates accessory tuning values and schedules changed parameters.
 *
 * Preserves the fixed initialization parameters and compares each configurable value with its
 * last successful mirror before setting the corresponding synchronization bit.
 *
 * @param[in,out] service Accessory service retaining desired and mirrored values.
 * @param[in] parameters Current tuning values to mirror to the accessory.
 */
void wheel_accessory_service_configure(WheelAccessoryService *service,
                                       const WheelAccessorySyncParameters *parameters) {
    if (service == NULL || parameters == NULL) {
        return;
    }
    uint8_t desired[12] = {
        parameters->sensitivity,
        parameters->force_feedback_strength,
        parameters->force_feedback_scale,
        parameters->natural_damper,
        (uint8_t)parameters->natural_friction,
        (uint8_t)(parameters->natural_friction >> 8),
        parameters->natural_inertia,
        parameters->interpolation_filter,
        parameters->force_effect_intensity,
        parameters->force_effect_strength,
        parameters->spring_effect_strength,
        parameters->damper_effect_strength,
    };
    memcpy(service->desired_parameters + 3, desired, sizeof(desired));
    for (uint8_t index = 2; index < WHEEL_ACCESSORY_SYNC_PARAMETER_COUNT; index++) {
        const WheelAccessorySyncParameter *parameter = &sync_parameters[index];
        if (memcmp(service->desired_parameters + parameter->data_offset,
                   service->mirrored_parameters + parameter->data_offset, parameter->length) != 0) {
            service->dirty_parameters |= 1u << index;
        }
    }
}

/**
 * @brief Reports the force-output inhibition state from the latest accessory status read.
 *
 * @param[in] service Accessory service to inspect.
 * @return True when the accessory requests output inhibition.
 */
bool wheel_accessory_service_output_inhibited(const WheelAccessoryService *service) {
    return service != NULL && service->output_inhibited;
}

/**
 * @brief Returns the latest attached wheel accessory identity.
 *
 * Exposes the kind and model from the last supported probe together with the latest retained
 * status, version, and extended accessory type.
 *
 * @param[in] service Accessory service to inspect.
 * @return Pointer to the retained accessory identity, or null when service is null.
 */
const WheelAccessory *wheel_accessory_service_identity(const WheelAccessoryService *service) {
    return service == NULL ? NULL : &service->accessory;
}
