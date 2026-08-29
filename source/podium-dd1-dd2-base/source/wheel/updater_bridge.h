#ifndef OPENTEC_BASE_WHEEL_UPDATER_BRIDGE_H
#define OPENTEC_BASE_WHEEL_UPDATER_BRIDGE_H

#include <stdbool.h>
#include <stdint.h>

enum {
    WHEEL_UPDATER_BRIDGE_MAX_REQUEST_SIZE = 63,
    WHEEL_UPDATER_BRIDGE_MAX_RESPONSE_SIZE = 66,
};

/** @brief Completion state of the current wheel transport operation. */
typedef enum {
    WHEEL_UPDATER_IO_IDLE,
    WHEEL_UPDATER_IO_PENDING,
    WHEEL_UPDATER_IO_COMPLETE,
    WHEEL_UPDATER_IO_FAILED,
} WheelUpdaterIoStatus;

/** @brief Result supplied by the wheel transport for one protocol step. */
typedef struct {
    const uint8_t *data;
    uint32_t now_ms;
    uint8_t length;
    WheelUpdaterIoStatus status;
} WheelUpdaterIo;

/** @brief Next wheel transport operation requested by the updater protocol. */
typedef enum {
    WHEEL_UPDATER_OPERATION_NONE,
    WHEEL_UPDATER_OPERATION_WRITE,
    WHEEL_UPDATER_OPERATION_READ,
} WheelUpdaterOperationKind;

/** @brief Wheel transport operation and its associated byte count. */
typedef struct {
    const uint8_t *data;
    uint8_t length;
    WheelUpdaterOperationKind kind;
} WheelUpdaterOperation;

/** @brief Logical phase of an updater request and response exchange. */
typedef enum {
    WHEEL_UPDATER_BRIDGE_IDLE,
    WHEEL_UPDATER_BRIDGE_WRITE_REQUEST,
    WHEEL_UPDATER_BRIDGE_READ_DELAY,
    WHEEL_UPDATER_BRIDGE_READ_HEADER,
    WHEEL_UPDATER_BRIDGE_READ_OPCODE,
    WHEEL_UPDATER_BRIDGE_READ_FIXED_PAYLOAD,
    WHEEL_UPDATER_BRIDGE_READ_LENGTH,
    WHEEL_UPDATER_BRIDGE_READ_METADATA,
    WHEEL_UPDATER_BRIDGE_READ_VARIABLE_PAYLOAD,
    WHEEL_UPDATER_BRIDGE_RESPONSE_READY,
} WheelUpdaterBridgePhase;

/** @brief Transport-independent updater request and response state. */
typedef struct {
    uint8_t request[WHEEL_UPDATER_BRIDGE_MAX_REQUEST_SIZE];
    uint8_t response[WHEEL_UPDATER_BRIDGE_MAX_RESPONSE_SIZE];
    uint32_t deadline_ms;
    uint16_t variable_payload_length;
    uint8_t request_length;
    uint8_t response_length;
    WheelUpdaterBridgePhase phase;
    bool retry_response;
} WheelUpdaterBridge;

void wheel_updater_bridge_init(WheelUpdaterBridge *bridge);
bool wheel_updater_bridge_start(WheelUpdaterBridge *bridge, const uint8_t *request, uint8_t length);
WheelUpdaterOperation wheel_updater_bridge_step(WheelUpdaterBridge *bridge,
                                                const WheelUpdaterIo *io);
bool wheel_updater_bridge_take_response(WheelUpdaterBridge *bridge, const uint8_t **response,
                                        uint8_t *length);
bool wheel_updater_bridge_active(const WheelUpdaterBridge *bridge);

#endif
