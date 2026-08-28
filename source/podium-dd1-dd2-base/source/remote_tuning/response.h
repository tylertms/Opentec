#ifndef OPENTEC_BASE_REMOTE_TUNING_RESPONSE_H
#define OPENTEC_BASE_REMOTE_TUNING_RESPONSE_H

#include <stdint.h>

/** @brief Attached-wheel link selected for a remote-tuning response. */
typedef enum {
    REMOTE_TUNING_LINK_NONE,
    REMOTE_TUNING_LINK_LEGACY,
    REMOTE_TUNING_LINK_EXTENDED,
} RemoteTuningLink;

/** @brief Semantic response retained for the attached-wheel remote-tuning protocol. */
typedef enum {
    REMOTE_TUNING_RESPONSE_NONE = 0,
    REMOTE_TUNING_RESPONSE_ACTIVE = 2,
    REMOTE_TUNING_RESPONSE_SETUP = 4,
    REMOTE_TUNING_RESPONSE_REFRESH = 5,
    REMOTE_TUNING_RESPONSE_INACTIVE = 0xff,
} RemoteTuningResponseCode;

/** @brief One pending remote-tuning response and its single-byte value. */
typedef struct {
    RemoteTuningLink link;
    RemoteTuningResponseCode code;
    uint8_t value;
} RemoteTuningResponse;

#endif
