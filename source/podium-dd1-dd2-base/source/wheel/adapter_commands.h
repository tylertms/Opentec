#ifndef OPENTEC_BASE_WHEEL_ADAPTER_COMMANDS_H
#define OPENTEC_BASE_WHEEL_ADAPTER_COMMANDS_H

#include <stdbool.h>
#include <stdint.h>

#include "transfer/command.h"
#include "wheel/adapter.h"
#include "wheel/output_reports.h"

/**
 * @brief Sizes and limits for attached-adapter command payloads.
 *
 * These values describe the fixed wire layouts used by adapter command reads and writes.
 */
enum {
    WHEEL_ADAPTER_HOST_CONTROLS_SIZE = 30,  /**< Number of bytes in the host-control area. */
    WHEEL_ADAPTER_TEXT_LINE_COUNT = 4,      /**< Number of retained text-line slots. */
    WHEEL_ADAPTER_TEXT_LENGTH_MAXIMUM = 27, /**< Maximum text bytes in one line record. */
    WHEEL_ADAPTER_TEXT_REPORT_SIZE = 30,    /**< Maximum serialized size of one text-line record. */
};

/**
 * @brief Active phase of attached-adapter command processing.
 *
 * The phase identifies the request currently queued or the idle state in which the next request
 * can be selected.
 */
typedef enum {
    WHEEL_ADAPTER_COMMAND_DISCOVERING,   /**< Probe request may be queued for the selected endpoint.
                                          */
    WHEEL_ADAPTER_COMMAND_PROBE_PENDING, /**< An endpoint probe is in flight. */
    WHEEL_ADAPTER_COMMAND_READY, /**< The endpoint is ready for the next prioritized request. */
    WHEEL_ADAPTER_COMMAND_STATUS_PENDING,        /**< An input-status read is in flight. */
    WHEEL_ADAPTER_COMMAND_BUTTONS_PENDING,       /**< An adapter-button read is in flight. */
    WHEEL_ADAPTER_COMMAND_AXES_PENDING,          /**< An adapter-axis read is in flight. */
    WHEEL_ADAPTER_COMMAND_ROTARY_PENDING,        /**< An adapter-rotary read is in flight. */
    WHEEL_ADAPTER_COMMAND_HOST_CONTROLS_PENDING, /**< A host-control read is in flight. */
    WHEEL_ADAPTER_COMMAND_INTERFACE_PRESENTATION_PENDING, /**< An interface-presentation write is in
                                                             flight. */
    WHEEL_ADAPTER_COMMAND_REMOTE_TUNING_ACTIVE_PENDING,   /**< A remote-tuning state write is in
                                                             flight. */
    WHEEL_ADAPTER_COMMAND_REFRESH_STATE_PENDING,   /**< A refresh-state write is in flight. */
    WHEEL_ADAPTER_COMMAND_SETUP_SELECTION_PENDING, /**< A setup-selection write is in flight. */
    WHEEL_ADAPTER_COMMAND_DISPLAY_STATE_PENDING, /**< A system display-state write is in flight. */
    WHEEL_ADAPTER_COMMAND_GLYPHS_PENDING,        /**< A glyph write is in flight. */
    WHEEL_ADAPTER_COMMAND_DISPLAY_PENDING,       /**< An adapter display write is in flight. */
    WHEEL_ADAPTER_COMMAND_TEXT_LINE_PENDING,     /**< An extended text record write is in flight. */
    WHEEL_ADAPTER_COMMAND_REPORT_TWO_PENDING,    /**< A standard report-two write is in flight. */
    WHEEL_ADAPTER_COMMAND_REPORT_ONE_PENDING,    /**< A standard report-one write is in flight. */
    WHEEL_ADAPTER_COMMAND_REPORT_FOUR_PENDING,   /**< An extended report-four write is in flight. */
    WHEEL_ADAPTER_COMMAND_REPORT_FIVE_PENDING,   /**< An extended report-five write is in flight. */
    WHEEL_ADAPTER_COMMAND_REPORT_SIX_PENDING,    /**< An extended report-six write is in flight. */
} WheelAdapterCommandPhase;

