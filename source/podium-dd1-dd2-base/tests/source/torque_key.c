#include "board/torque_key.h"

#include <assert.h>
#include <stdint.h>

static void test_establishes_initial_inserted_state_from_midpoint(void) {
    TorqueKey key;
    torque_key_init(&key);

    assert(torque_key_update(&key, true, 10) == TORQUE_KEY_EVENT_NONE);
    assert(torque_key_update(&key, true, 259) == TORQUE_KEY_EVENT_NONE);
    assert(torque_key_update(&key, true, 260) == TORQUE_KEY_EVENT_INSERTED);
    assert(key.inserted);
    assert(torque_key_update(&key, true, 1000) == TORQUE_KEY_EVENT_NONE);
}

static void test_establishes_initial_removed_state_from_midpoint(void) {
    TorqueKey key;
    torque_key_init(&key);

    assert(torque_key_update(&key, false, 0) == TORQUE_KEY_EVENT_NONE);
    assert(torque_key_update(&key, false, 249) == TORQUE_KEY_EVENT_NONE);
    assert(torque_key_update(&key, false, 250) == TORQUE_KEY_EVENT_REMOVED);
    assert(!key.inserted);
}

static void test_filters_insertion_and_removal(void) {
    TorqueKey key;
    torque_key_init(&key);

    assert(torque_key_update(&key, false, 0) == TORQUE_KEY_EVENT_NONE);
    assert(torque_key_update(&key, false, 250) == TORQUE_KEY_EVENT_REMOVED);
    assert(torque_key_update(&key, true, 749) == TORQUE_KEY_EVENT_NONE);
    assert(torque_key_update(&key, true, 750) == TORQUE_KEY_EVENT_INSERTED);
    assert(torque_key_update(&key, false, 1249) == TORQUE_KEY_EVENT_NONE);
    assert(torque_key_update(&key, false, 1250) == TORQUE_KEY_EVENT_REMOVED);
}

static void test_opposite_samples_cancel_filter_travel(void) {
    TorqueKey key;
    torque_key_init(&key);

    assert(torque_key_update(&key, false, 0) == TORQUE_KEY_EVENT_NONE);
    assert(torque_key_update(&key, true, 100) == TORQUE_KEY_EVENT_NONE);
    assert(torque_key_update(&key, false, 200) == TORQUE_KEY_EVENT_NONE);
    assert(torque_key_update(&key, true, 449) == TORQUE_KEY_EVENT_NONE);
    assert(torque_key_update(&key, true, 450) == TORQUE_KEY_EVENT_INSERTED);
}

static void test_preserves_filter_across_counter_wrap(void) {
    TorqueKey key;
    torque_key_init(&key);

    assert(torque_key_update(&key, false, UINT32_MAX - 100) == TORQUE_KEY_EVENT_NONE);
    assert(torque_key_update(&key, true, UINT32_MAX - 100) == TORQUE_KEY_EVENT_NONE);
    assert(torque_key_update(&key, true, 149) == TORQUE_KEY_EVENT_INSERTED);
}

int main(void) {
    test_establishes_initial_inserted_state_from_midpoint();
    test_establishes_initial_removed_state_from_midpoint();
    test_filters_insertion_and_removal();
    test_opposite_samples_cancel_filter_travel();
    test_preserves_filter_across_counter_wrap();
    return 0;
}
