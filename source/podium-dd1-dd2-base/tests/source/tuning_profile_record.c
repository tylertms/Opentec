#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "profile/record.h"

static void test_round_trip(void) {
    TuningProfileBank source;
    TuningProfileBank decoded;
    uint8_t record[TUNING_PROFILE_RECORD_SIZE];
    tuning_profile_bank_defaults(&source);
    source.selected_slot = 3;
    source.active_slot = 2;
    source.standard_mode_enabled = false;
    source.automatic_apply_pending = true;
    source.slots[0].rotation_degrees = 900;
    source.slots[1].force_feedback_strength = 80;
    source.slots[2].force_scale = TUNING_FORCE_SCALE_LINEAR;
    source.slots[3].natural_friction = 40;
    source.slots[4].paddle_mode = TUNING_DUAL_ANALOG;
    source.slots[5].throttle_pedal_curve = TUNING_PEDAL_CURVE_PROGRESSIVE;

    assert(tuning_profile_record_encode(&source, record));
    assert(tuning_profile_record_decode(record, &decoded));
    assert(decoded.selected_slot == 3);
    assert(decoded.active_slot == 2);
    assert(!decoded.standard_mode_enabled);
    assert(!decoded.automatic_apply_pending);
    assert(decoded.slots[0].rotation_degrees == 900);
    assert(decoded.slots[1].force_feedback_strength == 80);
    assert(decoded.slots[2].force_scale == TUNING_FORCE_SCALE_LINEAR);
    assert(decoded.slots[3].natural_friction == 40);
    assert(decoded.slots[4].paddle_mode == TUNING_DUAL_ANALOG);
    assert(decoded.slots[5].throttle_pedal_curve == TUNING_PEDAL_CURVE_PROGRESSIVE);
}

static void test_stable_header_and_field_encoding(void) {
    TuningProfileBank bank;
    uint8_t record[TUNING_PROFILE_RECORD_SIZE];
    tuning_profile_bank_defaults(&bank);
    bank.selected_slot = 1;
    bank.active_slot = 4;

    assert(tuning_profile_record_encode(&bank, record));
    assert(record[0] == 'O');
    assert(record[1] == 'T');
    assert(record[2] == 'P');
    assert(record[3] == 'F');
    assert(record[4] == TUNING_PROFILE_RECORD_VERSION);
    assert(record[5] == 1);
    assert(record[6] == 4);
    assert(record[7] == (uint8_t)(TUNING_PROFILE_SLOT_COUNT | 0x80));
    assert(record[8] == 0x38);
    assert(record[9] == 0x04);
    assert(record[10] == 1);
    assert(record[11] == 35);
}

static void test_rejects_invalid_records(void) {
    TuningProfileBank bank;
    uint8_t valid[TUNING_PROFILE_RECORD_SIZE];
    uint8_t changed[TUNING_PROFILE_RECORD_SIZE];
    tuning_profile_bank_defaults(&bank);
    assert(tuning_profile_record_encode(&bank, valid));

    memcpy(changed, valid, sizeof(changed));
    changed[0] = 0;
    assert(!tuning_profile_record_decode(changed, &bank));

    memcpy(changed, valid, sizeof(changed));
    changed[4]++;
    assert(!tuning_profile_record_decode(changed, &bank));

    memcpy(changed, valid, sizeof(changed));
    changed[20] ^= 1;
    assert(!tuning_profile_record_decode(changed, &bank));

    memcpy(changed, valid, sizeof(changed));
    changed[TUNING_PROFILE_RECORD_SIZE - 1] ^= 1;
    assert(!tuning_profile_record_decode(changed, &bank));
}

static void test_rejects_invalid_bank(void) {
    TuningProfileBank bank;
    uint8_t record[TUNING_PROFILE_RECORD_SIZE];
    tuning_profile_bank_defaults(&bank);

    bank.selected_slot = TUNING_PROFILE_SLOT_COUNT;
    assert(!tuning_profile_record_encode(&bank, record));
    bank.selected_slot = 0;
    bank.active_slot = TUNING_PROFILE_SLOT_COUNT;
    assert(!tuning_profile_record_encode(&bank, record));
}

int main(void) {
    test_round_trip();
    test_stable_header_and_field_encoding();
    test_rejects_invalid_records();
    test_rejects_invalid_bank();
    return 0;
}
