#include <assert.h>
#include <stdint.h>
#include <string.h>

#include "motor/serial_packet.h"

static void test_encodes_and_decodes_fixed_packet(void) {
    static const uint8_t payload[] = {0x20, 0x01, 0x02};
    uint8_t encoded[MOTOR_SERIAL_PACKET_SIZE];
    MotorSerialPacket decoded;

    assert(motor_serial_packet_encode(4 | MOTOR_SERIAL_PACKET_FINAL_FRAGMENT, 0x2a, payload,
                                      sizeof(payload), encoded));
    assert(encoded[0] == 0x7b && encoded[1] == 0x44 && encoded[2] == 0x2a && encoded[3] == 3);
    assert(memcmp(encoded + 4, payload, sizeof(payload)) == 0);
    for (uint8_t index = 7; index < 61; index++) {
        assert(encoded[index] == 0);
    }
    assert(encoded[61] == 0x9e && encoded[62] == 0xe6 && encoded[63] == 0x7d);

    assert(motor_serial_packet_decode(encoded, &decoded) == MOTOR_SERIAL_PACKET_VALID);
    assert(decoded.type_flags == 0x44 && decoded.sequence == 0x2a &&
           decoded.payload_length == sizeof(payload));
    assert(memcmp(decoded.payload, payload, sizeof(payload)) == 0);
}

static void test_rejects_invalid_packets(void) {
    uint8_t encoded[MOTOR_SERIAL_PACKET_SIZE];
    MotorSerialPacket decoded;
    assert(motor_serial_packet_encode(0, 0, 0, 0, encoded));

    encoded[0] = 0;
    assert(motor_serial_packet_decode(encoded, &decoded) == MOTOR_SERIAL_PACKET_INVALID_BOUNDARY);
    encoded[0] = 0x7b;
    encoded[3] = MOTOR_SERIAL_PACKET_MAX_PAYLOAD_SIZE + 1;
    assert(motor_serial_packet_decode(encoded, &decoded) == MOTOR_SERIAL_PACKET_INVALID_LENGTH);
    encoded[3] = 0;
    encoded[61] ^= 1;
    assert(motor_serial_packet_decode(encoded, &decoded) == MOTOR_SERIAL_PACKET_INVALID_CHECKSUM);

    uint8_t payload[MOTOR_SERIAL_PACKET_MAX_PAYLOAD_SIZE + 1] = {0};
    assert(!motor_serial_packet_encode(0, 0, payload, sizeof(payload), encoded));
    assert(!motor_serial_packet_encode(0, 0, 0, 1, encoded));
}

int main(void) {
    test_encodes_and_decodes_fixed_packet();
    test_rejects_invalid_packets();
    return 0;
}
