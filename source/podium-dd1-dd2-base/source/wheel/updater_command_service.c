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
 * Reports idle before an operation is queued, pending while a remote read is busy, and a complete
 * read fragment after the shared transport finishes. A failed operation is staged for the next
 * service iteration so the bridge observes the transport error boundary used by the reference
 * poller.
 *
 * @param[in,out] service Updater command service polling its pending operation.
 * @param[in] now_ms Current monotonic time in milliseconds.
 * @return Updater protocol input for the current service iteration.
 */
static WheelUpdaterIo poll_transport(WheelUpdaterCommandService *service, uint32_t now_ms) {
    WheelUpdaterIo io = {.now_ms = now_ms};
    if (service->failure_pending) {
        service->failure_pending = false;
        io.status = WHEEL_UPDATER_IO_FAILED;
        return io;
    }
    if (!service->operation_pending) {
        return io;
    }

    CommandTransportResult result =
        command_transport_poll(service->transport, WHEEL_UPDATER_COMMAND_OWNER);
    if (result == COMMAND_TRANSPORT_BUSY) {
        io.status = WHEEL_UPDATER_IO_PENDING;
        return io;
    }

    bool read_operation = service->pending_operation == WHEEL_UPDATER_OPERATION_READ;
    uint8_t pending_length = service->pending_length;
    service->operation_pending = false;
    service->pending_operation = WHEEL_UPDATER_OPERATION_NONE;
    service->pending_length = 0;
    if (result != COMMAND_TRANSPORT_COMPLETE) {
        service->failure_pending = true;
        io.status = WHEEL_UPDATER_IO_PENDING;
        return io;
    }
    io.status = WHEEL_UPDATER_IO_COMPLETE;
    if (read_operation) {
        io.data = service->read_buffer;
        io.length = pending_length;
    }
    return io;
}

/**
 * @brief Queues one updater operation on the shared command transport.
 *
 * Claims local owner 0x43 and submits offset-zero reads or writes to the selected remote target.
 * Accepted writes complete the protocol operation immediately; accepted reads remain pending
 * until their response arrives. A terminal transport result is staged for the next iteration.
 *
 * @param[in,out] service Updater command service submitting the operation.
 * @param[in] operation Write request or sized read returned by the protocol.
 */
static void queue_operation(WheelUpdaterCommandService *service, WheelUpdaterOperation operation,
                            uint32_t now_ms) {
    if (operation.kind == WHEEL_UPDATER_OPERATION_NONE) {
        return;
    }

    command_transport_claim(service->transport, WHEEL_UPDATER_COMMAND_OWNER);
    if (!command_transport_is_owner(service->transport, WHEEL_UPDATER_COMMAND_OWNER)) {
        return;
    }
    CommandTransportResult completion =
        command_transport_poll(service->transport, WHEEL_UPDATER_COMMAND_OWNER);
    if (completion == COMMAND_TRANSPORT_BUSY) {
        return;
    }
    if (completion != COMMAND_TRANSPORT_COMPLETE) {
        service->failure_pending = true;
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
    if (result != COMMAND_TRANSPORT_COMPLETE) {
        return;
    }
    if (operation.kind == WHEEL_UPDATER_OPERATION_WRITE) {
        WheelUpdaterIo write_io = {
            .now_ms = now_ms,
            .status = WHEEL_UPDATER_IO_COMPLETE,
        };
        (void)wheel_updater_bridge_step(&service->bridge, write_io);
        return;
    }
    service->pending_operation = WHEEL_UPDATER_OPERATION_READ;
    service->pending_length = operation.length;
    service->operation_pending = true;
}

/**
 * @brief Cancels the active command updater operation.
 *
 * Releases the shared command owner so a late remote response cannot be applied to a later
 * exchange, then clears the adapter's pending-operation state.
 *
 * @param[in,out] service Command updater service with a timed-out read.
 */
static void cancel_operation(WheelUpdaterCommandService *service) {
    command_transport_release(service->transport, WHEEL_UPDATER_COMMAND_OWNER);
    service->pending_operation = WHEEL_UPDATER_OPERATION_NONE;
    service->pending_length = 0;
    service->operation_pending = false;
    service->failure_pending = false;
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
 * @brief Starts one normal or route-probe exchange on a remote command channel.
 *
 * Accepts USB target 0x11 or protocol target 0x12 and delegates request validation and retention
 * to the updater protocol. The bridge phase, rather than a lower transport operation, controls
 * request ownership. A bridge timeout releases any lower read before a later exchange can start.
 *
 * @param[in,out] service Idle updater command service accepting the request.
 * @param[in] target Remote updater command channel.
 * @param[in] request Marker-prefixed updater request.
 * @param[in] length Request byte count.
 * @param[in] response_probe True when probe-only terminal response rules apply.
 * @return True when the target and request were accepted; otherwise false.
 */
static bool start_exchange(WheelUpdaterCommandService *service, WheelUpdaterTarget target,
                           const uint8_t *request, uint8_t length, bool response_probe) {
    if (service == NULL || service->transport == NULL ||
        (target != WHEEL_UPDATER_TARGET_USB && target != WHEEL_UPDATER_TARGET_PROTOCOL)) {
        return false;
    }
    bool started = response_probe
                       ? wheel_updater_bridge_start_probe(&service->bridge, request, length)
                       : wheel_updater_bridge_start(&service->bridge, request, length);
    if (!started) {
        return false;
    }
    service->target = target;
    service->operation_pending = false;
    service->pending_operation = WHEEL_UPDATER_OPERATION_NONE;
    service->pending_length = 0;
    service->failure_pending = false;
    return true;
}

bool wheel_updater_command_service_start(WheelUpdaterCommandService *service,
                                         WheelUpdaterTarget target, const uint8_t *request,
                                         uint8_t length) {
    return start_exchange(service, target, request, length, false);
}

bool wheel_updater_command_service_start_probe(WheelUpdaterCommandService *service,
                                               WheelUpdaterTarget target, const uint8_t *request,
                                               uint8_t length) {
    return start_exchange(service, target, request, length, true);
}

/**
 * @brief Advances updater protocol operations over the shared command transport.
 *
 * Polls a pending type-four read, advances the transport-independent response parser, and queues
 * its next offset-zero read or write while respecting the retained command owner. A reported
 * transport failure gets one bridge pass before its retry is queued. A bridge timeout releases the
 * still-pending shared command read before the response is exposed.
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
    if (operation.kind == WHEEL_UPDATER_OPERATION_CANCEL) {
        cancel_operation(service);
    } else if (!service->operation_pending && io.status != WHEEL_UPDATER_IO_FAILED) {
        queue_operation(service, operation, now_ms);
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
 * Includes every non-idle bridge phase, including delay and an untaken complete response. A lower
 * transport operation can remain pending after the bridge becomes idle and is intentionally not
 * part of this session predicate.
 *
 * @param[in] service Updater command service to inspect.
 * @return True while an updater exchange is active; otherwise false.
 */
bool wheel_updater_command_service_active(const WheelUpdaterCommandService *service) {
    return service != NULL && wheel_updater_bridge_active(&service->bridge);
}
