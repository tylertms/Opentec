#ifndef OPENTEC_BASE_WHEEL_ADAPTER_COMMANDS_H
#define OPENTEC_BASE_WHEEL_ADAPTER_COMMANDS_H

#include <stdbool.h>
#include <stdint.h>

#include "transfer/command.h"
#include "wheel/adapter.h"
#include "wheel/output_reports.h"

enum {
    WHEEL_ADAPTER_HOST_CONTROLS_SIZE = 30,
};

typedef enum {
    WHEEL_ADAPTER_COMMAND_DISCOVERING,
    WHEEL_ADAPTER_COMMAND_PROBE_PENDING,
    WHEEL_ADAPTER_COMMAND_READY,
    WHEEL_ADAPTER_COMMAND_STATUS_PENDING,
    WHEEL_ADAPTER_COMMAND_BUTTONS_PENDING,
    WHEEL_ADAPTER_COMMAND_AXES_PENDING,
    WHEEL_ADAPTER_COMMAND_ROTARY_PENDING,
    WHEEL_ADAPTER_COMMAND_HOST_CONTROLS_PENDING,
    WHEEL_ADAPTER_COMMAND_REMOTE_TUNING_ACTIVE_PENDING,
    WHEEL_ADAPTER_COMMAND_REFRESH_STATE_PENDING,
    WHEEL_ADAPTER_COMMAND_SETUP_SELECTION_PENDING,
    WHEEL_ADAPTER_COMMAND_DISPLAY_STATE_PENDING,
    WHEEL_ADAPTER_COMMAND_GLYPHS_PENDING,
    WHEEL_ADAPTER_COMMAND_DISPLAY_PENDING,
    WHEEL_ADAPTER_COMMAND_REPORT_TWO_PENDING,
    WHEEL_ADAPTER_COMMAND_REPORT_ONE_PENDING,
    WHEEL_ADAPTER_COMMAND_REPORT_FOUR_PENDING,
    WHEEL_ADAPTER_COMMAND_REPORT_FIVE_PENDING,
    WHEEL_ADAPTER_COMMAND_REPORT_SIX_PENDING,
} WheelAdapterCommandPhase;

/** @brief Asynchronous adapter discovery, input polling, and display-write state. */
typedef struct {
    uint8_t probe[4];
    uint8_t status[2];
    uint8_t glyphs[3];
    uint8_t display[3];
    uint8_t remote_tuning_active;
    uint8_t refresh_state;
    uint8_t setup_selection;
    uint8_t display_state;
    uint8_t report_one[WHEEL_OUTPUT_REPORT_ONE_SIZE];
    uint8_t report_two[WHEEL_OUTPUT_REPORT_TWO_SIZE];
    uint8_t report_four[WHEEL_OUTPUT_REPORT_FOUR_SIZE];
    uint8_t report_five[WHEEL_OUTPUT_REPORT_FIVE_SIZE];
    uint8_t host_controls[WHEEL_ADAPTER_HOST_CONTROLS_SIZE];
    uint8_t endpoint_index;
    uint8_t pending_inputs;
    uint8_t output_report_cadence;
    WheelAdapterCommandPhase phase;
    bool glyphs_pending;
    bool display_pending;
    bool host_controls_pending;
    bool host_controls_ready;
    bool remote_tuning_active_pending;
    bool refresh_state_pending;
    bool setup_selection_pending;
    bool display_state_pending;
    bool status_ready;
    bool report_one_pending;
    bool report_two_pending;
    bool report_four_pending;
    bool report_five_pending;
    bool report_six_pending;
    bool output_reports_due;
} WheelAdapterCommandService;

void wheel_adapter_command_service_init(WheelAdapterCommandService *service,
                                        WheelAdapterInput *adapter);
void wheel_adapter_command_service_queue_display(WheelAdapterCommandService *service,
                                                 uint16_t report);
void wheel_adapter_command_service_set_glyphs(WheelAdapterCommandService *service,
                                              const uint8_t glyphs[3]);
void wheel_adapter_command_service_queue_remote_tuning_active(WheelAdapterCommandService *service,
                                                              bool active);
void wheel_adapter_command_service_queue_refresh_state(WheelAdapterCommandService *service,
                                                       bool active);
void wheel_adapter_command_service_queue_setup_selection(WheelAdapterCommandService *service,
                                                         uint8_t selection);
void wheel_adapter_command_service_queue_display_state(WheelAdapterCommandService *service,
                                                       uint8_t state);
void wheel_adapter_command_service_queue_report_one(
    WheelAdapterCommandService *service, const uint8_t report[WHEEL_OUTPUT_REPORT_ONE_SIZE]);
void wheel_adapter_command_service_queue_report_two(
    WheelAdapterCommandService *service, const uint8_t report[WHEEL_OUTPUT_REPORT_TWO_SIZE]);
void wheel_adapter_command_service_queue_report_four(
    WheelAdapterCommandService *service, const uint8_t report[WHEEL_OUTPUT_REPORT_FOUR_SIZE]);
void wheel_adapter_command_service_queue_report_five(
    WheelAdapterCommandService *service, const uint8_t report[WHEEL_OUTPUT_REPORT_FIVE_SIZE]);
void wheel_adapter_command_service_queue_report_six(WheelAdapterCommandService *service,
                                                    uint8_t first, uint8_t second);
bool wheel_adapter_command_service_take_host_controls(
    WheelAdapterCommandService *service, uint8_t output[WHEEL_ADAPTER_HOST_CONTROLS_SIZE]);
void wheel_adapter_command_service_run(WheelAdapterCommandService *service,
                                       WheelAdapterInput *adapter, CommandTransport *transport);

#endif
