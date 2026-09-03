#ifndef OPENTEC_BASE_WHEEL_UPDATER_COMMAND_SERVICE_H
#define OPENTEC_BASE_WHEEL_UPDATER_COMMAND_SERVICE_H

#include <stdbool.h>
#include <stdint.h>

#include "transfer/command.h"
#include "wheel/updater_bridge.h"

/** @brief Remote command channels used by updater bridge runtime modes. */
typedef enum {
    WHEEL_UPDATER_TARGET_USB = 0x11,      /**< USB updater command channel. */
    WHEEL_UPDATER_TARGET_PROTOCOL = 0x12, /**< Protocol updater command channel. */
} WheelUpdaterTarget;

/** @brief Shared-command-transport adapter for the wheel updater protocol. */
typedef struct {
    WheelUpdaterBridge bridge;   /**< Transport-independent updater protocol state. */
    CommandTransport *transport; /**< Shared command transport used by the service. */
    uint8_t read_buffer[60];     /**< Buffer for completed remote reads. */
    WheelUpdaterOperationKind
        pending_operation;     /**< Operation kind stored for a pending remote read. */
    WheelUpdaterTarget target; /**< Remote updater command channel selected for the exchange. */
    uint8_t pending_length;    /**< Number of bytes requested by the pending read. */
    bool operation_pending;    /**< True while the shared command transport read is active. */
    bool failure_pending;      /**< True while a transport failure awaits the next iteration. */
} WheelUpdaterCommandService;

/**
 * @brief Initializes the shared-command updater service.
 *
 * Clears the updater bridge and operation state, then stores the shared command transport pointer.
 *
 * @param[out] service Updater command service to initialize; null is ignored.
 * @param[in,out] transport Shared command transport used for updater operations.
 */
void wheel_updater_command_service_init(WheelUpdaterCommandService *service,
                                        CommandTransport *transport);

/**
 * @brief Starts an updater exchange on a remote command channel.
 *
 * Selects the USB or protocol target and delegates marker, length, and bridge-state validation to
 * the transport-independent updater protocol.
 *
 * @param[in,out] service Idle updater command service to start.
 * @param[in] target Remote updater command channel.
 * @param[in] request Marker-prefixed updater request bytes.
 * @param[in] length Request length in bytes.
 * @return True when target, transport, and request are accepted; otherwise false.
 */
bool wheel_updater_command_service_start(WheelUpdaterCommandService *service,
                                         WheelUpdaterTarget target, const uint8_t *request,
                                         uint8_t length);

/**
 * @brief Starts a route-discovery probe on a remote command channel.
 *
 * Uses the same target and transport validation as #wheel_updater_command_service_start while
 * preserving the probe-only terminal response rules.
 *
 * @param[in,out] service Idle updater command service to start.
 * @param[in] target Remote updater command channel.
 * @param[in] request Marker-prefixed route-probe request bytes.
 * @param[in] length Probe request length in bytes.
 * @return True when the probe was accepted; otherwise false.
 */
bool wheel_updater_command_service_start_probe(WheelUpdaterCommandService *service,
                                               WheelUpdaterTarget target, const uint8_t *request,
                                               uint8_t length);

/**
 * @brief Advances shared-command updater work.
 *
 * Polls the current command operation, advances the updater bridge, and queues its next remote
 * read or write when the shared transport is available.
 *
 * @param[in,out] service Active updater command service to advance; null is ignored.
 * @param[in] now_ms Current monotonic time in milliseconds.
 */
void wheel_updater_command_service_run(WheelUpdaterCommandService *service, uint32_t now_ms);

/**
 * @brief Takes a complete shared-command updater response.
 *
 * Returns the bridge's retained response and clears its response-ready phase.
 *
 * @param[in,out] service Updater command service holding a response.
 * @param[out] response Receives a pointer to retained response bytes.
 * @param[out] length Receives the response length.
 * @return True when a complete response was available; otherwise false.
 */
bool wheel_updater_command_service_take_response(WheelUpdaterCommandService *service,
                                                 const uint8_t **response, uint8_t *length);

/**
 * @brief Reports whether shared-command updater work is active.
 *
 * Includes every non-idle bridge phase, including delay and an untaken complete response. A lower
 * transport operation can remain pending after the bridge becomes idle and is intentionally not
 * part of this session predicate.
 *
 * @param[in] service Updater command service to inspect.
 * @return True while service is non-null and owns an updater exchange; otherwise false.
 */
bool wheel_updater_command_service_active(const WheelUpdaterCommandService *service);

#endif
