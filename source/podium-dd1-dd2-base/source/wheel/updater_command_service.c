#include "wheel/updater_command_service.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "transfer/command.h"
#include "wheel/updater_bridge.h"

/** @brief Shared command-transport identifiers for updater operations. */
enum {
    WHEEL_UPDATER_COMMAND_OWNER = 0x43, /**< Local command-transport owner identifier. */
    WHEEL_UPDATER_COMMAND_OFFSET = 0,   /**< Remote updater register offset. */
};

/**
 * @brief Converts shared command completion into updater protocol input.
 *
 * Reports idle before an operation is queued, pending while the shared transport is busy, and a
 * complete read fragment or terminal failure after the transport finishes.
 *
 * @param[in,out] service Updater command service polling its pending operation.
 * @param[in] now_ms Current monotonic time in milliseconds.
 * @return Updater protocol input for the current service iteration.
 */
static WheelUpdaterIo poll_transport(WheelUpdaterCommandService *service, uint32_t now_ms) {
    WheelUpdaterIo io = {.now_ms = now_ms};
    if (!service->operation_pending) {
        return io;
    }

    CommandTransportResult result =
        command_transport_poll(service->transport, WHEEL_UPDATER_COMMAND_OWNER);
    if (result == COMMAND_TRANSPORT_BUSY) {
        io.status = WHEEL_UPDATER_IO_PENDING;
        return io;
    }

    service->operation_pending = false;
    io.status =
        result == COMMAND_TRANSPORT_COMPLETE ? WHEEL_UPDATER_IO_COMPLETE : WHEEL_UPDATER_IO_FAILED;
    if (io.status == WHEEL_UPDATER_IO_COMPLETE &&
        service->pending_operation == WHEEL_UPDATER_OPERATION_READ) {
        io.data = service->read_buffer;
        io.length = service->pending_length;
    }
    return io;
}

/**
 * @brief Queues one updater operation on the shared command transport.
 *
 * Claims local owner 0x43 and submits offset-zero reads or writes to the selected remote target.
 * Busy and rejected submissions remain available for retry on the next service iteration.
 *
 * @param[in,out] service Updater command service submitting the operation.
 * @param[in] operation Write request or sized read returned by the protocol.
 */
static void queue_operation(WheelUpdaterCommandService *service, WheelUpdaterOperation operation) {
    if (operation.kind == WHEEL_UPDATER_OPERATION_NONE) {
        command_transport_release(service->transport, WHEEL_UPDATER_COMMAND_OWNER);
        return;
    }

    command_transport_claim(service->transport, WHEEL_UPDATER_COMMAND_OWNER);
    if (!command_transport_is_owner(service->transport, WHEEL_UPDATER_COMMAND_OWNER)) {
        return;
    }
    if (command_transport_poll(service->transport, WHEEL_UPDATER_COMMAND_OWNER) ==
        COMMAND_TRANSPORT_BUSY) {
        return;
    }

    CommandTransportResult result;
    if (operation.kind == WHEEL_UPDATER_OPERATION_WRITE) {
        result = command_transport_queue_write_to(
            service->transport, WHEEL_UPDATER_COMMAND_OWNER, (uint8_t)service->target,
            WHEEL_UPDATER_COMMAND_OFFSET, operation.data, operation.length);
    } else {
        result = command_transport_queue_read_from(
            service->transport, WHEEL_UPDATER_COMMAND_OWNER, (uint8_t)service->target,
            WHEEL_UPDATER_COMMAND_OFFSET, service->read_buffer, operation.length);
    }
    if (result == COMMAND_TRANSPORT_COMPLETE) {
        service->pending_operation = operation.kind;
        service->pending_length = operation.length;
        service->operation_pending = true;
    }
}

/**
 * @brief Initializes updater command transport state.
 *
 * Attaches the shared command transport and clears protocol, target, and pending-operation state.
 *
 * @param[out] service Updater command service to initialize.
 * @param[in,out] transport Shared attached-wheel command transport.
 */
void wheel_updater_command_service_init(WheelUpdaterCommandService *service,
                                        CommandTransport *transport) {
    if (service == NULL) {
        return;
    }
    *service = (WheelUpdaterCommandService){.transport = transport};
    wheel_updater_bridge_init(&service->bridge);
}

/**
 * @brief Starts an updater exchange on one remote command channel.
 *
 * Accepts USB target 0x11 or protocol target 0x12 and delegates request validation and retention
 * to the updater protocol.
 *
 * @param[in,out] service Idle updater command service accepting the request.
 * @param[in] target Remote updater command channel.
 * @param[in] request Marker-prefixed updater request.
 * @param[in] length Request byte count.
 * @return True when the target and request were accepted; otherwise false.
 */
bool wheel_updater_command_service_start(WheelUpdaterCommandService *service,
                                         WheelUpdaterTarget target, const uint8_t *request,
                                         uint8_t length) {
    if (service == NULL || service->transport == NULL ||
        (target != WHEEL_UPDATER_TARGET_USB && target != WHEEL_UPDATER_TARGET_PROTOCOL)) {
        return false;
    }
    if (!wheel_updater_bridge_start(&service->bridge, request, length)) {
        return false;
    }
    service->target = target;
    return true;
}

/**
 * @brief Advances updater protocol operations over the shared command transport.
 *
 * Polls a pending type-four operation, advances the transport-independent response parser, and
 * queues its next offset-zero read or write while respecting other command owners.
 *
 * @param[in,out] service Active updater command service to advance.
 * @param[in] now_ms Current monotonic time in milliseconds.
 */
void wheel_updater_command_service_run(WheelUpdaterCommandService *service, uint32_t now_ms) {
    if (service == NULL || service->transport == NULL ||
        !wheel_updater_bridge_active(&service->bridge)) {
        return;
    }

    WheelUpdaterIo io = poll_transport(service, now_ms);
    WheelUpdaterOperation operation = wheel_updater_bridge_step(&service->bridge, io);
    if (!service->operation_pending) {
        queue_operation(service, operation);
    }
}

/**
 * @brief Takes one complete updater response.
 *
 * Delegates retained-response ownership to the transport-independent protocol service.
 *
 * @param[in,out] service Updater command service holding a response.
 * @param[out] response Complete updater response bytes.
 * @param[out] length Complete response length.
 * @return True when a response was available; otherwise false.
 */
bool wheel_updater_command_service_take_response(WheelUpdaterCommandService *service,
                                                 const uint8_t **response, uint8_t *length) {
    return service != NULL &&
           wheel_updater_bridge_take_response(&service->bridge, response, length);
}

/**
 * @brief Reports whether the updater command service owns an exchange.
 *
 * Includes queued and pending command operations plus untaken complete responses.
 *
 * @param[in] service Updater command service to inspect.
 * @return True while an updater exchange is active; otherwise false.
 */
bool wheel_updater_command_service_active(const WheelUpdaterCommandService *service) {
    return service != NULL && wheel_updater_bridge_active(&service->bridge);
}
