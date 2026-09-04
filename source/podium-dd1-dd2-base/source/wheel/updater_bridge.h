#ifndef OPENTEC_BASE_WHEEL_UPDATER_BRIDGE_H
#define OPENTEC_BASE_WHEEL_UPDATER_BRIDGE_H

#include <stdbool.h>
#include <stdint.h>

/** @brief Capacity limits for retained updater frames. */
enum {
    WHEEL_UPDATER_BRIDGE_MAX_REQUEST_SIZE = 63,  /**< Maximum retained updater request length. */
    WHEEL_UPDATER_BRIDGE_MAX_RESPONSE_SIZE = 64, /**< Maximum retained updater response length. */
};

/** @brief Completion state of the current updater transport operation. */
typedef enum {
    WHEEL_UPDATER_IO_IDLE,     /**< No transport operation is pending. */
    WHEEL_UPDATER_IO_PENDING,  /**< The current transport operation is still in progress. */
    WHEEL_UPDATER_IO_COMPLETE, /**< The current transport operation completed successfully. */
    WHEEL_UPDATER_IO_FAILED,   /**< The current transport operation failed. */
} WheelUpdaterIoStatus;

/** @brief Result supplied by a wheel transport for one updater protocol step. */
typedef struct {
    const uint8_t *data;         /**< Completed read bytes, or null when no bytes are available. */
    uint32_t now_ms;             /**< Current monotonic time in milliseconds. */
    uint8_t length;              /**< Number of completed read bytes in data. */
    WheelUpdaterIoStatus status; /**< Completion state of the preceding transport operation. */
} WheelUpdaterIo;

/** @brief Next transport operation requested by the updater protocol. */
typedef enum {
    WHEEL_UPDATER_OPERATION_NONE,  /**< No transport operation is requested. */
    WHEEL_UPDATER_OPERATION_WRITE, /**< Write the operation data to the updater endpoint. */
    WHEEL_UPDATER_OPERATION_READ,  /**< Read the requested number of bytes from the endpoint. */
} WheelUpdaterOperationKind;

/** @brief Transport operation and associated updater byte range. */
typedef struct {
    const uint8_t *data;            /**< Bytes to write, or null for a read operation. */
    uint8_t length;                 /**< Number of bytes to write or read. */
    WheelUpdaterOperationKind kind; /**< Requested transport operation kind. */
} WheelUpdaterOperation;

/** @brief Logical phase of an updater request and response exchange. */
typedef enum {
    WHEEL_UPDATER_BRIDGE_IDLE,          /**< No updater exchange is active. */
    WHEEL_UPDATER_BRIDGE_WRITE_REQUEST, /**< The retained request is ready for transport. */
    WHEEL_UPDATER_BRIDGE_READ_DELAY, /**< The protocol is waiting before a normal response read. */
    WHEEL_UPDATER_BRIDGE_READ_HEADER, /**< The protocol is reading a response frame marker. */
    WHEEL_UPDATER_BRIDGE_READ_OPCODE, /**< The protocol is reading a response opcode. */
    WHEEL_UPDATER_BRIDGE_READ_FIXED_PAYLOAD, /**< The protocol is reading a fixed response payload.
                                              */
    WHEEL_UPDATER_BRIDGE_READ_LENGTH,   /**< The protocol is reading a variable payload length. */
    WHEEL_UPDATER_BRIDGE_READ_METADATA, /**< The protocol is reading variable payload metadata. */
    WHEEL_UPDATER_BRIDGE_READ_VARIABLE_PAYLOAD, /**< The protocol is reading variable payload bytes.
                                                 */
    WHEEL_UPDATER_BRIDGE_RESPONSE_READY, /**< A complete response is retained for the caller. */
} WheelUpdaterBridgePhase;

/** @brief Transport-independent updater request and response state. */
typedef struct {
    uint8_t request[WHEEL_UPDATER_BRIDGE_MAX_REQUEST_SIZE];   /**< Retained request bytes. */
    uint8_t response[WHEEL_UPDATER_BRIDGE_MAX_RESPONSE_SIZE]; /**< Assembled response bytes. */
    uint16_t service_ticks; /**< Retained bridge ticks reset only by their owning protocol phase. */
    uint16_t variable_payload_length; /**< Expected variable response payload length. */
    uint8_t request_length;           /**< Number of valid bytes in request. */
    uint8_t response_length;          /**< Number of valid bytes in response. */
    WheelUpdaterBridgePhase phase;    /**< Current updater exchange phase. */
    bool retry_response; /**< True when the current response uses retry-response handling. */
    bool response_probe; /**< True when this exchange is the terminal route-discovery probe. */
} WheelUpdaterBridge;

/**
 * @brief Initializes an updater protocol bridge.
 *
 * Clears retained request and response bytes, timing, lengths, phase, and retry state.
 *
 * @param[out] bridge Updater bridge state to initialize; null is ignored.
 */
void wheel_updater_bridge_init(WheelUpdaterBridge *bridge);

/**
 * @brief Starts an updater request exchange.
 *
 * Accepts an idle bridge and a 0x5A-prefixed request from two through 63 bytes, retaining the
 * request independently of the caller's buffer.
 *
 * @param[in,out] bridge Idle updater bridge to start; null is rejected.
 * @param[in] request Marker-prefixed updater request bytes.
 * @param[in] length Request length in bytes.
 * @return True when the request is valid and accepted; otherwise false.
 */
bool wheel_updater_bridge_start(WheelUpdaterBridge *bridge, const uint8_t *request, uint8_t length);

/**
 * @brief Starts a route-discovery probe exchange.
 *
 * Accepts the same marker-prefixed request as #wheel_updater_bridge_start while retaining the
 * probe-only terminal failure rules for zero markers, read failures, and non-0xA7 opcodes.
 *
 * @param[in,out] bridge Idle updater bridge to start; null is rejected.
 * @param[in] request Marker-prefixed route-probe request bytes.
 * @param[in] length Probe request length in bytes.
 * @return True when the probe was accepted; otherwise false.
 */
bool wheel_updater_bridge_start_probe(WheelUpdaterBridge *bridge, const uint8_t *request,
                                      uint8_t length);

/**
 * @brief Advances one updater request and response exchange.
 *
 * Consumes the preceding transport result and returns the next write or read operation required by
 * the updater protocol.
 *
 * @param[in,out] bridge Updater bridge exchange to advance.
 * @param[in] io Current transport result and monotonic time.
 * @return Next transport operation, or WHEEL_UPDATER_OPERATION_NONE while waiting or when no
 * exchange is active.
 */
WheelUpdaterOperation wheel_updater_bridge_step(WheelUpdaterBridge *bridge, WheelUpdaterIo io);

/**
 * @brief Takes a complete updater response.
 *
 * Returns pointers to the retained response and length, then returns the bridge to its idle phase.
 *
 * @param[in,out] bridge Updater bridge holding a complete response.
 * @param[out] response Receives a pointer to retained response bytes.
 * @param[out] length Receives the response length.
 * @return True when a complete response was available and returned; otherwise false.
 */
bool wheel_updater_bridge_take_response(WheelUpdaterBridge *bridge, const uint8_t **response,
                                        uint8_t *length);

/**
 * @brief Reports whether an updater exchange is active.
 *
 * Includes request writes, response reads, timing delays, and an untaken complete response.
 *
 * @param[in] bridge Updater bridge to inspect.
 * @return True while bridge is non-null and not idle; otherwise false.
 */
bool wheel_updater_bridge_active(const WheelUpdaterBridge *bridge);

#endif
