#include "wheel/packet_remote_tuning.h"

#include <stddef.h>
#include <string.h>

enum {
    REMOTE_TUNING_REPORT_ID = 0xa7,
};

/**
 * @brief Tests whether a response has a supported attached-wheel encoding.
 *
 * Accepts standard records, active, alternate records, inactive, setup-page, refresh, and the
 * internal next-setup-page response.
 *
 * @param[in] code Semantic remote-tuning response code.
 * @return True when the response can be encoded.
 */
static bool response_supported(RemoteTuningResponseCode code) {
    return code == REMOTE_TUNING_RESPONSE_RECORDS || code == REMOTE_TUNING_RESPONSE_ACTIVE ||
           code == REMOTE_TUNING_RESPONSE_ALTERNATE_RECORDS ||
           code == REMOTE_TUNING_RESPONSE_INACTIVE || code == REMOTE_TUNING_RESPONSE_SETUP ||
           code == REMOTE_TUNING_RESPONSE_REFRESH || code == REMOTE_TUNING_RESPONSE_NEXT_SETUP_PAGE;
}

/**
 * @brief Tests whether a response contains valid data for its encoding.
 *
 * Record responses require one through 30 serialized bytes. Single-value responses require no
 * additional data constraints.
 *
 * @param[in] response Semantic remote-tuning response.
 * @return True when the response can be encoded safely.
 */
static bool response_valid(const RemoteTuningResponse *response) {
    if (response == NULL || response->link == REMOTE_TUNING_LINK_NONE ||
        !response_supported(response->code)) {
        return false;
    }
    if (response->code == REMOTE_TUNING_RESPONSE_RECORDS ||
        response->code == REMOTE_TUNING_RESPONSE_ALTERNATE_RECORDS) {
        return response->record_data_length != 0 &&
               response->record_data_length <= REMOTE_TUNING_RECORD_DATA_SIZE;
    }
    return true;
}

/**
 * @brief Initializes attached-wheel remote-tuning output.
 *
 * Clears the pending link, response code, and response value.
 *
 * @param[out] output Remote-tuning output to initialize.
 */
void wheel_packet_remote_tuning_init(WheelPacketRemoteTuningOutput *output) {
    output->response = (RemoteTuningResponse){0};
}

/**
 * @brief Queues an attached-wheel remote-tuning response.
 *
 * Replaces the pending response when its link, response code, and response data are supported by
 * the shared packet format.
 *
 * @param[in,out] output Attached-wheel remote-tuning output.
 * @param[in] response Response link, code, and value to retain.
 * @return True when the response was accepted.
 */
bool wheel_packet_remote_tuning_queue(WheelPacketRemoteTuningOutput *output,
                                      const RemoteTuningResponse *response) {
    if (output == NULL || !response_valid(response)) {
        return false;
    }
    output->response = *response;
    return true;
}

/**
 * @brief Reports whether an attached-wheel remote-tuning response is pending.
 *
 * Tests the retained response code without consuming it.
 *
 * @param[in] output Attached-wheel remote-tuning output.
 * @return True when a supported response is pending.
 */
bool wheel_packet_remote_tuning_pending(const WheelPacketRemoteTuningOutput *output) {
    return output != NULL && response_valid(&output->response);
}

/**
 * @brief Encodes one attached-wheel remote-tuning response.
 *
 * Writes report ID 0xA7. Standard and alternate records use response fields 1 and 3 and copy their
 * 30-byte record area. Active and inactive responses use field 2 with values one and zero. Setup
 * and refresh responses retain their response field and supplied value. The internal next-page
 * response is normalized to setup response field four with its supplied page value. The pending
 * response is consumed after encoding; the caller supplies cleared payload storage and writes the
 * checksum.
 *
 * @param[in,out] output Pending response consumed by a successful encoding.
 * @param[out] packet Thirty-three-byte destination for the response fields.
 * @return True when a response was encoded.
 */
bool wheel_packet_remote_tuning_encode(WheelPacketRemoteTuningOutput *output,
                                       uint8_t packet[WHEEL_PACKET_REMOTE_TUNING_SIZE]) {
    if (!wheel_packet_remote_tuning_pending(output) || packet == NULL) {
        return false;
    }

    RemoteTuningResponse response = output->response;
    packet[0] = REMOTE_TUNING_REPORT_ID;
    if (response.code == REMOTE_TUNING_RESPONSE_RECORDS ||
        response.code == REMOTE_TUNING_RESPONSE_ALTERNATE_RECORDS) {
        packet[1] = (uint8_t)response.code;
        memcpy(packet + 2, response.record_data, REMOTE_TUNING_RECORD_DATA_SIZE);
    } else if (response.code == REMOTE_TUNING_RESPONSE_ACTIVE ||
               response.code == REMOTE_TUNING_RESPONSE_INACTIVE) {
        packet[1] = REMOTE_TUNING_RESPONSE_ACTIVE;
        packet[2] = response.code == REMOTE_TUNING_RESPONSE_ACTIVE ? 1 : 0;
    } else {
        packet[1] = response.code == REMOTE_TUNING_RESPONSE_NEXT_SETUP_PAGE
                        ? REMOTE_TUNING_RESPONSE_SETUP
                        : (uint8_t)response.code;
        packet[2] = response.value;
    }
    output->response = (RemoteTuningResponse){0};
    return true;
}
