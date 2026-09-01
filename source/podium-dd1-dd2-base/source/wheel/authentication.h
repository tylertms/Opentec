#ifndef OPENTEC_BASE_WHEEL_AUTHENTICATION_H
#define OPENTEC_BASE_WHEEL_AUTHENTICATION_H

#include <stdbool.h>
#include <stdint.h>

/**
 * @brief Sizes of attached-wheel authentication messages and cipher state.
 *
 * The exchange uses 32-byte content, AES-128 keys, and 16-byte counters with an eight-byte retry
 * sequence.
 */
enum {
    WHEEL_AUTHENTICATION_CONTENT_SIZE =
        32,                             /**< Number of bytes in one authentication content block. */
    WHEEL_AUTHENTICATION_KEY_SIZE = 16, /**< Number of bytes in one AES-128 key. */
    WHEEL_AUTHENTICATION_ROUND_KEY_SIZE =
        176, /**< Number of bytes in an AES-128 round-key schedule. */
    WHEEL_AUTHENTICATION_COUNTER_SIZE =
        16, /**< Number of bytes in one authentication counter block. */
    WHEEL_AUTHENTICATION_RETRY_COUNTER_SIZE =
        8, /**< Number of incrementing bytes in the retry counter. */
};

/**
 * @brief Stage of an attached-wheel authentication exchange.
 *
 * The stage selects whether the next message is interpreted as a challenge or encrypted proof.
 */
typedef enum {
    WHEEL_AUTHENTICATION_AWAITING_CHALLENGE, /**< The next message must contain a challenge. */
    WHEEL_AUTHENTICATION_AWAITING_PROOF,     /**< The next message must contain encrypted proof. */
} WheelAuthenticationStage;

/**
 * @brief Persistent state for an attached-wheel authentication exchange.
 *
 * The state retains both AES directions, their counters, the retry sequence, and the selected
 * wheel mode between protocol messages.
 */
typedef struct {
    uint8_t
        transmit_round_keys[WHEEL_AUTHENTICATION_ROUND_KEY_SIZE]; /**< Expanded transmit AES key. */
    uint8_t
        receive_round_keys[WHEEL_AUTHENTICATION_ROUND_KEY_SIZE]; /**< Expanded receive AES key. */
    uint8_t transmit_counter[WHEEL_AUTHENTICATION_COUNTER_SIZE]; /**< Counter used to encrypt
                                                                    responses. */
    uint8_t
        receive_counter[WHEEL_AUTHENTICATION_COUNTER_SIZE]; /**< Counter used to decrypt proofs. */
    uint8_t
        retry_counter[WHEEL_AUTHENTICATION_RETRY_COUNTER_SIZE]; /**< Big-endian retry sequence. */
    uint8_t wheel_mode; /**< Attached-wheel mode selecting the authentication key pair. */
    WheelAuthenticationStage stage; /**< Current challenge-or-proof stage. */
} WheelAuthentication;

/**
 * @brief Reports whether a wheel mode uses authentication.
 *
 * Checks whether the mode has a supported shared transmit and receive key pair.
 *
 * @param[in] wheel_mode Attached-wheel mode to test.
 * @return true when the mode requires authentication; false when it has no supported key pair.
 */
bool wheel_authentication_required(uint8_t wheel_mode);

/**
 * @brief Initializes authentication for an attached-wheel mode.
 *
 * Clears cipher schedules, counters, retry state, and stage state before setting the selected mode
 * and awaiting its challenge.
 *
 * @param[out] authentication Authentication exchange state to initialize.
 * @param[in] wheel_mode Selected attached-wheel mode.
 */
void wheel_authentication_init(WheelAuthentication *authentication, uint8_t wheel_mode);

/**
 * @brief Processes one authenticated attached-wheel message.
 *
 * Accepts a challenge or encrypted proof according to the current stage and prepares the response
 * content for the next protocol transfer.
 *
 * @param[in,out] authentication Authentication state for the selected wheel mode.
 * @param[in] request First WHEEL_AUTHENTICATION_CONTENT_SIZE bytes of the request.
 * @param[in] checksum_valid true when the request checksum matches its content.
 * @param[in,out] response First WHEEL_AUTHENTICATION_CONTENT_SIZE bytes of the persistent response.
 * @return true only after a correctly encrypted proof command is accepted; false for a challenge,
 * rejected checksum, or non-proof encrypted command.
 */
bool wheel_authentication_accept(WheelAuthentication *authentication,
                                 const uint8_t request[WHEEL_AUTHENTICATION_CONTENT_SIZE],
                                 bool checksum_valid,
                                 uint8_t response[WHEEL_AUTHENTICATION_CONTENT_SIZE]);

#endif
