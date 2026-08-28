#include "wheel/authentication.h"

#include <stdbool.h>
#include <stdint.h>

enum {
    AES_BLOCK_SIZE = 16,
    AES_ROUNDS = 10,
    AUTHENTICATION_COMMAND_CHALLENGE = 0xa6,
    AUTHENTICATION_COMMAND_PROOF = 0xa7,
    AUTHENTICATION_RESPONSE_COMMAND = 0xa5,
    AUTHENTICATION_CHALLENGE_MARKER = 0xaa,
    AUTHENTICATION_NONCE_OFFSET = 2,
    AUTHENTICATION_NONCE_SIZE = 8,
    AUTHENTICATION_REPLY_PREFIX_SIZE = 10,
    KEY_PAIR_UNSUPPORTED = 0xff,
};

typedef struct {
    uint8_t transmit[WHEEL_AUTHENTICATION_KEY_SIZE];
    uint8_t receive[WHEEL_AUTHENTICATION_KEY_SIZE];
} WheelAuthenticationKeys;

static const uint8_t aes_s_box[256] = {
    0x63, 0x7c, 0x77, 0x7b, 0xf2, 0x6b, 0x6f, 0xc5, 0x30, 0x01, 0x67, 0x2b, 0xfe, 0xd7, 0xab, 0x76,
    0xca, 0x82, 0xc9, 0x7d, 0xfa, 0x59, 0x47, 0xf0, 0xad, 0xd4, 0xa2, 0xaf, 0x9c, 0xa4, 0x72, 0xc0,
    0xb7, 0xfd, 0x93, 0x26, 0x36, 0x3f, 0xf7, 0xcc, 0x34, 0xa5, 0xe5, 0xf1, 0x71, 0xd8, 0x31, 0x15,
    0x04, 0xc7, 0x23, 0xc3, 0x18, 0x96, 0x05, 0x9a, 0x07, 0x12, 0x80, 0xe2, 0xeb, 0x27, 0xb2, 0x75,
    0x09, 0x83, 0x2c, 0x1a, 0x1b, 0x6e, 0x5a, 0xa0, 0x52, 0x3b, 0xd6, 0xb3, 0x29, 0xe3, 0x2f, 0x84,
    0x53, 0xd1, 0x00, 0xed, 0x20, 0xfc, 0xb1, 0x5b, 0x6a, 0xcb, 0xbe, 0x39, 0x4a, 0x4c, 0x58, 0xcf,
    0xd0, 0xef, 0xaa, 0xfb, 0x43, 0x4d, 0x33, 0x85, 0x45, 0xf9, 0x02, 0x7f, 0x50, 0x3c, 0x9f, 0xa8,
    0x51, 0xa3, 0x40, 0x8f, 0x92, 0x9d, 0x38, 0xf5, 0xbc, 0xb6, 0xda, 0x21, 0x10, 0xff, 0xf3, 0xd2,
    0xcd, 0x0c, 0x13, 0xec, 0x5f, 0x97, 0x44, 0x17, 0xc4, 0xa7, 0x7e, 0x3d, 0x64, 0x5d, 0x19, 0x73,
    0x60, 0x81, 0x4f, 0xdc, 0x22, 0x2a, 0x90, 0x88, 0x46, 0xee, 0xb8, 0x14, 0xde, 0x5e, 0x0b, 0xdb,
    0xe0, 0x32, 0x3a, 0x0a, 0x49, 0x06, 0x24, 0x5c, 0xc2, 0xd3, 0xac, 0x62, 0x91, 0x95, 0xe4, 0x79,
    0xe7, 0xc8, 0x37, 0x6d, 0x8d, 0xd5, 0x4e, 0xa9, 0x6c, 0x56, 0xf4, 0xea, 0x65, 0x7a, 0xae, 0x08,
    0xba, 0x78, 0x25, 0x2e, 0x1c, 0xa6, 0xb4, 0xc6, 0xe8, 0xdd, 0x74, 0x1f, 0x4b, 0xbd, 0x8b, 0x8a,
    0x70, 0x3e, 0xb5, 0x66, 0x48, 0x03, 0xf6, 0x0e, 0x61, 0x35, 0x57, 0xb9, 0x86, 0xc1, 0x1d, 0x9e,
    0xe1, 0xf8, 0x98, 0x11, 0x69, 0xd9, 0x8e, 0x94, 0x9b, 0x1e, 0x87, 0xe9, 0xce, 0x55, 0x28, 0xdf,
    0x8c, 0xa1, 0x89, 0x0d, 0xbf, 0xe6, 0x42, 0x68, 0x41, 0x99, 0x2d, 0x0f, 0xb0, 0x54, 0xbb, 0x16,
};

