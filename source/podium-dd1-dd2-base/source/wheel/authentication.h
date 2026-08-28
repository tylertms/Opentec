#ifndef OPENTEC_BASE_WHEEL_AUTHENTICATION_H
#define OPENTEC_BASE_WHEEL_AUTHENTICATION_H

#include <stdbool.h>
#include <stdint.h>

enum {
    WHEEL_AUTHENTICATION_CONTENT_SIZE = 32,
    WHEEL_AUTHENTICATION_KEY_SIZE = 16,
    WHEEL_AUTHENTICATION_ROUND_KEY_SIZE = 176,
    WHEEL_AUTHENTICATION_COUNTER_SIZE = 16,
    WHEEL_AUTHENTICATION_RETRY_COUNTER_SIZE = 8,
};

typedef enum {
    WHEEL_AUTHENTICATION_AWAITING_CHALLENGE,
    WHEEL_AUTHENTICATION_AWAITING_PROOF,
} WheelAuthenticationStage;

typedef struct {
    uint8_t transmit_round_keys[WHEEL_AUTHENTICATION_ROUND_KEY_SIZE];
    uint8_t receive_round_keys[WHEEL_AUTHENTICATION_ROUND_KEY_SIZE];
    uint8_t transmit_counter[WHEEL_AUTHENTICATION_COUNTER_SIZE];
    uint8_t receive_counter[WHEEL_AUTHENTICATION_COUNTER_SIZE];
    uint8_t retry_counter[WHEEL_AUTHENTICATION_RETRY_COUNTER_SIZE];
    uint8_t operating_mode;
    WheelAuthenticationStage stage;
} WheelAuthentication;

bool wheel_authentication_required(uint8_t operating_mode);
void wheel_authentication_init(WheelAuthentication *authentication, uint8_t operating_mode);
bool wheel_authentication_accept(WheelAuthentication *authentication,
                                 const uint8_t request[WHEEL_AUTHENTICATION_CONTENT_SIZE],
                                 bool checksum_valid,
                                 uint8_t response[WHEEL_AUTHENTICATION_CONTENT_SIZE]);

#endif
