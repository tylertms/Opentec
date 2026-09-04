#ifndef OPENTEC_BASE_WHEEL_OUTPUT_REPORTS_H
#define OPENTEC_BASE_WHEEL_OUTPUT_REPORTS_H

#include <stdbool.h>
#include <stdint.h>

#include "wheel/interface_catalog.h"

/** @brief Sizes of attached-wheel output-report payloads in bytes. */
enum {
    WHEEL_OUTPUT_REPORT_ONE_SIZE = 12,       /**< Report-one payload size. */
    WHEEL_OUTPUT_REPORT_TWO_SIZE = 18,       /**< Report-two payload size. */
    WHEEL_OUTPUT_REPORT_FOUR_SIZE = 25,      /**< Report-four payload size. */
    WHEEL_OUTPUT_REPORT_FIVE_SIZE = 16,      /**< Report-five payload size. */
    WHEEL_OUTPUT_REPORT_SEVENTEEN_SIZE = 61, /**< Report-seventeen payload size. */
    WHEEL_OUTPUT_REMOTE_TELEMETRY_SIZE = 30, /**< Remote telemetry payload size. */
};

/** @brief Host output-report action selectors. */
typedef enum {
    WHEEL_OUTPUT_REPORT_ACTION_TWO = 0,  /**< Queues report two. */
    WHEEL_OUTPUT_REPORT_ACTION_ONE = 1,  /**< Queues report one. */
    WHEEL_OUTPUT_REPORT_ACTION_FOUR = 2, /**< Queues report four. */
    WHEEL_OUTPUT_REPORT_ACTION_FIVE = 3, /**< Queues report five. */
} WheelOutputReportAction;

/** @brief Retained attached-wheel output report payloads and pending state. */
typedef struct {
    uint8_t report_one[WHEEL_OUTPUT_REPORT_ONE_SIZE];   /**< Retained report-one payload. */
    uint8_t report_two[WHEEL_OUTPUT_REPORT_TWO_SIZE];   /**< Retained report-two payload. */
    uint8_t report_four[WHEEL_OUTPUT_REPORT_FOUR_SIZE]; /**< Retained report-four/six payload. */
    uint8_t report_five[WHEEL_OUTPUT_REPORT_FIVE_SIZE]; /**< Retained report-five payload. */
    uint8_t
        report_seventeen[WHEEL_OUTPUT_REPORT_SEVENTEEN_SIZE]; /**< Retained report-17 payload. */
    uint8_t
        remote_telemetry[WHEEL_OUTPUT_REMOTE_TELEMETRY_SIZE]; /**< Retained telemetry payload. */
    WheelInterfaceCatalog interface_catalog;                  /**< Catalog transfer state. */
    uint32_t interface_mode_toggle_deadline_ms;   /**< Next interface-gate toggle deadline. */
    uint8_t report_seventeen_sequence;            /**< Next report-17 segment number. */
    uint8_t remote_telemetry_transmissions;       /**< Remaining telemetry transmissions. */
    uint8_t shifter_state[3];                     /**< Pending type-0x16 shifter-state payload. */
    uint8_t display_command;                      /**< Pending native display command. */
    uint8_t display_notifications_pending;        /**< Pending prompt and confirmation flags. */
    uint8_t interface_presentation_command;       /**< Pending interface-presentation command. */
    uint8_t interface_presentation_transmissions; /**< Remaining presentation transmissions. */
    uint8_t pending;                              /**< Pending output-report bit mask. */
    bool interface_mode_gate;           /**< Whether legacy interface-mode forwarding is open. */
    bool interface_mode_button_latched; /**< Whether the gate chord has been consumed. */
    bool button_illumination;           /**< Requested button-illumination state. */
    bool sent_button_illumination;      /**< Last button-illumination state sent. */
    bool shifter_state_pending;         /**< Whether a type-0x16 shifter-state report is pending. */
} WheelOutputReports;

/**
 * @brief Initializes attached-wheel output-report state.
 *
 * Clears retained payloads, transfer counters, pending flags, and the interface catalog state.
 *
 * @param[out] reports Output-report state to initialize.
 */
void wheel_output_reports_init(WheelOutputReports *reports);

/**
 * @brief Applies a host output-report action.
 *
 * Copies the action payload into its retained report when the negotiated wheel mode and interface
 * gate allow that report.
 *
 * @param[in,out] reports Output-report state to update.
 * @param[in] arguments Action byte followed by the selected report payload.
 * @param[in] wheel_mode Negotiated attached-wheel mode.
 * @param[in] adapter_mode Negotiated attached-adapter mode.
 * @return True when the action was supported and its payload was queued; otherwise false.
 */
bool wheel_output_reports_apply(WheelOutputReports *reports, const uint8_t *arguments,
                                uint8_t wheel_mode, uint16_t adapter_mode);

/**
 * @brief Expands and queues a compact report-one or report-two mask.
 *
 * Decodes four packed bytes into the selected retained report and marks it pending. Legacy report
 * two is rejected while the interface-mode gate is closed.
 *
 * @param[in,out] reports Output-report state to update.
 * @param[in] report Report number, either one or two.
 * @param[in] packed Four-byte compact mask.
 * @param[in] wheel_mode Negotiated attached-wheel mode.
 * @return True when the report was supported and queued; otherwise false.
 */
bool wheel_output_reports_queue_packed(WheelOutputReports *reports, uint8_t report,
                                       const uint8_t packed[4], uint8_t wheel_mode);

/**
 * @brief Sets the legacy interface-mode gate state.
 *
 * Replaces the gate value used to allow legacy report-two forwarding.
 *
 * @param[in,out] reports Output-report state to update.
 * @param[in] enabled True to open the gate; false to close it.
 */
