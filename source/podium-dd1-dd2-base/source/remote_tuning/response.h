#ifndef OPENTEC_BASE_REMOTE_TUNING_RESPONSE_H
#define OPENTEC_BASE_REMOTE_TUNING_RESPONSE_H

#include <stdint.h>

/** @brief Size of one remote-tuning response record payload. */
enum {
    REMOTE_TUNING_RECORD_DATA_SIZE = 30, /**< Bytes available for response record data. */
};

/** @brief Attached-wheel link selected for a remote-tuning response. */
typedef enum {
    REMOTE_TUNING_LINK_NONE,     /**< No response link is selected. */
    REMOTE_TUNING_LINK_LEGACY,   /**< Use the legacy attached-wheel link. */
    REMOTE_TUNING_LINK_EXTENDED, /**< Use the extended attached-wheel link. */
} RemoteTuningLink;

/** @brief Semantic response retained for the attached-wheel remote-tuning protocol. */
typedef enum {
    REMOTE_TUNING_RESPONSE_NONE = 0,               /**< No response is pending. */
    REMOTE_TUNING_RESPONSE_RECORDS = 1,            /**< Return profile records. */
    REMOTE_TUNING_RESPONSE_ACTIVE = 2,             /**< Return active profile state. */
    REMOTE_TUNING_RESPONSE_ALTERNATE_RECORDS = 3,  /**< Return alternate profile records. */
    REMOTE_TUNING_RESPONSE_SETUP = 4,              /**< Return setup state. */
    REMOTE_TUNING_RESPONSE_REFRESH = 5,            /**< Request a response refresh. */
    REMOTE_TUNING_RESPONSE_NEXT_SETUP_PAGE = 0x10, /**< Return the next setup page. */
    REMOTE_TUNING_RESPONSE_INACTIVE = 0xff,        /**< Report inactive remote tuning. */
} RemoteTuningResponseCode;

/** @brief One pending remote-tuning response and its semantic data. */
typedef struct {
    RemoteTuningLink link;                               /**< Link used for the response. */
    RemoteTuningResponseCode code;                       /**< Semantic response code. */
    uint8_t value;                                       /**< Response value byte. */
    uint8_t record_data_length;                          /**< Number of valid record-data bytes. */
    uint8_t record_data[REMOTE_TUNING_RECORD_DATA_SIZE]; /**< Response record data. */
} RemoteTuningResponse;

#endif
