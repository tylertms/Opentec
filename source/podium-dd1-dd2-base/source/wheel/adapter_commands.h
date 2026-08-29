#ifndef OPENTEC_BASE_WHEEL_ADAPTER_COMMANDS_H
#define OPENTEC_BASE_WHEEL_ADAPTER_COMMANDS_H

#include <stdbool.h>
#include <stdint.h>

#include "transfer/command.h"
#include "wheel/adapter.h"

typedef enum {
    WHEEL_ADAPTER_COMMAND_DISCOVERING,
    WHEEL_ADAPTER_COMMAND_PROBE_PENDING,
    WHEEL_ADAPTER_COMMAND_READY,
    WHEEL_ADAPTER_COMMAND_STATUS_PENDING,
    WHEEL_ADAPTER_COMMAND_BUTTONS_PENDING,
    WHEEL_ADAPTER_COMMAND_AXES_PENDING,
    WHEEL_ADAPTER_COMMAND_ROTARY_PENDING,
    WHEEL_ADAPTER_COMMAND_GLYPHS_PENDING,
    WHEEL_ADAPTER_COMMAND_DISPLAY_PENDING,
} WheelAdapterCommandPhase;

/** @brief Asynchronous adapter discovery, input polling, and display-write state. */
typedef struct {
    uint8_t probe[4];
    uint8_t status[2];
    uint8_t glyphs[3];
    uint8_t display[3];
    uint8_t endpoint_index;
    uint8_t pending_inputs;
    WheelAdapterCommandPhase phase;
    bool glyphs_pending;
    bool display_pending;
} WheelAdapterCommandService;

void wheel_adapter_command_service_init(WheelAdapterCommandService *service,
                                        WheelAdapterInput *adapter);
void wheel_adapter_command_service_queue_display(WheelAdapterCommandService *service,
                                                 uint16_t report);
void wheel_adapter_command_service_set_glyphs(WheelAdapterCommandService *service,
                                              const uint8_t glyphs[3]);
void wheel_adapter_command_service_run(WheelAdapterCommandService *service,
                                       WheelAdapterInput *adapter, CommandTransport *transport);

#endif