static const WheelAuthenticationKeys key_pairs[] = {
    {{0x70, 0x32, 0x73, 0x35, 0x76, 0x38, 0x79, 0x2f, 0x42, 0x3f, 0x45, 0x28, 0x48, 0x2b, 0x4d,
      0x62},
     {0x45, 0x28, 0x48, 0x2b, 0x4d, 0x62, 0x51, 0x65, 0x54, 0x68, 0x57, 0x6d, 0x5a, 0x71, 0x34,
      0x74}},
    {{0x28, 0x48, 0x2b, 0x4d, 0x62, 0x51, 0x65, 0x54, 0x68, 0x57, 0x6d, 0x59, 0x71, 0x33, 0x74,
      0x36},
     {0x35, 0x75, 0x38, 0x78, 0x2f, 0x41, 0x25, 0x44, 0x2a, 0x47, 0x2d, 0x4b, 0x61, 0x50, 0x64,
      0x53}},
    {{0x4f, 0x8d, 0x86, 0x45, 0x28, 0xb6, 0x3c, 0x15, 0x10, 0x3a, 0x49, 0x23, 0xd5, 0x70, 0x44,
      0x39},
     {0x7b, 0x83, 0x4b, 0x41, 0x22, 0x18, 0xce, 0x1d, 0xd2, 0x16, 0xf4, 0x98, 0x49, 0x53, 0xda,
      0x64}},
    {{0x8e, 0x69, 0xc2, 0xa9, 0x9f, 0x42, 0x3a, 0xa2, 0x4c, 0x93, 0x5f, 0x29, 0xb2, 0x88, 0x98,
      0x7c},
     {0xbf, 0x8d, 0x2e, 0x66, 0x9a, 0x43, 0xd8, 0x82, 0x3e, 0x53, 0xb3, 0x19, 0x90, 0xa4, 0xd0,
      0x1a}},
    {{0xa3, 0xcb, 0x61, 0x6b, 0xc2, 0xb9, 0x10, 0xfd, 0x42, 0x27, 0x4b, 0xe3, 0x37, 0x8a, 0x12,
      0x25},
     {0x42, 0xfe, 0xd3, 0x5b, 0x48, 0x7c, 0x8a, 0x27, 0x74, 0xba, 0x7f, 0x84, 0xa6, 0xd3, 0x37,
      0x70}},
    {{0x21, 0x7a, 0x25, 0x43, 0x2a, 0x46, 0x2d, 0x4a, 0x61, 0x4e, 0x64, 0x52, 0x67, 0x55, 0x6b,
      0x58},
     {0x37, 0x78, 0x21, 0x41, 0x25, 0x44, 0x2a, 0x47, 0x2d, 0x4b, 0x61, 0x50, 0x64, 0x53, 0x67,
      0x56}},
    {{0x4e, 0x64, 0x52, 0x67, 0x55, 0x6b, 0x58, 0x70, 0x32, 0x73, 0x35, 0x76, 0x38, 0x79, 0x2f,
      0x42},
     {0x4a, 0x61, 0x4e, 0x64, 0x52, 0x67, 0x55, 0x6b, 0x58, 0x70, 0x32, 0x73, 0x35, 0x76, 0x38,
      0x79}},
    {{0x41, 0x3f, 0x44, 0x28, 0x47, 0x2b, 0x4b, 0x62, 0x50, 0x64, 0x53, 0x67, 0x56, 0x6b, 0x59,
      0x70},
     {0x71, 0x34, 0x74, 0x36, 0x77, 0x39, 0x7a, 0x24, 0x43, 0x26, 0x46, 0x29, 0x4a, 0x40, 0x4e,
      0x63}},
    {{0x2b, 0x4b, 0x62, 0x50, 0x65, 0x53, 0x68, 0x56, 0x6d, 0x59, 0x71, 0x33, 0x74, 0x36, 0x77,
      0x39},
     {0x66, 0x55, 0x6a, 0x58, 0x6e, 0x32, 0x72, 0x35, 0x75, 0x38, 0x78, 0x2f, 0x41, 0x3f, 0x44,
      0x28}},
    {{0x17, 0xa2, 0x37, 0x05, 0xd3, 0x6b, 0x81, 0x6a, 0x72, 0xf1, 0x21, 0x45, 0x33, 0x99, 0x17,
      0x36},
     {0x96, 0xc0, 0xec, 0xa1, 0x74, 0xaa, 0xe2, 0x54, 0x88, 0x1f, 0x10, 0x2d, 0xa9, 0xaf, 0x84,
      0xa3}},
    {{0x1d, 0x72, 0x6a, 0x44, 0x87, 0x88, 0x11, 0x37, 0xe8, 0xf4, 0xe5, 0x10, 0x29, 0xb6, 0x80,
      0x72},
     {0x65, 0x21, 0x93, 0xc3, 0xb2, 0x6c, 0x54, 0xe1, 0x59, 0x83, 0x55, 0xe3, 0x8a, 0x97, 0x84,
      0xa4}},
};

