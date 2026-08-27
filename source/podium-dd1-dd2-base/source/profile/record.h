#ifndef OPENTEC_BASE_PROFILE_RECORD_H
#define OPENTEC_BASE_PROFILE_RECORD_H

#include <stdbool.h>
#include <stdint.h>

#include "profile/bank.h"

enum {
    TUNING_PROFILE_RECORD_VERSION = 1,
    TUNING_PROFILE_RECORD_HEADER_SIZE = 8,
    TUNING_PROFILE_RECORD_PROFILE_SIZE = 27,
    TUNING_PROFILE_RECORD_CHECKSUM_SIZE = 2,
    TUNING_PROFILE_RECORD_SIZE = TUNING_PROFILE_RECORD_HEADER_SIZE +
                                 TUNING_PROFILE_SLOT_COUNT * TUNING_PROFILE_RECORD_PROFILE_SIZE +
                                 TUNING_PROFILE_RECORD_CHECKSUM_SIZE,
};

bool tuning_profile_record_encode(const TuningProfileBank *bank,
                                  uint8_t output[TUNING_PROFILE_RECORD_SIZE]);
bool tuning_profile_record_decode(const uint8_t input[TUNING_PROFILE_RECORD_SIZE],
                                  TuningProfileBank *bank);

#endif
