#include <assert.h>
#include <stdbool.h>

#include "shifter/calibration.h"

static HPatternCalibrationSamples calibration_samples(void) {
    return (HPatternCalibrationSamples){
        .neutral_longitudinal = 500,
        .reverse_lateral = 900,
        .reverse_longitudinal = 900,
        .first_lateral = 700,
        .second_lateral = 650,
        .second_longitudinal = 100,
        .third_lateral = 500,
        .fourth_lateral = 450,
        .fifth_lateral = 300,
        .sixth_lateral = 250,
        .seventh_lateral = 100,
    };
}

static void test_command_decode(void) {
    HPatternCalibrationCommand command;
    UsbOperatingModeCommand source = {.opcode = 0x19, .parameters = {1}};

    assert(h_pattern_calibration_command_decode(&source, &command));
    assert(command == H_PATTERN_CALIBRATION_COMMAND_START);

    source.parameters[0] = 2;
    assert(h_pattern_calibration_command_decode(&source, &command));
    assert(command == H_PATTERN_CALIBRATION_COMMAND_ADVANCE);

    source.parameters[0] = 0;
    assert(!h_pattern_calibration_command_decode(&source, &command));
    source.opcode = 0x18;
    source.parameters[0] = 1;
    assert(!h_pattern_calibration_command_decode(&source, &command));
    assert(!h_pattern_calibration_command_decode(NULL, &command));
    assert(!h_pattern_calibration_command_decode(&source, NULL));
}

static void test_calibration_thresholds(void) {
    HPatternCalibrationSamples samples = calibration_samples();
    HPatternCalibration result = h_pattern_calibration_build(&samples);

    assert(result.reverse_first_boundary == 800);
    assert(result.first_third_boundary == 600);
    assert(result.second_fourth_boundary == 550);
    assert(result.third_fifth_boundary == 400);
    assert(result.fourth_sixth_boundary == 350);
    assert(result.fifth_seventh_boundary == 200);
    assert(result.upper_row_threshold == 700);
    assert(result.lower_row_threshold == 300);
}

static void test_seventh_gear_boundary_fallback(void) {
    HPatternCalibrationSamples samples = calibration_samples();
    samples.seventh_lateral = 295;
    assert(h_pattern_calibration_build(&samples).fifth_seventh_boundary == 280);

    samples.seventh_lateral = 294;
    assert(h_pattern_calibration_build(&samples).fifth_seventh_boundary == 297);
}

static HPatternCalibrationResult capture(HPatternCalibrationService *service,
                                         HPatternSettings *settings, uint16_t lateral,
                                         uint16_t longitudinal) {
    if (service->release_required) {
        assert(h_pattern_calibration_service_capture(service, 5001, lateral, longitudinal,
                                                     settings) == H_PATTERN_CALIBRATION_NO_CAPTURE);
    }
    h_pattern_calibration_service_request(service, H_PATTERN_CALIBRATION_COMMAND_ADVANCE, 0, 5001);
    return h_pattern_calibration_service_capture(service, 5001, lateral, longitudinal, settings);
}

static void test_starts_for_uncalibrated_h_pattern_input(void) {
    HPatternCalibrationService service = {
        .advance_pending = true,
        .advance_input_active = true,
        .release_required = true,
    };

    assert(!h_pattern_calibration_service_start_if_required(&service, false, false, 0, 100));
    assert(!service.active);
    assert(!h_pattern_calibration_service_start_if_required(&service, true, true, 0, 100));
    assert(!service.active);
    assert(h_pattern_calibration_service_start_if_required(&service, true, false, 0, 100));
    assert(service.active);
    assert(!service.advance_pending);
    assert(!service.advance_input_active);
    assert(!service.release_required);
    assert(service.session.next_position == H_PATTERN_CALIBRATION_NEUTRAL);
    assert(!h_pattern_calibration_service_start_if_required(&service, true, false, 0, 200));
    assert(service.active);
}

static void test_entry_prompts_and_capture_delay(void) {
    HPatternCalibrationService service = {0};
    HPatternSettings settings = {0};

    h_pattern_calibration_service_request(&service, H_PATTERN_CALIBRATION_COMMAND_START, 0, 100);
    h_pattern_calibration_service_request(&service, H_PATTERN_CALIBRATION_COMMAND_ADVANCE, 0, 100);
    assert(h_pattern_calibration_service_prompt(&service, 2100) ==
           H_PATTERN_CALIBRATION_PROMPT_SHIFTER);
    assert(h_pattern_calibration_service_prompt(&service, 2101) ==
           H_PATTERN_CALIBRATION_PROMPT_CALIBRATION);
    assert(h_pattern_calibration_service_prompt(&service, 4100) ==
           H_PATTERN_CALIBRATION_PROMPT_CALIBRATION);
    assert(h_pattern_calibration_service_capture(&service, 4100, 500, 500, &settings) ==
           H_PATTERN_CALIBRATION_NO_CAPTURE);
    assert(service.advance_pending);
    assert(h_pattern_calibration_service_prompt(&service, 4101) ==
           H_PATTERN_CALIBRATION_PROMPT_POSITION);
    assert(h_pattern_calibration_service_capture(&service, 4101, 500, 500, &settings) ==
           H_PATTERN_CALIBRATION_CAPTURED);

    h_pattern_calibration_service_request(&service, H_PATTERN_CALIBRATION_COMMAND_START, 0x1c, 500);
    assert(h_pattern_calibration_service_prompt(&service, 5500) ==
           H_PATTERN_CALIBRATION_PROMPT_WAITING);
    assert(h_pattern_calibration_service_prompt(&service, 5501) ==
           H_PATTERN_CALIBRATION_PROMPT_POSITION);
}

