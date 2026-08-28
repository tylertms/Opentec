#include <assert.h>
#include <stdint.h>

#include "wheel/packet_remote_tuning.h"

static RemoteTuningResponse response(RemoteTuningResponseCode code, uint8_t value) {
    return (RemoteTuningResponse){
        .link = REMOTE_TUNING_LINK_LEGACY,
        .code = code,
        .value = value,
    };
}

static void encodes_active_state(void) {
    WheelPacketRemoteTuningOutput output;
    wheel_packet_remote_tuning_init(&output);
    uint8_t packet[WHEEL_PACKET_REMOTE_TUNING_SIZE] = {0};

    RemoteTuningResponse active = response(REMOTE_TUNING_RESPONSE_ACTIVE, 0x55);
    assert(wheel_packet_remote_tuning_queue(&output, &active));
    assert(wheel_packet_remote_tuning_pending(&output));
    assert(wheel_packet_remote_tuning_encode(&output, packet));
    assert(packet[0] == 0xa7 && packet[1] == 2 && packet[2] == 1);
    assert(!wheel_packet_remote_tuning_pending(&output));

    RemoteTuningResponse inactive = response(REMOTE_TUNING_RESPONSE_INACTIVE, 0x55);
    assert(wheel_packet_remote_tuning_queue(&output, &inactive));
    assert(wheel_packet_remote_tuning_encode(&output, packet));
    assert(packet[0] == 0xa7 && packet[1] == 2 && packet[2] == 0);
}

static void encodes_setup_and_refresh_values(void) {
    WheelPacketRemoteTuningOutput output;
    wheel_packet_remote_tuning_init(&output);
    uint8_t packet[WHEEL_PACKET_REMOTE_TUNING_SIZE] = {0};

    RemoteTuningResponse setup = response(REMOTE_TUNING_RESPONSE_SETUP, 5);
    assert(wheel_packet_remote_tuning_queue(&output, &setup));
    assert(wheel_packet_remote_tuning_encode(&output, packet));
    assert(packet[0] == 0xa7 && packet[1] == 4 && packet[2] == 5);

    RemoteTuningResponse refresh = response(REMOTE_TUNING_RESPONSE_REFRESH, 1);
    assert(wheel_packet_remote_tuning_queue(&output, &refresh));
    assert(wheel_packet_remote_tuning_encode(&output, packet));
    assert(packet[0] == 0xa7 && packet[1] == 5 && packet[2] == 1);
}

static void rejects_unsupported_responses(void) {
    WheelPacketRemoteTuningOutput output;
    wheel_packet_remote_tuning_init(&output);
    uint8_t packet[WHEEL_PACKET_REMOTE_TUNING_SIZE] = {0};
    RemoteTuningResponse none = response(REMOTE_TUNING_RESPONSE_NONE, 0);
    assert(!wheel_packet_remote_tuning_queue(&output, &none));
    assert(!wheel_packet_remote_tuning_encode(&output, packet));

    RemoteTuningResponse no_link = response(REMOTE_TUNING_RESPONSE_SETUP, 1);
    no_link.link = REMOTE_TUNING_LINK_NONE;
    assert(!wheel_packet_remote_tuning_queue(&output, &no_link));
}

int main(void) {
    encodes_active_state();
    encodes_setup_and_refresh_values();
    rejects_unsupported_responses();
    return 0;
}
