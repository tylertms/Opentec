#include "wheel/transfer_service.h"

#include <stdbool.h>
#include <stdint.h>

#include "platform/aux_bus.h"
#include "transfer/command.h"

/** @brief Transfer channel addresses used by the wheel-transfer handshake. */
enum {
    WHEEL_TRANSFER_AUXILIARY_ADDRESS = 0x31, /**< Auxiliary-bus write-channel address. */
    WHEEL_TRANSFER_AUXILIARY_REGISTER = 0,   /**< Register offset used by the handshake. */
};

/** @brief Command-transport owner identifier for each transfer request channel. */
static const uint8_t request_owners[WHEEL_TRANSFER_REQUEST_COUNT] = {0, 0x30};

/** @brief Fixed probe payload sent to start a wheel-transfer handshake. */
static const uint8_t probe[WHEEL_TRANSFER_PAYLOAD_SIZE] = {'E', 'n', 'd', 'O', 'f',
                                                           'L', 'i', 'n', 'e', '+'};

/**
 * @brief Calculates the wheel-transfer response checksum.
 *
 * Applies the reflected 0x8C polynomial to each byte from an initial value of 0xFF.
 *
 * @param[in] data Response bytes to process.
 * @param[in] length Number of response bytes.
 * @return Calculated CRC-8 value.
 */
static uint8_t checksum(const uint8_t *data, uint8_t length) {
    uint8_t crc = UINT8_MAX;
    for (uint8_t index = 0; index < length; index++) {
        crc ^= data[index];
        for (uint8_t bit = 0; bit < 8; bit++) {
            crc = (crc & 1u) != 0 ? (uint8_t)((crc >> 1) ^ 0x8cu) : (uint8_t)(crc >> 1);
        }
    }
    return crc;
}

/**
 * @brief Selects the command-transport owner for a command-backed request.
 *
 * The write request uses the auxiliary bus and has no command-transport owner; the read request
 * uses command owner 0x30.
 *
 * @param[in] request Wheel-transfer request channel.
 * @return Command-transport owner identifier.
 */
static uint8_t request_owner(WheelTransferRequest request) { return request_owners[request]; }

/**
 * @brief Selects the physical transport for a wheel-transfer request.
 *
 * The reference broker sends request 0x31 over the auxiliary bus and keeps request 0x30 on the
 * command transport.
 *
 * @param[in] request Wheel-transfer request channel.
 * @return True when the request uses the auxiliary bus; otherwise false.
 */
static bool request_uses_auxiliary(WheelTransferRequest request) {
    return request == WHEEL_TRANSFER_WRITE;
}

/**
 * @brief Finishes the active wheel-transfer request.
 *
 * Stores the terminal status, releases the selected command-transport owner when the request uses
 * that transport, and returns the service to idle.
 *
 * @param[in,out] service Wheel-transfer service to finish.
 * @param[in,out] transport Shared command transport, when the request uses it.
 * @param[in] status Terminal request status.
 */
static void finish(WheelTransferService *service, CommandTransport *transport,
                   WheelTransferStatus status) {
    service->statuses[service->request] = status;
    if (!request_uses_auxiliary(service->request)) {
        command_transport_release(transport, request_owner(service->request));
    }
    service->phase = WHEEL_TRANSFER_PHASE_IDLE;
}

/**
 * @brief Checks whether the shared auxiliary bus can accept a wheel-transfer operation.
 *
 * Leaves terminal results for the service that started them. The wheel-transfer phases consume
 * only results from their own pending operation, which prevents another auxiliary-bus owner from
 * losing its completion status.
 *
 * @return True when the bus is idle or has a consumable terminal result; otherwise false.
 */
static bool auxiliary_bus_available(void) {
    return platform_aux_bus_status() == PLATFORM_AUX_BUS_IDLE;
}

/**
 * @brief Converts a command-transport failure into a wheel-transfer status.
 *
 * Distinguishes rejected writes, rejected reads, and malformed or unsupported results.
 *
 * @param[in] result Command-transport completion result.
 * @return Corresponding wheel-transfer failure status.
 */
