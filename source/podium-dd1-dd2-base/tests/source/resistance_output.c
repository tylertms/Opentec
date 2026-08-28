#include "analog/resistance_output.h"

#include <assert.h>
#include <stdbool.h>

static void test_direct_output(void) {
    assert(resistance_output_compare(0, false, false) == 0);
    assert(resistance_output_compare(25, false, false) == 798);
    assert(resistance_output_compare(100, false, false) == 3192);
    assert(resistance_output_compare(101, false, false) == 3192);
}

static void test_inverted_output(void) {
    assert(resistance_output_compare(0, true, false) == 3192);
    assert(resistance_output_compare(25, true, false) == 2394);
    assert(resistance_output_compare(100, true, false) == 0);
    assert(resistance_output_compare(101, true, false) == 0);
}

static void test_disabled_output(void) {
    assert(resistance_output_compare(50, false, true) == 0);
    assert(resistance_output_compare(50, true, true) == 3192);
}

int main(void) {
    test_direct_output();
    test_inverted_output();
    test_disabled_output();
    return 0;
}
