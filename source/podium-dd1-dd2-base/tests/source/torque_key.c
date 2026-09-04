#include "board/torque_key.h"

#include <assert.h>
#include <stdint.h>

static void test_establishes_initial_inserted_state_from_midpoint(void) {
    TorqueKey key;
    torque_key_init(&key);

    torque_key_update(&key, true, 10);
    torque_key_update(&key, true, 259);
    torque_key_update(&key, true, 260);
    torque_key_update(&key, true, 261);
    assert(key.inserted);
    torque_key_update(&key, true, 1000);
}

static void test_establishes_initial_removed_state_from_midpoint(void) {
    TorqueKey key;
    torque_key_init(&key);

    torque_key_update(&key, false, 0);
    torque_key_update(&key, false, 249);
    torque_key_update(&key, false, 250);
    torque_key_update(&key, false, 251);
    assert(!key.inserted);
}

static void test_filters_insertion_and_removal(void) {
    TorqueKey key;
    torque_key_init(&key);

    torque_key_update(&key, false, 0);
    torque_key_update(&key, false, 250);
    torque_key_update(&key, false, 251);
    torque_key_update(&key, true, 749);
    torque_key_update(&key, true, 750);
    torque_key_update(&key, true, 751);
    torque_key_update(&key, false, 1249);
    torque_key_update(&key, false, 1250);
    torque_key_update(&key, false, 1251);
    torque_key_update(&key, false, 1252);
}

static void test_opposite_samples_cancel_filter_travel(void) {
    TorqueKey key;
    torque_key_init(&key);

    torque_key_update(&key, false, 0);
    torque_key_update(&key, true, 100);
    torque_key_update(&key, false, 200);
    torque_key_update(&key, true, 449);
    torque_key_update(&key, true, 450);
    torque_key_update(&key, true, 451);
}

static void test_preserves_filter_across_counter_wrap(void) {
    TorqueKey key;
    torque_key_init(&key);

    torque_key_update(&key, false, UINT32_MAX - 100);
    torque_key_update(&key, true, UINT32_MAX - 100);
    torque_key_update(&key, true, 149);
    torque_key_update(&key, true, 150);
}

static void test_reports_endpoint_before_reversal_sample(void) {
    TorqueKey key;
    torque_key_init(&key);

    torque_key_update(&key, true, 0);
    torque_key_update(&key, true, 250);
    torque_key_update(&key, false, 251);
    assert(key.state_known);
    assert(key.inserted);
    assert(key.filter_position_ms == 2);
}

static void test_reports_removed_endpoint_before_reversal_sample(void) {
    TorqueKey key;
    torque_key_init(&key);

    torque_key_update(&key, false, 0);
    torque_key_update(&key, false, 250);
    torque_key_update(&key, true, 251);
    assert(key.state_known);
    assert(!key.inserted);
    assert(key.filter_position_ms == 498);
}

int main(void) {
    test_establishes_initial_inserted_state_from_midpoint();
    test_establishes_initial_removed_state_from_midpoint();
    test_filters_insertion_and_removal();
    test_opposite_samples_cancel_filter_travel();
    test_preserves_filter_across_counter_wrap();
    test_reports_endpoint_before_reversal_sample();
    test_reports_removed_endpoint_before_reversal_sample();
    return 0;
}