/**
 * @brief Asynchronous adapter discovery, input polling, and display-write state.
 *
 * The service retains one request's payload and pending flags while sharing the command transport
 * with other wheel services.
 */
typedef struct {
    uint8_t probe[4];   /**< Endpoint probe response bytes. */
    uint8_t status[2];  /**< Two-byte input-status response. */
    uint8_t glyphs[3];  /**< Three glyph bytes retained for a glyph write. */
    uint8_t display[3]; /**< Three-byte adapter display payload. */
    uint8_t
        text_lines[WHEEL_ADAPTER_TEXT_LINE_COUNT]
                  [WHEEL_ADAPTER_TEXT_REPORT_SIZE]; /**< Serialized extended text-line records. */
    uint8_t text_line_lengths[WHEEL_ADAPTER_TEXT_LINE_COUNT]; /**< Serialized length of each
                                                                 retained text-line record. */
    uint8_t text_close[4];        /**< Serialized extended text-page close record. */
    uint8_t remote_tuning_active; /**< One-byte remote-tuning state to write. */
    uint8_t refresh_state;        /**< One-byte refresh state to write. */
    uint8_t setup_selection;      /**< One-based setup selection to write. */
    uint8_t display_state;        /**< One-byte system display state to write. */
    uint8_t
        interface_presentation_offset; /**< Extended endpoint offset for the presentation write. */
    uint8_t report_one[WHEEL_OUTPUT_REPORT_ONE_SIZE]; /**< Retained standard report-one payload. */
    uint8_t report_two[WHEEL_OUTPUT_REPORT_TWO_SIZE]; /**< Retained standard report-two payload. */
    uint8_t
        report_four[WHEEL_OUTPUT_REPORT_FOUR_SIZE]; /**< Retained extended report-four payload. */
    uint8_t
        report_five[WHEEL_OUTPUT_REPORT_FIVE_SIZE]; /**< Retained extended report-five payload. */
    uint8_t host_controls[WHEEL_ADAPTER_HOST_CONTROLS_SIZE]; /**< Latest adapter-originated
                                                                host-control area. */
    uint8_t endpoint_index; /**< Selected endpoint index, zero for standard or one for extended. */
    uint8_t pending_inputs; /**< Changed input components awaiting reads. */
    uint8_t output_report_cadence;  /**< Number of scheduling passes since the last output batch. */
    uint16_t wait_calls;            /**< Remaining service calls before retry or continuation. */
    WheelAdapterCommandPhase phase; /**< Current command-processing phase. */
    bool glyphs_pending;            /**< True when retained glyphs await a write. */
    bool display_pending;           /**< True when the retained display payload awaits a write. */
    uint8_t text_lines_pending;     /**< Bit mask of text-line slots awaiting writes. */
    bool text_close_pending;        /**< True when the text-page close record awaits a write. */
    bool host_controls_pending;     /**< True when a host-control read is requested. */
    bool host_controls_ready; /**< True when host_controls contains a completed unread response. */
    bool interface_presentation_pending; /**< True when an extended presentation write is requested.
                                          */
    bool remote_tuning_active_pending;   /**< True when remote_tuning_active awaits a write. */
    bool refresh_state_pending;          /**< True when refresh_state awaits a write. */
    bool setup_selection_pending;        /**< True when setup_selection awaits a write. */
    bool display_state_pending; /**< True when display_state awaits a standard-endpoint write. */
    bool status_ready;          /**< True after a successful input-status read. */
    bool report_one_pending;    /**< True when report_one awaits a write. */
    bool report_two_pending;    /**< True when report_two awaits a write. */
    bool report_four_pending;   /**< True when report_four awaits a write. */
    bool report_five_pending;   /**< True when report_five awaits a write. */
    bool report_six_pending;    /**< True when report-six bytes await a write. */
    bool
        output_reports_due; /**< True while the current endpoint's output batch is being written. */
} WheelAdapterCommandService;

