#ifndef OPENTEC_BASE_WHEEL_UPDATER_COMMAND_SERVICE_H
#define OPENTEC_BASE_WHEEL_UPDATER_COMMAND_SERVICE_H

#include <stdbool.h>
#include <stdint.h>

#include "transfer/command.h"
#include "wheel/updater_bridge.h"

/** @brief Remote command channels used by updater bridge runtime modes. */
typedef enum {
    WHEEL_UPDATER_TARGET_USB = 0x11,
    WHEEL_UPDATER_TARGET_PROTOCOL = 0x12,
    WHEEL_UPDATER_TARGET_AUXILIARY = 0x20,
} WheelUpdaterTarget;

/** @brief Shared-command-transport adapter for the wheel updater protocol. */
typedef struct {
    WheelUpdaterBridge bridge;
    CommandTransport *transport;
    uint8_t read_buffer[60];
    WheelUpdaterOperationKind pending_operation;
    WheelUpdaterTarget target;
    uint8_t pending_length;
    bool operation_pending;
} WheelUpdaterCommandService;

void wheel_updater_command_service_init(WheelUpdaterCommandService *service,
                                        CommandTransport *transport);
bool wheel_updater_command_service_start(WheelUpdaterCommandService *service,
                                         WheelUpdaterTarget target, const uint8_t *request,
                                         uint8_t length);
void wheel_updater_command_service_run(WheelUpdaterCommandService *service, uint32_t now_ms);
bool wheel_updater_command_service_take_response(WheelUpdaterCommandService *service,
                                                 const uint8_t **response, uint8_t *length);
bool wheel_updater_command_service_active(const WheelUpdaterCommandService *service);

#endif
