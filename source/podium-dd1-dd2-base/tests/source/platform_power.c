#include <assert.h>
#include <stdbool.h>
#include <xc.h>

#include "platform/power.h"

static void test_preserves_startup_latch(void) {
    LATD = 0x0100;
    TRISD = 0;
    platform_power_init();
    assert((LATD & 0x0100u) != 0);
    assert((TRISD & 0x0200u) != 0);
    assert((TRISD & 0x0100u) == 0);
}

static void test_controls_latch(void) {
    platform_power_latch_set(true);
    assert((LATD & 0x0100u) != 0);
    platform_power_latch_set(false);
    assert((LATD & 0x0100u) == 0);
}

int main(void) {
    test_preserves_startup_latch();
    test_controls_latch();
    return 0;
}