static const uint8_t mode_key_pair[] = {
    0,
    0,
    1,
    KEY_PAIR_UNSUPPORTED,
    10,
    9,
    3,
    2,
    4,
    6,
    5,
    7,
    8,
    9,
    KEY_PAIR_UNSUPPORTED,
    KEY_PAIR_UNSUPPORTED,
    KEY_PAIR_UNSUPPORTED,
    0,
    0,
    0,
    3,
};

static void clear_bytes(uint8_t *data, uint8_t length) {
    for (uint8_t index = 0; index < length; index++) {
        data[index] = 0;
    }
}

static void copy_bytes(uint8_t *target, const uint8_t *source, uint8_t length) {
    for (uint8_t index = 0; index < length; index++) {
        target[index] = source[index];
    }
}

static uint8_t multiply_by_x(uint8_t value) {
    return (uint8_t)((value << 1) ^ ((value & 0x80u) != 0 ? 0x1bu : 0));
}

static void expand_key(uint8_t round_keys[WHEEL_AUTHENTICATION_ROUND_KEY_SIZE],
                       const uint8_t key[WHEEL_AUTHENTICATION_KEY_SIZE]) {
    copy_bytes(round_keys, key, WHEEL_AUTHENTICATION_KEY_SIZE);
    uint8_t generated = WHEEL_AUTHENTICATION_KEY_SIZE;
    uint8_t round_constant = 1;
    uint8_t word[4];

    while (generated < WHEEL_AUTHENTICATION_ROUND_KEY_SIZE) {
        copy_bytes(word, &round_keys[generated - 4], sizeof(word));
        if ((generated % WHEEL_AUTHENTICATION_KEY_SIZE) == 0) {
            uint8_t first = word[0];
            word[0] = (uint8_t)(aes_s_box[word[1]] ^ round_constant);
            word[1] = aes_s_box[word[2]];
            word[2] = aes_s_box[word[3]];
            word[3] = aes_s_box[first];
            round_constant = multiply_by_x(round_constant);
        }
        for (uint8_t index = 0; index < sizeof(word); index++) {
            round_keys[generated] =
                round_keys[generated - WHEEL_AUTHENTICATION_KEY_SIZE] ^ word[index];
            generated++;
        }
    }
}

static void add_round_key(uint8_t state[AES_BLOCK_SIZE], const uint8_t *round_key) {
    for (uint8_t index = 0; index < AES_BLOCK_SIZE; index++) {
        state[index] ^= round_key[index];
    }
}

static void substitute_bytes(uint8_t state[AES_BLOCK_SIZE]) {
    for (uint8_t index = 0; index < AES_BLOCK_SIZE; index++) {
        state[index] = aes_s_box[state[index]];
    }
}

static void shift_rows(uint8_t state[AES_BLOCK_SIZE]) {
    uint8_t shifted[AES_BLOCK_SIZE];
    shifted[0] = state[0];
    shifted[1] = state[5];
    shifted[2] = state[10];
    shifted[3] = state[15];
    shifted[4] = state[4];
    shifted[5] = state[9];
    shifted[6] = state[14];
    shifted[7] = state[3];
    shifted[8] = state[8];
    shifted[9] = state[13];
    shifted[10] = state[2];
    shifted[11] = state[7];
    shifted[12] = state[12];
    shifted[13] = state[1];
    shifted[14] = state[6];
    shifted[15] = state[11];
    copy_bytes(state, shifted, AES_BLOCK_SIZE);
}

static void mix_columns(uint8_t state[AES_BLOCK_SIZE]) {
    for (uint8_t column = 0; column < AES_BLOCK_SIZE; column += 4) {
        uint8_t first = state[column];
        uint8_t combined =
            state[column] ^ state[column + 1] ^ state[column + 2] ^ state[column + 3];
        state[column] ^= combined ^ multiply_by_x(state[column] ^ state[column + 1]);
        state[column + 1] ^= combined ^ multiply_by_x(state[column + 1] ^ state[column + 2]);
        state[column + 2] ^= combined ^ multiply_by_x(state[column + 2] ^ state[column + 3]);
        state[column + 3] ^= combined ^ multiply_by_x(state[column + 3] ^ first);
    }
}

