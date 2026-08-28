#ifndef OPENTEC_BASE_WHEEL_TRANSFER_SERVICE_H
#define OPENTEC_BASE_WHEEL_TRANSFER_SERVICE_H

#include <stdbool.h>
#include <stdint.h>

#include "transfer/command.h"

enum {
    WHEEL_TRANSFER_PAYLOAD_SIZE = 10,
};

typedef enum {
    WHEEL_TRANSFER_WRITE,
    WHEEL_TRANSFER_READ,
    WHEEL_TRANSFER_REQUEST_COUNT,
} WheelTransferRequest;

typedef enum {
    WHEEL_TRANSFER_INVALID_RESPONSE = -3,
    WHEEL_TRANSFER_READ_FAILED = -2,
    WHEEL_TRANSFER_WRITE_FAILED = -1,
    WHEEL_TRANSFER_IDLE = 0,
    WHEEL_TRANSFER_PENDING = 1,
    WHEEL_TRANSFER_COMPLETE = 2,
} WheelTransferStatus;

typedef enum {
    WHEEL_TRANSFER_PHASE_IDLE,
    WHEEL_TRANSFER_PHASE_WRITE_READY,
    WHEEL_TRANSFER_PHASE_WRITE_PENDING,
    WHEEL_TRANSFER_PHASE_READ_PENDING,
} WheelTransferPhase;

typedef struct {
    uint8_t response[WHEEL_TRANSFER_PAYLOAD_SIZE];
    WheelTransferStatus statuses[WHEEL_TRANSFER_REQUEST_COUNT];
    WheelTransferRequest request;
    WheelTransferPhase phase;
} WheelTransferService;

void wheel_transfer_service_init(WheelTransferService *service);
bool wheel_transfer_service_start(WheelTransferService *service, WheelTransferRequest request);
void wheel_transfer_service_run(WheelTransferService *service, CommandTransport *transport);
WheelTransferStatus wheel_transfer_service_status(const WheelTransferService *service,
                                                  WheelTransferRequest request);

#endif
