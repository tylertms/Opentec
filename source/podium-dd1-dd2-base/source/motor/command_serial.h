#ifndef OPENTEC_BASE_MOTOR_COMMAND_SERIAL_H
#define OPENTEC_BASE_MOTOR_COMMAND_SERIAL_H

#include <stdbool.h>
#include <stdint.h>

#include "serial/service.h"
#include "transfer/command.h"

/**
 * @brief Starts a queued command through the serial service.
 *
 * Submits the transport's queued read or write request as logical message type four and advances
 * the transport to its response-pending phase. Null pointers, an unqueued request, or a busy
 * serial service cause the function to return false.
 *
 * @param[in,out] transport Command transport containing a queued request.
 * @param[in,out] service Idle serial service to start.
 * @param[in] now_ms Current monotonic time in milliseconds.
 * @return True when the serial exchange starts and the transport enters a response-pending phase.
 */
bool motor_command_serial_submit(CommandTransport *transport, SerialService *service,
                                 uint32_t now_ms);

/**
 * @brief Completes a command transport from a serial response.
 *
 * Consumes a matching type-four response, applies it to the transport, and releases the serial
 * service. A failed serial exchange is propagated to the transport before release. A type mismatch
 * or response that is not ready leaves the serial service untouched and returns false.
 *
 * @param[in,out] transport Command transport awaiting a response.
 * @param[in,out] service Serial service holding a completed type-four response or failure.
 * @return True when a matching response or failure was consumed; false when no response is ready.
 */
bool motor_command_serial_receive(CommandTransport *transport, SerialService *service);

#endif