static void encrypt_block(const uint8_t round_keys[WHEEL_AUTHENTICATION_ROUND_KEY_SIZE],
                          uint8_t state[AES_BLOCK_SIZE]) {
    add_round_key(state, round_keys);
    for (uint8_t round = 1; round < AES_ROUNDS; round++) {
        substitute_bytes(state);
        shift_rows(state);
        mix_columns(state);
        add_round_key(state, &round_keys[round * AES_BLOCK_SIZE]);
    }
    substitute_bytes(state);
    shift_rows(state);
    add_round_key(state, &round_keys[AES_ROUNDS * AES_BLOCK_SIZE]);
}

/**
 * @brief Advances an authentication counter.
 *
 * Increments the big-endian eight-byte sequence at the start of the counter.
 *
 * @param[in,out] counter Counter whose leading eight bytes are incremented.
 */
static void increment_counter(uint8_t *counter) {
    for (uint8_t index = WHEEL_AUTHENTICATION_RETRY_COUNTER_SIZE; index > 0; index--) {
        counter[index - 1]++;
        if (counter[index - 1] != 0) {
            return;
        }
    }
}

static void xcrypt_content(const uint8_t round_keys[WHEEL_AUTHENTICATION_ROUND_KEY_SIZE],
                           uint8_t counter[WHEEL_AUTHENTICATION_COUNTER_SIZE],
                           uint8_t content[WHEEL_AUTHENTICATION_CONTENT_SIZE]) {
    for (uint8_t offset = 0; offset < WHEEL_AUTHENTICATION_CONTENT_SIZE; offset += AES_BLOCK_SIZE) {
        uint8_t stream[AES_BLOCK_SIZE];
        copy_bytes(stream, counter, AES_BLOCK_SIZE);
        encrypt_block(round_keys, stream);
        increment_counter(counter);
        for (uint8_t index = 0; index < AES_BLOCK_SIZE; index++) {
            content[offset + index] ^= stream[index];
        }
    }
}

static const WheelAuthenticationKeys *keys_for_mode(uint8_t wheel_mode) {
    if (wheel_mode < 0x0a || wheel_mode > 0x1e) {
        return 0;
    }
    uint8_t key_pair = mode_key_pair[wheel_mode - 0x0a];
    return key_pair == KEY_PAIR_UNSUPPORTED ? 0 : &key_pairs[key_pair];
}

static bool initialize_ciphers(WheelAuthentication *authentication,
                               const uint8_t request[WHEEL_AUTHENTICATION_CONTENT_SIZE]) {
    const WheelAuthenticationKeys *keys = keys_for_mode(authentication->wheel_mode);
    if (keys == 0) {
        return false;
    }
    copy_bytes(authentication->transmit_counter, &request[AUTHENTICATION_NONCE_OFFSET],
               AUTHENTICATION_NONCE_SIZE);
    clear_bytes(&authentication->transmit_counter[AUTHENTICATION_NONCE_SIZE],
                WHEEL_AUTHENTICATION_COUNTER_SIZE - AUTHENTICATION_NONCE_SIZE);
    copy_bytes(authentication->receive_counter, authentication->transmit_counter,
               WHEEL_AUTHENTICATION_COUNTER_SIZE);
    expand_key(authentication->transmit_round_keys, keys->transmit);
    expand_key(authentication->receive_round_keys, keys->receive);
    return true;
}

static void advance_retry_counter(WheelAuthentication *authentication) {
    increment_counter(authentication->retry_counter);
}

static bool accept_challenge(WheelAuthentication *authentication,
                             const uint8_t request[WHEEL_AUTHENTICATION_CONTENT_SIZE],
                             bool checksum_valid,
                             uint8_t response[WHEEL_AUTHENTICATION_CONTENT_SIZE]) {
    if (!checksum_valid) {
        advance_retry_counter(authentication);
        clear_bytes(&response[AUTHENTICATION_NONCE_OFFSET],
                    WHEEL_AUTHENTICATION_CONTENT_SIZE - AUTHENTICATION_NONCE_OFFSET);
        return false;
    }

    response[1] = AUTHENTICATION_CHALLENGE_MARKER;
    clear_bytes(&response[AUTHENTICATION_NONCE_OFFSET],
                WHEEL_AUTHENTICATION_CONTENT_SIZE - AUTHENTICATION_NONCE_OFFSET);
    if (request[0] != AUTHENTICATION_COMMAND_CHALLENGE) {
        return false;
    }

    if (!initialize_ciphers(authentication, request)) {
        return false;
    }
    xcrypt_content(authentication->transmit_round_keys, authentication->transmit_counter, response);
    authentication->stage = WHEEL_AUTHENTICATION_AWAITING_PROOF;
    return false;
}