static void test_requires_release_between_physical_captures(void) {
    HPatternCalibrationService service = {0};
    HPatternSettings settings = {0};

    h_pattern_calibration_service_request(&service, H_PATTERN_CALIBRATION_COMMAND_START, 0, 0);
    h_pattern_calibration_service_set_advance_input(&service, true);
    assert(h_pattern_calibration_service_capture(&service, 4001, 500, 500, &settings) ==
           H_PATTERN_CALIBRATION_CAPTURED);
    assert(service.session.next_position == H_PATTERN_CALIBRATION_REVERSE);

    assert(h_pattern_calibration_service_capture(&service, 4001, 900, 900, &settings) ==
           H_PATTERN_CALIBRATION_NO_CAPTURE);
    h_pattern_calibration_service_request(&service, H_PATTERN_CALIBRATION_COMMAND_ADVANCE, 0, 4001);
    assert(h_pattern_calibration_service_capture(&service, 4001, 900, 900, &settings) ==
           H_PATTERN_CALIBRATION_NO_CAPTURE);

    h_pattern_calibration_service_set_advance_input(&service, false);
    assert(h_pattern_calibration_service_capture(&service, 4001, 900, 900, &settings) ==
           H_PATTERN_CALIBRATION_NO_CAPTURE);
    assert(h_pattern_calibration_service_capture(&service, 4001, 900, 900, &settings) ==
           H_PATTERN_CALIBRATION_CAPTURED);
    assert(service.session.next_position == H_PATTERN_CALIBRATION_FIRST);

    h_pattern_calibration_service_set_advance_input(&service, false);
    assert(h_pattern_calibration_service_capture(&service, 4001, 700, 850, &settings) ==
           H_PATTERN_CALIBRATION_NO_CAPTURE);
    h_pattern_calibration_service_set_advance_input(&service, true);
    assert(h_pattern_calibration_service_capture(&service, 4001, 700, 850, &settings) ==
           H_PATTERN_CALIBRATION_CAPTURED);
    assert(service.session.next_position == H_PATTERN_CALIBRATION_SECOND);
}

static void test_calibration_capture_sequence(void) {
    HPatternCalibrationService service = {0};
    HPatternSettings settings = {0};

    assert(h_pattern_calibration_service_capture(&service, 4001, 500, 500, &settings) ==
           H_PATTERN_CALIBRATION_NO_CAPTURE);
    h_pattern_calibration_service_request(&service, H_PATTERN_CALIBRATION_COMMAND_START, 0, 0);
    assert(service.active);
    assert(service.session.next_position == H_PATTERN_CALIBRATION_NEUTRAL);
    assert(h_pattern_calibration_service_capture(&service, 4001, 500, 500, &settings) ==
           H_PATTERN_CALIBRATION_NO_CAPTURE);

    assert(capture(&service, &settings, 500, 500) == H_PATTERN_CALIBRATION_CAPTURED);
    assert(service.session.next_position == H_PATTERN_CALIBRATION_REVERSE);
    assert(capture(&service, &settings, 900, 900) == H_PATTERN_CALIBRATION_CAPTURED);
    assert(capture(&service, &settings, 700, 850) == H_PATTERN_CALIBRATION_CAPTURED);
    assert(capture(&service, &settings, 650, 100) == H_PATTERN_CALIBRATION_CAPTURED);
    assert(capture(&service, &settings, 500, 800) == H_PATTERN_CALIBRATION_CAPTURED);
    assert(capture(&service, &settings, 450, 150) == H_PATTERN_CALIBRATION_CAPTURED);
    assert(capture(&service, &settings, 300, 750) == H_PATTERN_CALIBRATION_CAPTURED);
    assert(capture(&service, &settings, 250, 200) == H_PATTERN_CALIBRATION_CAPTURED);
    assert(!settings.calibrated);
    assert(capture(&service, &settings, 100, 700) == H_PATTERN_CALIBRATION_COMPLETED);

    assert(service.active);
    assert(service.session.next_position == H_PATTERN_CALIBRATION_COMPLETE);
    assert(settings.calibrated);
    assert(settings.calibration.reverse_first_boundary == 800);
    assert(settings.calibration.first_third_boundary == 600);
    assert(settings.calibration.second_fourth_boundary == 550);
    assert(settings.calibration.third_fifth_boundary == 400);
    assert(settings.calibration.fourth_sixth_boundary == 350);
    assert(settings.calibration.fifth_seventh_boundary == 200);
    assert(settings.calibration.upper_row_threshold == 700);
    assert(settings.calibration.lower_row_threshold == 300);
    assert(capture(&service, &settings, 0, 0) == H_PATTERN_CALIBRATION_NO_CAPTURE);
    assert(!service.active);
}

