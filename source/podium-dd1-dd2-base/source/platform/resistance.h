#ifndef OPENTEC_BASE_PLATFORM_RESISTANCE_H
#define OPENTEC_BASE_PLATFORM_RESISTANCE_H

#include <stdbool.h>
#include <stdint.h>

typedef enum {
    PLATFORM_RESISTANCE_PRIMARY,
    PLATFORM_RESISTANCE_SECONDARY,
} PlatformResistanceChannel;

typedef struct {
    uint32_t previous_capture;
    uint32_t current_capture;
    bool present;
} PlatformResistanceCapture;

void platform_resistance_init(bool inverted_pwm);
void platform_resistance_set_duty(uint16_t primary_percent, uint16_t secondary_percent,
                                  bool outputs_disabled);
void platform_resistance_service(uint32_t now_ms);
bool platform_resistance_take_capture(PlatformResistanceChannel channel,
                                      PlatformResistanceCapture *capture);

#endif