static bool accept_proof(WheelAuthentication *authentication,
                         const uint8_t request[WHEEL_AUTHENTICATION_CONTENT_SIZE],
                         bool checksum_valid, uint8_t response[WHEEL_AUTHENTICATION_CONTENT_SIZE]) {
    if (!checksum_valid) {
        advance_retry_counter(authentication);
        clear_bytes(&response[AUTHENTICATION_REPLY_PREFIX_SIZE],
                    WHEEL_AUTHENTICATION_CONTENT_SIZE - AUTHENTICATION_REPLY_PREFIX_SIZE);
        xcrypt_content(authentication->transmit_round_keys, authentication->transmit_counter,
                       response);
        return false;
    }

    uint8_t decoded[WHEEL_AUTHENTICATION_CONTENT_SIZE];
    copy_bytes(decoded, request, WHEEL_AUTHENTICATION_CONTENT_SIZE);
    xcrypt_content(authentication->receive_round_keys, authentication->receive_counter, decoded);
    if (decoded[0] == AUTHENTICATION_COMMAND_PROOF) {
        clear_bytes(&response[AUTHENTICATION_NONCE_OFFSET],
                    WHEEL_AUTHENTICATION_CONTENT_SIZE - AUTHENTICATION_NONCE_OFFSET);
        return true;
    }

    response[0] = AUTHENTICATION_RESPONSE_COMMAND;
    response[1] = 0;
    copy_bytes(&response[AUTHENTICATION_NONCE_OFFSET], &decoded[AUTHENTICATION_NONCE_OFFSET],
               AUTHENTICATION_NONCE_SIZE);
    clear_bytes(&response[AUTHENTICATION_REPLY_PREFIX_SIZE],
                WHEEL_AUTHENTICATION_CONTENT_SIZE - AUTHENTICATION_REPLY_PREFIX_SIZE);
    xcrypt_content(authentication->transmit_round_keys, authentication->transmit_counter, response);
    return false;
}

/**
 * @brief Reports whether a wheel mode uses the authentication exchange.
 *
 * Selects modes with a supported authentication key pair.
 *
 * @param[in] wheel_mode Selected attached-wheel mode.
 * @return True when the mode requires authentication.
 */
bool wheel_authentication_required(uint8_t wheel_mode) { return keys_for_mode(wheel_mode) != 0; }

/**
 * @brief Initializes authentication for an attached-wheel mode.
 *
 * Clears both cipher schedules, counters, and retry state before awaiting a challenge.
 *
 * @param[out] authentication Authentication exchange state.
 * @param[in] wheel_mode Selected attached-wheel mode.
 */
void wheel_authentication_init(WheelAuthentication *authentication, uint8_t wheel_mode) {
    clear_bytes(authentication->transmit_round_keys, WHEEL_AUTHENTICATION_ROUND_KEY_SIZE);
    clear_bytes(authentication->receive_round_keys, WHEEL_AUTHENTICATION_ROUND_KEY_SIZE);
    clear_bytes(authentication->transmit_counter, WHEEL_AUTHENTICATION_COUNTER_SIZE);
    clear_bytes(authentication->receive_counter, WHEEL_AUTHENTICATION_COUNTER_SIZE);
    clear_bytes(authentication->retry_counter, WHEEL_AUTHENTICATION_RETRY_COUNTER_SIZE);
    authentication->wheel_mode = wheel_mode;
    authentication->stage = WHEEL_AUTHENTICATION_AWAITING_CHALLENGE;
}

/**
 * @brief Processes one authenticated attached-wheel message.
 *
 * Accepts the challenge or encrypted proof for the current exchange stage and prepares the next
 * response content.
 *
 * @param[in,out] authentication Authentication state for the selected wheel mode.
 * @param[in] request First 32 bytes of the attached-wheel request.
 * @param[in] checksum_valid True when the request checksum matches its content.
 * @param[in,out] response First 32 bytes of the persistent attached-wheel response.
 * @return True after a correctly encrypted proof command is received.
 */
bool wheel_authentication_accept(WheelAuthentication *authentication,
                                 const uint8_t request[WHEEL_AUTHENTICATION_CONTENT_SIZE],
                                 bool checksum_valid,
                                 uint8_t response[WHEEL_AUTHENTICATION_CONTENT_SIZE]) {
    return authentication->stage == WHEEL_AUTHENTICATION_AWAITING_CHALLENGE
               ? accept_challenge(authentication, request, checksum_valid, response)
               : accept_proof(authentication, request, checksum_valid, response);
}
