#ifndef OPENTEC_BASE_A71CH_BUS_H
#define OPENTEC_BASE_A71CH_BUS_H

#include <stdbool.h>
#include <stdint.h>

#include "secure_element/a71ch.h"

/**
 * @brief Starts a fixed A71CH control transaction.
 *
 * Sends the control selector and, for commands with a response, starts a read into the supplied
 * response buffer.
 *
 * @param[in] command Fixed session control command.
 * @param[out] response Response destination for commands that return bytes; null for wake-up.
 * @return True when the auxiliary bus accepts the transaction; otherwise false.
 */
bool a71ch_bus_start(A71chCommand command, uint8_t *response);

/**
 * @brief Starts an A71CH authentication frame write.
 *
 * Sends the frame selector and encoded APDU body through the auxiliary bus.
 *
 * @param[in] frame Encoded authentication frame to transmit.
 * @return True when the auxiliary bus accepts the write; otherwise false.
 */
bool a71ch_bus_start_frame_write(const A71chAuthenticationFrame *frame);

/**
 * @brief Starts an A71CH authentication frame read.
 *
 * Reads the response length recorded in frame into the supplied response buffer.
 *
 * @param[in] frame Encoded frame containing the expected response length.
 * @param[out] response Destination for the complete response.
 * @return True when the auxiliary bus accepts the read; otherwise false.
 */
bool a71ch_bus_start_frame_read(const A71chAuthenticationFrame *frame, uint8_t *response);

#endif
