#include <assert.h>
#include <string.h>

#include "wheel.h"

int main(void) {
    Dd1SimWheel wheel;
    uint8_t packet[DD1_SIM_WHEEL_PACKET_SIZE];
    uint8_t output[DD1_SIM_WHEEL_OUTPUT_SIZE] = {0};

    dd1_sim_wheel_init(&wheel);
    dd1_sim_wheel_response(&wheel, packet);
    assert(packet[0] == 0);
    assert(packet[32] == dd1_sim_wheel_checksum(packet));

    dd1_sim_wheel_accept_output(&wheel, output);
    dd1_sim_wheel_accept_output(&wheel, output);
    dd1_sim_wheel_accept_output(&wheel, output);
    dd1_sim_wheel_response(&wheel, packet);
    assert(packet[0] == 0xa5);
    assert(packet[1] == 1);

    dd1_sim_wheel_set_buttons(&wheel, UINT32_C(0x563412));
    dd1_sim_wheel_accept_output(&wheel, output);
    dd1_sim_wheel_response(&wheel, packet);
    assert(packet[2] == 0x12);
    assert(packet[3] == 0x34);
    assert(packet[4] == 0x56);
    assert(packet[32] == dd1_sim_wheel_checksum(packet));
    assert(memcmp(wheel.output, output, sizeof(output)) == 0);
    return 0;
}