static WheelTransferStatus failure_status(CommandTransportResult result) {
    if (result == COMMAND_TRANSPORT_WRITE_REJECTED) {
        return WHEEL_TRANSFER_WRITE_FAILED;
    }
    if (result == COMMAND_TRANSPORT_READ_REJECTED) {
        return WHEEL_TRANSFER_READ_FAILED;
    }
    return WHEEL_TRANSFER_INVALID_RESPONSE;
}

/**
 * @brief Initializes wheel-transfer request state.
 *
 * Clears both reported statuses, the response buffer, and the active request phase.
 *
 * @param[out] service Wheel-transfer service to initialize.
 */
void wheel_transfer_service_init(WheelTransferService *service) {
    *service = (WheelTransferService){0};
}

/**
 * @brief Starts one wheel-transfer request.
 *
 * Selects the write or read channel, reports it pending, clears the response buffer, and schedules
 * the fixed ten-byte probe write.
 *
 * @param[in,out] service Idle wheel-transfer service accepting the request.
 * @param[in] request Write or read request channel.
 * @return True when the request starts.
 */
bool wheel_transfer_service_start(WheelTransferService *service, WheelTransferRequest request) {
    if (service == 0 || request >= WHEEL_TRANSFER_REQUEST_COUNT ||
        service->phase != WHEEL_TRANSFER_PHASE_IDLE) {
        return false;
    }
    service->request = request;
    service->statuses[request] = WHEEL_TRANSFER_PENDING;
    for (uint8_t index = 0; index < sizeof(service->response); index++) {
        service->response[index] = 0;
    }
    service->phase = WHEEL_TRANSFER_PHASE_WRITE_READY;
    return true;
}

/**
 * @brief Advances the active wheel-transfer handshake.
 *
 * Writes the fixed ten-byte probe at offset zero and reads ten response bytes from offset zero.
 * Request 0x31 uses the shared auxiliary bus; request 0x30 claims the matching command owner.
 * Publishes the CRC or transport result when the exchange completes.
 *
 * @param[in,out] service Wheel-transfer service to advance.
 * @param[in,out] transport Shared command transport used by the command-backed handshake.
 */
