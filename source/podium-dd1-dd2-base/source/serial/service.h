#ifndef OPENTEC_BASE_SERIAL_SERVICE_H
#define OPENTEC_BASE_SERIAL_SERVICE_H

#include <stdbool.h>
#include <stdint.h>

#include "serial/session.h"

typedef enum {
    SERIAL_SERVICE_IDLE,
    SERIAL_SERVICE_PENDING,
    SERIAL_SERVICE_SUCCEEDED,
    SERIAL_SERVICE_FAILED,
} SerialServiceStatus;

typedef struct {
    SerialSession session;
    uint8_t packet[SERIAL_PACKET_SIZE];
    uint32_t deadline_ms;
    uint32_t error_count;
    uint8_t request_type;
    uint8_t attempts;
    SerialServiceStatus status;
    bool packet_pending;
} SerialService;

void serial_service_init(SerialService *service);
bool serial_service_start(SerialService *service, uint8_t type, const uint8_t *message,
                          uint16_t length, uint32_t now_ms);
void serial_service_run(SerialService *service, uint32_t now_ms);
const SerialMessageAssembly *serial_service_response(const SerialService *service);
uint32_t serial_service_error_count(const SerialService *service);

/**
 * @brief Cancels the current attached-device request.
 *
 * Stops a pending physical transfer, clears logical transmit and receive state, and returns the
 * service to idle while preserving the shared packet sequence and cumulative error count.
 *
 * @param[in,out] service Serial service to cancel.
 */
void serial_service_cancel(SerialService *service);
void serial_service_release(SerialService *service);

#endif