static void test_extended_completion_waits_for_release_and_deadline(void) {
    HPatternCalibrationService service = {
        .session = {.next_position = H_PATTERN_CALIBRATION_SEVENTH},
        .wheel_mode = 0x1c,
        .active = true,
        .advance_input_active = true,
        .completion_input_active = true,
    };
    HPatternSettings settings = {0};

    assert(h_pattern_calibration_service_capture(&service, 5001, 100, 700, &settings) ==
           H_PATTERN_CALIBRATION_COMPLETED);
    assert(service.active);
    assert(h_pattern_calibration_service_prompt(&service, 5001) ==
           H_PATTERN_CALIBRATION_PROMPT_NONE);

    h_pattern_calibration_service_set_advance_input(&service, false);
    assert(h_pattern_calibration_service_capture(&service, 6001, 0, 0, &settings) ==
           H_PATTERN_CALIBRATION_NO_CAPTURE);
    assert(service.active);
    h_pattern_calibration_service_set_completion_input(&service, false);
    assert(h_pattern_calibration_service_capture(&service, 6002, 0, 0, &settings) ==
           H_PATTERN_CALIBRATION_NO_CAPTURE);
    assert(!service.active);
}

static void test_completion_ignores_adapter_advance_input(void) {
    HPatternCalibrationService service = {
        .session = {.next_position = H_PATTERN_CALIBRATION_COMPLETE},
        .active = true,
        .advance_input_active = true,
        .completion_input_active = false,
    };
    HPatternSettings settings = {0};

    assert(h_pattern_calibration_service_capture(&service, 5000, 0, 0, &settings) ==
           H_PATTERN_CALIBRATION_NO_CAPTURE);
    assert(!service.active);
}

static void test_cancel_clears_only_transient_state(void) {
    HPatternCalibrationService service = {
        .session = {.next_position = H_PATTERN_CALIBRATION_FOURTH},
        .started_ms = 100,
        .finish_deadline_ms = 500,
        .wheel_mode = 0x1c,
        .active = true,
        .advance_pending = true,
        .advance_input_active = true,
        .completion_input_active = true,
        .release_required = true,
    };

    h_pattern_calibration_service_cancel(&service);
    assert(!service.active);
    assert(service.session.next_position == H_PATTERN_CALIBRATION_NEUTRAL);
    assert(service.started_ms == 0);
    assert(service.finish_deadline_ms == 0);
    assert(service.wheel_mode == 0);
    assert(!service.advance_pending);
    assert(!service.advance_input_active);
    assert(!service.completion_input_active);
    assert(!service.release_required);
}

static void test_reports_stage_changes_and_connected_cadence(void) {
    HPatternCalibrationService service = {0};
    uint8_t report[3] = {0};

    h_pattern_calibration_service_request(&service, H_PATTERN_CALIBRATION_COMMAND_START, 0, 100);
    assert(h_pattern_calibration_service_take_report(&service, 100, false, report));
    assert(report[0] == 0);
    assert(report[1] == H_PATTERN_CALIBRATION_STAGE_SHOW_READY);
    assert(report[2] == 0);
    assert(!h_pattern_calibration_service_take_report(&service, 2100, false, report));
    assert(h_pattern_calibration_service_take_report(&service, 2201, false, report));
    assert(report[1] == H_PATTERN_CALIBRATION_STAGE_SHOW_START);
    assert(h_pattern_calibration_service_take_report(&service, 4201, false, report));
    assert(report[1] == H_PATTERN_CALIBRATION_STAGE_WAIT_START);

    h_pattern_calibration_service_request(&service, H_PATTERN_CALIBRATION_COMMAND_ADVANCE, 0, 4201);
    assert(h_pattern_calibration_service_take_report(&service, 4201, false, report));
    assert(report[1] == H_PATTERN_CALIBRATION_STAGE_CAPTURE_NEUTRAL);
    assert(!h_pattern_calibration_service_take_report(&service, 6200, true, report));
    assert(h_pattern_calibration_service_take_report(&service, 6201, true, report));
    assert(report[1] == H_PATTERN_CALIBRATION_STAGE_CAPTURE_NEUTRAL);

    h_pattern_calibration_service_cancel(&service);
    assert(h_pattern_calibration_service_take_report(&service, 6202, false, report));
    assert(report[1] == H_PATTERN_CALIBRATION_STAGE_DETECT_INPUT);
}

int main(void) {
    test_command_decode();
    test_calibration_thresholds();
    test_seventh_gear_boundary_fallback();
    test_starts_for_uncalibrated_h_pattern_input();
    test_entry_prompts_and_capture_delay();
    test_requires_release_between_physical_captures();
    test_calibration_capture_sequence();
    test_extended_completion_waits_for_release_and_deadline();
    test_completion_ignores_adapter_advance_input();
    test_cancel_clears_only_transient_state();
    test_reports_stage_changes_and_connected_cadence();
    return 0;
}
