#include "platform/time.h"

#include <stdbool.h>
#include <stdint.h>

bool platform_time_reached(uint32_t now, uint32_t deadline) {
    return (int32_t)(now - deadline) >= 0;
}
