#ifndef OPENTEC_BASE_REMOTE_TUNING_RESPONSE_H
#define OPENTEC_BASE_REMOTE_TUNING_RESPONSE_H

#include <stdint.h>

enum {
    REMOTE_TUNING_RECORD_DATA_SIZE = 30,
};

/** @brief Attached-wheel link selected for a remote-tuning response. */
typedef enum {
    REMOTE_TUNING_LINK_NONE,
    REMOTE_TUNING_LINK_LEGACY,
    REMOTE_TUNING_LINK_EXTENDED,
} RemoteTuningLink;

/** @brief Semantic response retained for the attached-wheel remote-tuning protocol. */
typedef enum {
    REMOTE_TUNING_RESPONSE_NONE = 0,
    REMOTE_TUNING_RESPONSE_RECORDS = 1,
    REMOTE_TUNING_RESPONSE_ACTIVE = 2,
    REMOTE_TUNING_RESPONSE_ALTERNATE_RECORDS = 3,
    REMOTE_TUNING_RESPONSE_SETUP = 4,
    REMOTE_TUNING_RESPONSE_REFRESH = 5,
    REMOTE_TUNING_RESPONSE_NEXT_SETUP_PAGE = 0x10,
    REMOTE_TUNING_RESPONSE_INACTIVE = 0xff,
} RemoteTuningResponseCode;

/** @brief One pending remote-tuning response and its semantic data. */
typedef struct {
    RemoteTuningLink link;
    RemoteTuningResponseCode code;
    uint8_t value;
    uint8_t record_data_length;
    uint8_t record_data[REMOTE_TUNING_RECORD_DATA_SIZE];
} RemoteTuningResponse;

#endif
