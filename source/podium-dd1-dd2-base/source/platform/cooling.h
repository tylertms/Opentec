#ifndef OPENTEC_BASE_PLATFORM_COOLING_H
#define OPENTEC_BASE_PLATFORM_COOLING_H

#include <stdbool.h>
#include <stdint.h>

typedef enum {
    PLATFORM_FAN_PRIMARY,
    PLATFORM_FAN_SECONDARY,
} PlatformFan;

typedef struct {
    uint32_t previous_capture;
    uint32_t current_capture;
    bool present;
} PlatformFanTachometer;

void platform_cooling_init(bool inverted_pwm);
void platform_cooling_set_duty(uint16_t primary_percent, uint16_t secondary_percent,
                               bool outputs_disabled);
void platform_cooling_service(uint32_t now_ms);
bool platform_cooling_take_tachometer(PlatformFan fan, PlatformFanTachometer *tachometer);

#endif
