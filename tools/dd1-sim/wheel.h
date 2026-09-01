#ifndef OPENTEC_DD1_SIM_WHEEL_H
#define OPENTEC_DD1_SIM_WHEEL_H

#include <stdint.h>

enum {
    DD1_SIM_WHEEL_PACKET_SIZE = 33,
    DD1_SIM_WHEEL_OUTPUT_SIZE = 33,
};

typedef struct {
    uint8_t output[DD1_SIM_WHEEL_OUTPUT_SIZE];
    uint32_t buttons;
    uint32_t exchanges;
} Dd1SimWheel;

void dd1_sim_wheel_init(Dd1SimWheel *wheel);
void dd1_sim_wheel_set_buttons(Dd1SimWheel *wheel, uint32_t buttons);
void dd1_sim_wheel_response(const Dd1SimWheel *wheel, uint8_t response[DD1_SIM_WHEEL_PACKET_SIZE]);
void dd1_sim_wheel_accept_output(Dd1SimWheel *wheel,
                                 const uint8_t output[DD1_SIM_WHEEL_OUTPUT_SIZE]);
uint16_t dd1_sim_wheel_alternate_response(const Dd1SimWheel *wheel, uint16_t output_word);
uint8_t dd1_sim_wheel_checksum(const uint8_t packet[DD1_SIM_WHEEL_PACKET_SIZE]);

#endif