/**
 * @brief Initializes adapter command polling and logical adapter defaults.
 *
 * Clears service and adapter state, selects the standard endpoint for discovery, centers the
 * adapter axes at their protocol defaults, and marks the adapter disconnected.
 *
 * @param[out] service Adapter command service to initialize.
 * @param[out] adapter Logical adapter state to initialize.
 */
void wheel_adapter_command_service_init(WheelAdapterCommandService *service,
                                        WheelAdapterInput *adapter);

/**
 * @brief Retains the newest adapter display report for transmission.
 *
 * Stores a nonzero report as two little-endian bytes followed by the fixed zero byte used by the
 * adapter display command; a null service or zero report is ignored.
 *
 * @param[in,out] service Adapter command service retaining the report.
 * @param[in] report Nonzero two-byte display report.
 */
void wheel_adapter_command_service_queue_display(WheelAdapterCommandService *service,
                                                 uint16_t report);

/**
 * @brief Updates the glyph response available to the adapter.
 *
 * Copies three glyph bytes into retained storage; the next adapter status response controls when
 * those bytes are written.
 *
 * @param[in,out] service Adapter command service retaining the glyphs.
 * @param[in] glyphs Three display glyph bytes to retain.
 */
void wheel_adapter_command_service_set_glyphs(WheelAdapterCommandService *service,
                                              const uint8_t glyphs[3]);

/**
 * @brief Retains the adapter remote-tuning active state.
 *
 * Converts the Boolean state to one byte and marks it for a write to the remote-tuning register.
 *
 * @param[in,out] service Adapter command service retaining the state.
 * @param[in] active True when remote tuning is active.
 */
void wheel_adapter_command_service_queue_remote_tuning_active(WheelAdapterCommandService *service,
                                                              bool active);

/**
 * @brief Retains the adapter refresh state.
 *
 * Converts the Boolean state to one byte and marks it for a write to the refresh-state register.
 *
 * @param[in,out] service Adapter command service retaining the state.
 * @param[in] active True when the adapter refresh state is active.
 */
void wheel_adapter_command_service_queue_refresh_state(WheelAdapterCommandService *service,
                                                       bool active);

/**
 * @brief Retains a remote setup selection for the adapter.
 *
 * Stores a nonzero one-based selection and marks it for a write; zero is ignored.
 *
 * @param[in,out] service Adapter command service retaining the selection.
 * @param[in] selection Nonzero one-based setup selection.
 */
void wheel_adapter_command_service_queue_setup_selection(WheelAdapterCommandService *service,
                                                         uint8_t selection);

/**
 * @brief Retains a system display state for the standard adapter endpoint.
 *
 * Stores a nonzero state and marks it for a standard-endpoint write; the request remains pending
 * while the extended endpoint is selected.
 *
 * @param[in,out] service Adapter command service retaining the state.
 * @param[in] state Nonzero system display state.
 */
void wheel_adapter_command_service_queue_display_state(WheelAdapterCommandService *service,
                                                       uint8_t state);

/**
 * @brief Queues an extended-adapter interface presentation command.
 *
 * Retains a mode from one through three and maps it to an extended endpoint offset; unsupported
 * modes are ignored.
 *
 * @param[in,out] service Adapter command service retaining the command.
 * @param[in] mode Legacy host-interface presentation mode from one through three.
 */
void wheel_adapter_command_service_queue_interface_presentation(WheelAdapterCommandService *service,
                                                                uint8_t mode);

/**
 * @brief Queues one extended-adapter text line.
 *
 * Builds and retains a line record for a one-based line from one through four, with at most the
 * maximum supported text length; a queued line replaces only the same line slot.
 *
 * @param[in,out] service Adapter command service retaining the line.
 * @param[in] line One-based display line identifier.
 * @param[in] metadata Display line presentation metadata.
 * @param[in] text Text bytes to copy.
 * @param[in] length Number of text bytes, up to WHEEL_ADAPTER_TEXT_LENGTH_MAXIMUM.
 * @return true when the line and length are valid and the record was retained; false otherwise.
 */
