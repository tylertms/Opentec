#include <assert.h>

#include "shifter/input.h"

static void test_first_contact_has_priority(void) {
    assert(shifter_sequential_pair_state(false, false) == SHIFTER_SEQUENTIAL_NONE);
    assert(shifter_sequential_pair_state(false, true) == SHIFTER_SEQUENTIAL_SECOND);
    assert(shifter_sequential_pair_state(true, false) == SHIFTER_SEQUENTIAL_FIRST);
    assert(shifter_sequential_pair_state(true, true) == SHIFTER_SEQUENTIAL_FIRST);
}

int main(void) {
    test_first_contact_has_priority();
    return 0;
}
