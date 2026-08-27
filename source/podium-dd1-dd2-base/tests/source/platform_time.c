#include <assert.h>
#include <stdint.h>

#include "platform/time.h"

int main(void) {
    assert(platform_time_reached(100, 100));
    assert(platform_time_reached(101, 100));
    assert(!platform_time_reached(99, 100));
    assert(!platform_time_reached(UINT32_MAX, 0));
    assert(platform_time_reached(0, UINT32_MAX));
    assert(platform_time_reached(5, UINT32_MAX - 5));
    return 0;
}