void wheel_transfer_service_run(WheelTransferService *service, CommandTransport *transport) {
    if (service == 0 || transport == 0 || service->phase == WHEEL_TRANSFER_PHASE_IDLE) {
        return;
    }

    if (request_uses_auxiliary(service->request)) {
        PlatformAuxBusStatus status = platform_aux_bus_status();
        if (service->phase == WHEEL_TRANSFER_PHASE_WRITE_READY) {
            if (auxiliary_bus_available() &&
                platform_aux_bus_start_write(WHEEL_TRANSFER_AUXILIARY_ADDRESS,
                                             WHEEL_TRANSFER_AUXILIARY_REGISTER, probe,
                                             sizeof(probe))) {
                service->phase = WHEEL_TRANSFER_PHASE_WRITE_PENDING;
            }
            return;
        }
        if (service->phase == WHEEL_TRANSFER_PHASE_WRITE_PENDING) {
            if (status == PLATFORM_AUX_BUS_BUSY) {
                return;
            }
            platform_aux_bus_clear();
            if (status != PLATFORM_AUX_BUS_SUCCEEDED) {
                finish(service, transport, WHEEL_TRANSFER_WRITE_FAILED);
            } else {
                service->phase = WHEEL_TRANSFER_PHASE_READ_READY;
            }
            return;
        }
        if (service->phase == WHEEL_TRANSFER_PHASE_READ_READY) {
            if (auxiliary_bus_available() &&
                platform_aux_bus_start_read(WHEEL_TRANSFER_AUXILIARY_ADDRESS,
                                            WHEEL_TRANSFER_AUXILIARY_REGISTER, service->response,
                                            sizeof(service->response))) {
                service->phase = WHEEL_TRANSFER_PHASE_READ_PENDING;
            }
            return;
        }
        if (status == PLATFORM_AUX_BUS_BUSY) {
            return;
        }
        platform_aux_bus_clear();
        if (status != PLATFORM_AUX_BUS_SUCCEEDED) {
            finish(service, transport, WHEEL_TRANSFER_READ_FAILED);
            return;
        }
        finish(service, transport,
               checksum(service->response, sizeof(service->response)) == 0
                   ? WHEEL_TRANSFER_COMPLETE
                   : WHEEL_TRANSFER_INVALID_RESPONSE);
        return;
    }

    uint8_t owner = request_owner(service->request);
    CommandTransportResult result = command_transport_poll(transport, owner);
    if (result == COMMAND_TRANSPORT_BUSY) {
        return;
    }
    if (result != COMMAND_TRANSPORT_COMPLETE) {
        if (command_transport_is_owner(transport, owner)) {
            finish(service, transport, failure_status(result));
        }
        return;
    }

    command_transport_claim(transport, owner);
    if (!command_transport_is_owner(transport, owner)) {
        return;
    }
    if (service->phase == WHEEL_TRANSFER_PHASE_WRITE_READY) {
        result = command_transport_queue_write(transport, owner, 0, probe, sizeof(probe));
        if (result == COMMAND_TRANSPORT_COMPLETE) {
            service->phase = WHEEL_TRANSFER_PHASE_WRITE_PENDING;
        } else if (result != COMMAND_TRANSPORT_BUSY) {
            finish(service, transport, failure_status(result));
        }
        return;
    }
    if (service->phase == WHEEL_TRANSFER_PHASE_WRITE_PENDING) {
        result = command_transport_queue_read(transport, owner, 0, service->response,
                                              sizeof(service->response));
        if (result == COMMAND_TRANSPORT_COMPLETE) {
            service->phase = WHEEL_TRANSFER_PHASE_READ_PENDING;
        } else if (result != COMMAND_TRANSPORT_BUSY) {
            finish(service, transport, failure_status(result));
        }
        return;
    }

    WheelTransferStatus status = checksum(service->response, sizeof(service->response)) == 0
                                     ? WHEEL_TRANSFER_COMPLETE
                                     : WHEEL_TRANSFER_INVALID_RESPONSE;
    finish(service, transport, status);
}

/**
 * @brief Returns the last status for a wheel-transfer request.
 *
 * Reads the independent status retained for the write or read request channel.
 *
 * @param[in] service Wheel-transfer service to inspect.
 * @param[in] request Write or read request channel.
 * @return Idle, pending, complete, or failure status.
 */
WheelTransferStatus wheel_transfer_service_status(const WheelTransferService *service,
                                                  WheelTransferRequest request) {
    return service != 0 && request < WHEEL_TRANSFER_REQUEST_COUNT ? service->statuses[request]
                                                                  : WHEEL_TRANSFER_IDLE;
}

bool wheel_transfer_service_queue_native_payload(WheelTransferService *service,
                                                 const uint8_t *payload, uint8_t length) {
    if (service == 0 || payload == 0 || length == 0 ||
        length > WHEEL_TRANSFER_NATIVE_PAYLOAD_CAPACITY ||
        service->native_queue_count == WHEEL_TRANSFER_NATIVE_QUEUE_CAPACITY) {
        return false;
    }

    WheelTransferNativePayload *entry = &service->native_queue[service->native_queue_write];
    for (uint8_t index = 0; index < length; index++) {
        entry->data[index] = payload[index];
    }
    entry->length = length;
    service->native_queue_write =
        (uint8_t)((service->native_queue_write + 1u) % WHEEL_TRANSFER_NATIVE_QUEUE_CAPACITY);
    service->native_queue_count++;
    return true;
}

const WheelTransferNativePayload *
wheel_transfer_service_native_payload(const WheelTransferService *service) {
    return service != 0 && service->native_queue_count != 0
               ? &service->native_queue[service->native_queue_read]
               : 0;
}

void wheel_transfer_service_release_native_payload(WheelTransferService *service) {
    if (service == 0 || service->native_queue_count == 0) {
        return;
    }
    service->native_queue[service->native_queue_read].length = 0;
    service->native_queue_read =
        (uint8_t)((service->native_queue_read + 1u) % WHEEL_TRANSFER_NATIVE_QUEUE_CAPACITY);
    service->native_queue_count--;
}
