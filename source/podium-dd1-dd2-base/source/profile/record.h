#ifndef OPENTEC_BASE_PROFILE_RECORD_H
#define OPENTEC_BASE_PROFILE_RECORD_H

#include <stdbool.h>
#include <stdint.h>

#include "profile/bank.h"

/** @brief Fixed wire format sizes for retained tuning-profile records. */
enum {
    TUNING_PROFILE_RECORD_VERSION = 2,       /**< Current encoded record version. */
    TUNING_PROFILE_RECORD_HEADER_SIZE = 8,   /**< Header bytes before profile data. */
    TUNING_PROFILE_RECORD_PROFILE_SIZE = 27, /**< Encoded bytes per tuning profile. */
    TUNING_PROFILE_RECORD_CHECKSUM_SIZE = 2, /**< CRC-16 bytes at the record end. */
    TUNING_PROFILE_RECORD_SIZE = TUNING_PROFILE_RECORD_HEADER_SIZE +
                                 TUNING_PROFILE_SLOT_COUNT * TUNING_PROFILE_RECORD_PROFILE_SIZE +
                                 TUNING_PROFILE_RECORD_CHECKSUM_SIZE, /**< Total encoded bytes. */
};

/**
 * @brief Encodes a complete profile bank.
 *
 * Writes the versioned header, all retained profiles, and the trailing CRC-16 checksum.
 *
 * @param[in] bank Profile bank to encode.
 * @param[out] output Buffer receiving TUNING_PROFILE_RECORD_SIZE bytes.
 * @return true when both slot indexes are valid and encoding completes; false otherwise.
 */
bool tuning_profile_record_encode(const TuningProfileBank *bank,
                                  uint8_t output[TUNING_PROFILE_RECORD_SIZE]);

/**
 * @brief Decodes a complete profile bank.
 *
 * Validates the magic, supported version, slot count, slot indexes, and CRC before replacing bank.
 *
 * @param[in] input Buffer containing TUNING_PROFILE_RECORD_SIZE encoded bytes.
 * @param[out] bank Profile bank receiving the decoded and normalized values.
 * @return true when input is valid and decoded; false when validation fails.
 */
bool tuning_profile_record_decode(const uint8_t input[TUNING_PROFILE_RECORD_SIZE],
                                  TuningProfileBank *bank);

#endif