bool wheel_adapter_command_service_queue_text_line(WheelAdapterCommandService *service,
                                                   uint8_t line, uint8_t metadata,
                                                   const uint8_t *text, uint8_t length);

/**
 * @brief Queues the extended-adapter text-page close record.
 *
 * Retains the fixed four-byte record that closes the current extended display text page; a null
 * service is ignored.
 *
 * @param[in,out] service Adapter command service retaining the close record.
 */
void wheel_adapter_command_service_queue_text_close(WheelAdapterCommandService *service);

/**
 * @brief Retains host output report one for the standard adapter endpoint.
 *
 * Copies the complete report-one payload and marks it for transmission; a null service or report
 * is ignored.
 *
 * @param[in,out] service Adapter command service retaining the report.
 * @param[in] report Complete report-one payload.
 */
void wheel_adapter_command_service_queue_report_one(
    WheelAdapterCommandService *service, const uint8_t report[WHEEL_OUTPUT_REPORT_ONE_SIZE]);

/**
 * @brief Retains host output report two for the standard adapter endpoint.
 *
 * Copies the complete report-two payload and marks it for transmission; a null service or report
 * is ignored.
 *
 * @param[in,out] service Adapter command service retaining the report.
 * @param[in] report Complete report-two payload.
 */
void wheel_adapter_command_service_queue_report_two(
    WheelAdapterCommandService *service, const uint8_t report[WHEEL_OUTPUT_REPORT_TWO_SIZE]);

/**
 * @brief Retains host output report four for the extended adapter endpoint.
 *
 * Copies the complete report-four payload and marks it for transmission; a null service or report
 * is ignored.
 *
 * @param[in,out] service Adapter command service retaining the report.
 * @param[in] report Complete report-four payload.
 */
void wheel_adapter_command_service_queue_report_four(
    WheelAdapterCommandService *service, const uint8_t report[WHEEL_OUTPUT_REPORT_FOUR_SIZE]);

/**
 * @brief Retains host output report five for the extended adapter endpoint.
 *
 * Copies the complete report-five payload and marks it for transmission; a null service or report
 * is ignored.
 *
 * @param[in,out] service Adapter command service retaining the report.
 * @param[in] report Complete report-five payload.
 */
void wheel_adapter_command_service_queue_report_five(
    WheelAdapterCommandService *service, const uint8_t report[WHEEL_OUTPUT_REPORT_FIVE_SIZE]);

/**
 * @brief Retains the two report-six fields for the extended adapter endpoint.
 *
 * Replaces the first two bytes shared with report four and marks those bytes for a report-six
 * write; a null service is ignored.
 *
 * @param[in,out] service Adapter command service retaining the report fields.
 * @param[in] first First shared report byte.
 * @param[in] second Second shared report byte.
 */
void wheel_adapter_command_service_queue_report_six(WheelAdapterCommandService *service,
                                                    uint8_t first, uint8_t second);

/**
 * @brief Takes the latest completed adapter host-control batch.
 *
 * Copies the retained control area and clears its ready latch so the next completed read can be
 * consumed; no data is copied when a batch is unavailable.
 *
 * @param[in,out] service Adapter command service retaining the control area.
 * @param[out] output Destination for the complete control area.
 * @return true when a completed batch was copied; false when service, output, or the ready latch
 * is unavailable.
 */
bool wheel_adapter_command_service_take_host_controls(
    WheelAdapterCommandService *service, uint8_t output[WHEEL_ADAPTER_HOST_CONTROLS_SIZE]);

/**
 * @brief Advances adapter discovery, polling, and display transmission.
 *
 * Completes at most one active command per call or claims the shared transport to queue the next
 * prioritized request. Null inputs are ignored.
 *
 * @param[in,out] service Adapter command service to advance.
 * @param[in,out] adapter Logical adapter state updated by completed reads.
 * @param[in,out] transport Shared command transport used for requests.
 */
void wheel_adapter_command_service_run(WheelAdapterCommandService *service,
                                       WheelAdapterInput *adapter, CommandTransport *transport);

#endif
