#ifndef OPENTEC_BASE_WHEEL_OUTPUT_REPORTS_H
#define OPENTEC_BASE_WHEEL_OUTPUT_REPORTS_H

#include <stdbool.h>
#include <stdint.h>

enum {
    WHEEL_OUTPUT_REPORT_ONE_SIZE = 12,
    WHEEL_OUTPUT_REPORT_TWO_SIZE = 18,
    WHEEL_OUTPUT_REPORT_FOUR_SIZE = 25,
    WHEEL_OUTPUT_REPORT_FIVE_SIZE = 16,
    WHEEL_OUTPUT_REPORT_SEVENTEEN_SIZE = 61,
    WHEEL_OUTPUT_REMOTE_TELEMETRY_SIZE = 30,
};

/** @brief Host output-report action selectors. */
typedef enum {
    WHEEL_OUTPUT_REPORT_ACTION_TWO = 0,
    WHEEL_OUTPUT_REPORT_ACTION_ONE = 1,
    WHEEL_OUTPUT_REPORT_ACTION_FOUR = 2,
    WHEEL_OUTPUT_REPORT_ACTION_FIVE = 3,
} WheelOutputReportAction;

/** @brief Retained attached-wheel output report payloads and pending state. */
typedef struct {
    uint8_t report_one[WHEEL_OUTPUT_REPORT_ONE_SIZE];
    uint8_t report_two[WHEEL_OUTPUT_REPORT_TWO_SIZE];
    uint8_t report_four[WHEEL_OUTPUT_REPORT_FOUR_SIZE];
    uint8_t report_five[WHEEL_OUTPUT_REPORT_FIVE_SIZE];
    uint8_t report_seventeen[WHEEL_OUTPUT_REPORT_SEVENTEEN_SIZE];
    uint8_t remote_telemetry[WHEEL_OUTPUT_REMOTE_TELEMETRY_SIZE];
    uint8_t report_seventeen_sequence;
    uint8_t remote_telemetry_transmissions;
    uint8_t pending;
    bool button_illumination;
    bool sent_button_illumination;
} WheelOutputReports;

void wheel_output_reports_init(WheelOutputReports *reports);
void wheel_output_reports_apply(WheelOutputReports *reports, const uint8_t *arguments,
                                uint8_t wheel_mode, uint16_t adapter_mode,
                                bool display_blink_active);
void wheel_output_reports_queue_six(WheelOutputReports *reports, uint8_t first, uint8_t second);
void wheel_output_reports_queue_seventeen(
    WheelOutputReports *reports, const uint8_t payload[WHEEL_OUTPUT_REPORT_SEVENTEEN_SIZE]);
bool wheel_output_reports_queue_remote_telemetry(
    WheelOutputReports *reports, const uint8_t payload[WHEEL_OUTPUT_REMOTE_TELEMETRY_SIZE]);
bool wheel_output_reports_remote_telemetry_pending(const WheelOutputReports *reports);
void wheel_output_reports_set_button_illumination(WheelOutputReports *reports, bool enabled);
bool wheel_output_reports_encode_next(WheelOutputReports *reports, uint8_t wheel_mode,
                                      uint8_t *frame);

#endif