void wheel_output_reports_set_interface_mode_gate(WheelOutputReports *reports, bool enabled);

/**
 * @brief Updates the legacy interface-mode gate from its button chord.
 *
 * Toggles the gate once when secondary-button mask 0x9000 remains held past the debounce delay and
 * rearms it after release.
 *
 * @param[in,out] reports Output-report state to update.
 * @param[in] secondary_buttons Current attached-wheel secondary buttons.
 * @param[in] now_ms Current monotonic time in milliseconds.
 */
void wheel_output_reports_update_interface_mode_gate(WheelOutputReports *reports,
                                                     uint16_t secondary_buttons, uint32_t now_ms);

/**
 * @brief Returns the legacy interface-mode gate state.
 *
 * Reads the gate without changing its latch or deadline.
 *
 * @param[in] reports Output-report state to inspect.
 * @return True while legacy interface-mode forwarding is open; otherwise false.
 */
bool wheel_output_reports_interface_mode_gate(const WheelOutputReports *reports);

/**
 * @brief Queues the first two bytes as attached-wheel report six.
 *
 * Replaces the first two bytes of the retained report-four payload and marks the shared 25-byte
 * payload pending under report number six.
 *
 * @param[in,out] reports Output-report state to update.
 * @param[in] first First shared payload byte.
 * @param[in] second Second shared payload byte.
 */
void wheel_output_reports_queue_six(WheelOutputReports *reports, uint8_t first, uint8_t second);

/**
 * @brief Queues an H-pattern shifter-state report.
 *
 * Retains byte zero and the little-endian calibration stage for the next type-0x16 transfer.
 *
 * @param[in,out] reports Output-report state to update.
 * @param[in] state Three-byte shifter-state payload.
 */
void wheel_output_reports_queue_shifter_state(WheelOutputReports *reports, const uint8_t state[3]);

/**
 * @brief Reports whether an H-pattern shifter-state report is pending.
 *
 * Reads the pending state without consuming the transfer.
 *
 * @param[in] reports Output-report state to inspect.
 * @return True when a type-0x16 shifter-state report awaits transmission.
 */
bool wheel_output_reports_shifter_state_pending(const WheelOutputReports *reports);

/**
 * @brief Queues a segmented attached-wheel report-seventeen payload.
 *
 * Retains the complete payload, resets its segment sequence, and marks the transfer pending.
 *
 * @param[in,out] reports Output-report state to update.
 * @param[in] payload Complete report-seventeen payload.
 */
void wheel_output_reports_queue_seventeen(
    WheelOutputReports *reports, const uint8_t payload[WHEEL_OUTPUT_REPORT_SEVENTEEN_SIZE]);

/**
 * @brief Queues a native attached-wheel display command.
 *
 * Replaces the pending one-byte command sent in a type-0x82 configuration packet.
 *
 * @param[in,out] reports Output-report state to update.
 * @param[in] command Native wheel display command.
 */
void wheel_output_reports_queue_display_command(WheelOutputReports *reports, uint8_t command);

/**
 * @brief Queues a native attached-wheel display notification.
 *
 * Latches recognized prompt and confirmation commands until the next compatible transmission.
 *
 * @param[in,out] reports Output-report state to update.
 * @param[in] command Native prompt or confirmation command.
 */
void wheel_output_reports_queue_display_notification(WheelOutputReports *reports, uint8_t command);

/**
 * @brief Activates one legacy host-interface presentation.
 *
 * Starts the selected empty command, binary catalog, or indexed-help transfer and replaces any
 * earlier direct presentation cycle.
 *
 * @param[in,out] reports Output-report state to update.
 * @param[in] mode Requested host-interface presentation mode.
 */
void wheel_output_reports_activate_interface_presentation(WheelOutputReports *reports,
                                                          uint8_t mode);

/**
 * @brief Queues one remote telemetry report.
 *
 * Retains a telemetry payload only when no earlier telemetry payload is pending.
 *
 * @param[in,out] reports Output-report state to update.
 * @param[in] payload Complete remote telemetry payload.
 * @return True when the payload was retained; otherwise false.
 */
bool wheel_output_reports_queue_remote_telemetry(
    WheelOutputReports *reports, const uint8_t payload[WHEEL_OUTPUT_REMOTE_TELEMETRY_SIZE]);

/**
 * @brief Reports whether remote telemetry is pending.
 *
 * Reads the telemetry pending flag without consuming a transmission.
 *
 * @param[in] reports Output-report state to inspect.
 * @return True while a telemetry payload remains queued; otherwise false.
 */
bool wheel_output_reports_remote_telemetry_pending(const WheelOutputReports *reports);

/**
 * @brief Selects the requested legacy attached-wheel button illumination state.
 *
 * Retains the requested state until a legacy mode 0x0e wheel receives a changed value. Other modes
 * do not consume or emit the pending state.
 *
 * @param[in,out] reports Output-report state to update.
 * @param[in] enabled True to request button illumination.
 */
void wheel_output_reports_set_button_illumination(WheelOutputReports *reports, bool enabled);

/**
 * @brief Encodes the next pending attached-wheel output report.
 *
 * Selects the highest-priority pending report, encodes it into the supplied frame, and consumes
 * one-shot or one-segment pending state. The caller supplies the final checksum.
 *
 * @param[in,out] reports Output-report state to advance.
 * @param[in] wheel_mode Negotiated attached-wheel mode.
 * @param[out] frame Attached-wheel frame receiving the report payload.
 * @return True when a pending report was encoded; otherwise false.
 */
bool wheel_output_reports_encode_next(WheelOutputReports *reports, uint8_t wheel_mode,
                                      uint8_t *frame);

#endif
