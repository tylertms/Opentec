#include "secure_element/bus.h"

#include <stdbool.h>
#include <stdint.h>

#include "platform/aux_bus.h"
#include "secure_element/a71ch.h"

enum {
    A71CH_DEVICE_ADDRESS = 0x48,
    A71CH_RESPONSE_CONTROL = 0x82,
};

/**
 * @brief Starts one fixed A71CH SCI2C control transaction.
 *
 * Uses 7-bit address 0x48 and sends the protocol control byte before an optional repeated-start
 * read. Wake-up sends only the control byte; the other session commands read their complete fixed
 * response.
 *
 * @param[in] command Fixed session command to start.
 * @param[out] response Response destination for a read command, or null for wake-up.
 * @return True when the auxiliary bus accepted the transaction; otherwise false.
 */
bool a71ch_bus_start(A71chCommand command, uint8_t *response) {
    const A71chControlRequest *request = a71ch_control_request_lookup(command);
    if (request == 0) {
        return false;
    }
    if (request->response_length == 0) {
        return platform_aux_bus_start_write(A71CH_DEVICE_ADDRESS, request->selector, 0, 0);
    }
    if (response == 0) {
        return false;
    }
    return platform_aux_bus_start_read(A71CH_DEVICE_ADDRESS, request->selector, response,
                                       request->response_length);
}

/**
 * @brief Starts one encoded A71CH authentication APDU write.
 *
 * Addresses the secure element at 0x48, sends the SCI2C control byte, and transmits the complete
 * length-prefixed APDU body.
 *
 * @param[in] frame Encoded command selector, body, and body length.
 * @return True when the auxiliary bus accepted the write; otherwise false.
 */
bool a71ch_bus_start_frame_write(const A71chAuthenticationFrame *frame) {
    if (frame == 0 || frame->write_length == 0) {
        return false;
    }
    return platform_aux_bus_start_write(A71CH_DEVICE_ADDRESS, frame->selector, frame->write_data,
                                        frame->write_length);
}

/**
 * @brief Starts one A71CH authentication response read.
 *
 * Sends SCI2C response control byte 0x82 and reads the response length selected by the APDU frame.
 *
 * @param[in] frame Encoded command carrying the expected response length.
 * @param[out] response Destination for the complete response.
 * @return True when the auxiliary bus accepted the read; otherwise false.
 */
bool a71ch_bus_start_frame_read(const A71chAuthenticationFrame *frame, uint8_t *response) {
    if (frame == 0 || response == 0 || frame->response_length == 0) {
        return false;
    }
    return platform_aux_bus_start_read(A71CH_DEVICE_ADDRESS, A71CH_RESPONSE_CONTROL, response,
                                       frame->response_length);
}
