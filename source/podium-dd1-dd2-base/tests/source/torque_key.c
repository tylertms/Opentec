#include "board/torque_key.h"

#include <assert.h>
#include <stdint.h>

static void test_reports_key_present_on_first_sample(void) {
    TorqueKey key;
    torque_key_init(&key);

    assert(torque_key_update(&key, true, 10) == TORQUE_KEY_EVENT_INSERTED);
    assert(key.inserted);
    assert(torque_key_update(&key, true, 1000) == TORQUE_KEY_EVENT_NONE);
}

static void test_filters_insertion_and_removal(void) {
    TorqueKey key;
    torque_key_init(&key);

    assert(torque_key_update(&key, false, 0) == TORQUE_KEY_EVENT_NONE);
    assert(torque_key_update(&key, true, 499) == TORQUE_KEY_EVENT_NONE);
    assert(torque_key_update(&key, true, 500) == TORQUE_KEY_EVENT_INSERTED);
    assert(torque_key_update(&key, false, 999) == TORQUE_KEY_EVENT_NONE);
    assert(torque_key_update(&key, false, 1000) == TORQUE_KEY_EVENT_REMOVED);
    assert(!key.inserted);
}

static void test_opposite_samples_cancel_filter_travel(void) {
    TorqueKey key;
    torque_key_init(&key);

    assert(torque_key_update(&key, false, 0) == TORQUE_KEY_EVENT_NONE);
    assert(torque_key_update(&key, true, 100) == TORQUE_KEY_EVENT_NONE);
    assert(torque_key_update(&key, false, 200) == TORQUE_KEY_EVENT_NONE);
    assert(torque_key_update(&key, true, 699) == TORQUE_KEY_EVENT_NONE);
    assert(torque_key_update(&key, true, 700) == TORQUE_KEY_EVENT_INSERTED);
}

static void test_preserves_filter_across_counter_wrap(void) {
    TorqueKey key;
    torque_key_init(&key);

    assert(torque_key_update(&key, false, UINT32_MAX - 100) == TORQUE_KEY_EVENT_NONE);
    assert(torque_key_update(&key, true, UINT32_MAX - 100) == TORQUE_KEY_EVENT_NONE);
    assert(torque_key_update(&key, true, 399) == TORQUE_KEY_EVENT_INSERTED);
}

int main(void) {
    test_reports_key_present_on_first_sample();
    test_filters_insertion_and_removal();
    test_opposite_samples_cancel_filter_travel();
    test_preserves_filter_across_counter_wrap();
    return 0;
}
