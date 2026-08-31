#include "wheel/updater_bridge.h"

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "platform/time.h"

enum {
    WHEEL_UPDATER_FRAME_MARKER = 0x5a,
    WHEEL_UPDATER_RETRY_OPCODE = 0xa1,
    WHEEL_UPDATER_ACKNOWLEDGEMENT_OPCODE = 0xa2,
    WHEEL_UPDATER_VARIABLE_OPCODE = 0xa4,
    WHEEL_UPDATER_FIXED_OPCODE = 0xa7,
    WHEEL_UPDATER_FIXED_PAYLOAD_SIZE = 8,
    WHEEL_UPDATER_LENGTH_SIZE = 2,
    WHEEL_UPDATER_METADATA_SIZE = 2,
    WHEEL_UPDATER_READ_DELAY_MS = 2,
    WHEEL_UPDATER_RETRY_TIMEOUT_MS = 2000,
};

/**
 * @brief Builds the next write or read operation for the current phase.
 *
 * Exposes the retained request for writes and the exact response fragment size for reads.
 *
 * @param[in] bridge Active updater bridge.
 * @return Transport operation for the current phase, or no operation while waiting.
 */
static WheelUpdaterOperation current_operation(const WheelUpdaterBridge *bridge) {
    WheelUpdaterOperation operation = {0};
    if (bridge->phase == WHEEL_UPDATER_BRIDGE_WRITE_REQUEST) {
        operation.kind = WHEEL_UPDATER_OPERATION_WRITE;
        operation.data = bridge->request;
        operation.length = bridge->request_length;
    } else if (bridge->phase == WHEEL_UPDATER_BRIDGE_READ_HEADER ||
               bridge->phase == WHEEL_UPDATER_BRIDGE_READ_OPCODE) {
        operation.kind = WHEEL_UPDATER_OPERATION_READ;
        operation.length = 1;
    } else if (bridge->phase == WHEEL_UPDATER_BRIDGE_READ_FIXED_PAYLOAD) {
        operation.kind = WHEEL_UPDATER_OPERATION_READ;
        operation.length = WHEEL_UPDATER_FIXED_PAYLOAD_SIZE;
    } else if (bridge->phase == WHEEL_UPDATER_BRIDGE_READ_LENGTH) {
        operation.kind = WHEEL_UPDATER_OPERATION_READ;
        operation.length = WHEEL_UPDATER_LENGTH_SIZE;
    } else if (bridge->phase == WHEEL_UPDATER_BRIDGE_READ_METADATA) {
        operation.kind = WHEEL_UPDATER_OPERATION_READ;
        operation.length = WHEEL_UPDATER_METADATA_SIZE;
    } else if (bridge->phase == WHEEL_UPDATER_BRIDGE_READ_VARIABLE_PAYLOAD) {
        operation.kind = WHEEL_UPDATER_OPERATION_READ;
        operation.length = (uint8_t)bridge->variable_payload_length;
    }
    return operation;
}

/**
 * @brief Publishes the assembled updater response.
 *
 * Retains the response bytes until the USB owner takes them.
 *
 * @param[in,out] bridge Updater bridge completing its response.
 * @return No wheel transport operation.
 */
static WheelUpdaterOperation finish_response(WheelUpdaterBridge *bridge) {
    bridge->phase = WHEEL_UPDATER_BRIDGE_RESPONSE_READY;
    return (WheelUpdaterOperation){0};
}

/**
 * @brief Stores one completed response fragment.
 *
 * Copies only an exact-length fragment that fits in the remaining response capacity.
 *
 * @param[in,out] bridge Updater bridge receiving the fragment.
 * @param[in] io Completed wheel read result.
 * @param[in] length Required fragment size.
 * @return True when the complete fragment was stored; otherwise false.
 */
static bool append_fragment(WheelUpdaterBridge *bridge, WheelUpdaterIo io, uint8_t length) {
    if (io.data == NULL || io.length != length ||
        (uint16_t)bridge->response_length + length > sizeof(bridge->response)) {
        return false;
    }
    memcpy(bridge->response + bridge->response_length, io.data, length);
    bridge->response_length += length;
    return true;
}

/**
 * @brief Initializes an updater bridge exchange.
 *
 * Clears retained request, response, timing, and parser state.
 *
 * @param[out] bridge Updater bridge state to initialize.
 */
void wheel_updater_bridge_init(WheelUpdaterBridge *bridge) {
    if (bridge != NULL) {
        *bridge = (WheelUpdaterBridge){0};
    }
}

/**
 * @brief Starts forwarding one updater request to the attached wheel.
 *
 * Accepts marker-prefixed requests from two through 63 bytes, retains them independently of the
 * USB receive buffer, and schedules the complete request as one wheel write.
 *
 * @param[in,out] bridge Idle updater bridge accepting the request.
 * @param[in] request Updater request beginning with marker 0x5A.
 * @param[in] length Request length from two through 63 bytes.
 * @return True when the request was accepted; otherwise false.
 */
bool wheel_updater_bridge_start(WheelUpdaterBridge *bridge, const uint8_t *request,
                                uint8_t length) {
    if (bridge == NULL || request == NULL || bridge->phase != WHEEL_UPDATER_BRIDGE_IDLE ||
        length < 2 || length > sizeof(bridge->request) ||
        request[0] != WHEEL_UPDATER_FRAME_MARKER) {
        return false;
    }

    memcpy(bridge->request, request, length);
    bridge->request_length = length;
    bridge->response_length = 0;
    bridge->variable_payload_length = 0;
    bridge->retry_response = false;
    bridge->phase = WHEEL_UPDATER_BRIDGE_WRITE_REQUEST;
    return true;
}

/**
 * @brief Advances one updater request and response exchange.
 *
 * Reissues the current operation after a failed wheel transfer, waits two milliseconds after the
 * request write, recognizes response opcodes 0xA1, 0xA2, 0xA4, and 0xA7, assembles their fixed or
 * variable fragments, and ends an 0xA1 retry response on a zero marker or after 2000 milliseconds.
 *
 * @param[in,out] bridge Active updater bridge to advance.
 * @param[in] io Current time and completion state of the preceding wheel operation.
 * @return Next wheel write or read operation, or no operation while waiting or response-ready.
 */
WheelUpdaterOperation wheel_updater_bridge_step(WheelUpdaterBridge *bridge, WheelUpdaterIo io) {
    if (bridge == NULL || bridge->phase == WHEEL_UPDATER_BRIDGE_IDLE ||
        bridge->phase == WHEEL_UPDATER_BRIDGE_RESPONSE_READY) {
        return (WheelUpdaterOperation){0};
    }
    if (io.status == WHEEL_UPDATER_IO_FAILED) {
        return current_operation(bridge);
    }

    if (bridge->phase == WHEEL_UPDATER_BRIDGE_WRITE_REQUEST) {
        if (io.status != WHEEL_UPDATER_IO_COMPLETE) {
            return io.status == WHEEL_UPDATER_IO_PENDING ? (WheelUpdaterOperation){0}
                                                         : current_operation(bridge);
        }
        if (bridge->request[1] == WHEEL_UPDATER_RETRY_OPCODE) {
            bridge->phase = WHEEL_UPDATER_BRIDGE_IDLE;
            return (WheelUpdaterOperation){0};
        }
        bridge->deadline_ms = io.now_ms + WHEEL_UPDATER_READ_DELAY_MS;
        bridge->phase = WHEEL_UPDATER_BRIDGE_READ_DELAY;
        return (WheelUpdaterOperation){0};
    }

    if (bridge->phase == WHEEL_UPDATER_BRIDGE_READ_DELAY) {
        if (!platform_time_reached(io.now_ms, bridge->deadline_ms)) {
            return (WheelUpdaterOperation){0};
        }
        bridge->phase = WHEEL_UPDATER_BRIDGE_READ_HEADER;
        return current_operation(bridge);
    }

    if (bridge->phase == WHEEL_UPDATER_BRIDGE_READ_HEADER && bridge->retry_response &&
        io.status != WHEEL_UPDATER_IO_COMPLETE &&
        platform_time_reached(io.now_ms, bridge->deadline_ms)) {
        return finish_response(bridge);
    }
    if (io.status != WHEEL_UPDATER_IO_COMPLETE) {
        return io.status == WHEEL_UPDATER_IO_PENDING ? (WheelUpdaterOperation){0}
                                                     : current_operation(bridge);
    }

    if (bridge->phase == WHEEL_UPDATER_BRIDGE_READ_HEADER) {
        if (io.data == NULL || io.length != 1) {
            return current_operation(bridge);
        }
        if (io.data[0] == 0) {
            return bridge->retry_response ? finish_response(bridge) : current_operation(bridge);
        }
        if (io.data[0] != WHEEL_UPDATER_FRAME_MARKER) {
            bridge->response_length = 0;
            bridge->retry_response = false;
            bridge->phase = WHEEL_UPDATER_BRIDGE_WRITE_REQUEST;
            return current_operation(bridge);
        }
        if (bridge->response_length > sizeof(bridge->response) - 2) {
            return finish_response(bridge);
        }
        bridge->response[bridge->response_length] = WHEEL_UPDATER_FRAME_MARKER;
        bridge->phase = WHEEL_UPDATER_BRIDGE_READ_OPCODE;
        return current_operation(bridge);
    }

    if (bridge->phase == WHEEL_UPDATER_BRIDGE_READ_OPCODE) {
        if (io.data == NULL || io.length != 1) {
            return current_operation(bridge);
        }
        uint8_t opcode = io.data[0];
        bridge->response[bridge->response_length + 1] = opcode;
        if (opcode == WHEEL_UPDATER_RETRY_OPCODE) {
            bridge->response_length += 2;
            bridge->retry_response = true;
            bridge->deadline_ms = io.now_ms + WHEEL_UPDATER_RETRY_TIMEOUT_MS;
            bridge->phase = WHEEL_UPDATER_BRIDGE_READ_HEADER;
            return current_operation(bridge);
        }
        if (opcode == WHEEL_UPDATER_ACKNOWLEDGEMENT_OPCODE) {
            bridge->response_length += 2;
            return finish_response(bridge);
        }
        if (opcode == WHEEL_UPDATER_FIXED_OPCODE) {
            bridge->response_length += 2;
            bridge->phase = WHEEL_UPDATER_BRIDGE_READ_FIXED_PAYLOAD;
            return current_operation(bridge);
        }
        if (opcode == WHEEL_UPDATER_VARIABLE_OPCODE) {
            bridge->response_length += 2;
            bridge->phase = WHEEL_UPDATER_BRIDGE_READ_LENGTH;
            return current_operation(bridge);
        }
        bridge->phase = WHEEL_UPDATER_BRIDGE_IDLE;
        return (WheelUpdaterOperation){0};
    }

    if (bridge->phase == WHEEL_UPDATER_BRIDGE_READ_FIXED_PAYLOAD) {
        if (!append_fragment(bridge, io, WHEEL_UPDATER_FIXED_PAYLOAD_SIZE)) {
            return current_operation(bridge);
        }
        return finish_response(bridge);
    }

    if (bridge->phase == WHEEL_UPDATER_BRIDGE_READ_LENGTH) {
        if (!append_fragment(bridge, io, WHEEL_UPDATER_LENGTH_SIZE)) {
            return current_operation(bridge);
        }
        bridge->variable_payload_length = (uint16_t)io.data[0] | (uint16_t)io.data[1] << 8;
        bridge->phase = WHEEL_UPDATER_BRIDGE_READ_METADATA;
        return current_operation(bridge);
    }

    if (bridge->phase == WHEEL_UPDATER_BRIDGE_READ_METADATA) {
        if (!append_fragment(bridge, io, WHEEL_UPDATER_METADATA_SIZE)) {
            return current_operation(bridge);
        }
        uint16_t capacity = sizeof(bridge->response) - bridge->response_length;
        if (bridge->variable_payload_length > capacity) {
            bridge->variable_payload_length = capacity;
        }
        if (bridge->variable_payload_length == 0) {
            return finish_response(bridge);
        }
        bridge->phase = WHEEL_UPDATER_BRIDGE_READ_VARIABLE_PAYLOAD;
        return current_operation(bridge);
    }

    if (bridge->phase != WHEEL_UPDATER_BRIDGE_READ_VARIABLE_PAYLOAD) {
        return (WheelUpdaterOperation){0};
    }
    if (!append_fragment(bridge, io, (uint8_t)bridge->variable_payload_length)) {
        return current_operation(bridge);
    }
    return finish_response(bridge);
}

/**
 * @brief Takes one complete updater response.
 *
 * Exposes the retained response and returns the bridge to idle for the next USB request.
 *
 * @param[in,out] bridge Updater bridge holding a complete response.
 * @param[out] response Retained response bytes.
 * @param[out] length Complete response length.
 * @return True when a response was available; otherwise false.
 */
bool wheel_updater_bridge_take_response(WheelUpdaterBridge *bridge, const uint8_t **response,
                                        uint8_t *length) {
    if (bridge == NULL || response == NULL || length == NULL ||
        bridge->phase != WHEEL_UPDATER_BRIDGE_RESPONSE_READY) {
        return false;
    }
    *response = bridge->response;
    *length = bridge->response_length;
    bridge->phase = WHEEL_UPDATER_BRIDGE_IDLE;
    return true;
}

/**
 * @brief Reports whether an updater exchange owns the bridge.
 *
 * Includes request writes, response reads, timing delays, and an untaken complete response.
 *
 * @param[in] bridge Updater bridge to inspect.
 * @return True while an exchange is active; otherwise false.
 */
bool wheel_updater_bridge_active(const WheelUpdaterBridge *bridge) {
    return bridge != NULL && bridge->phase != WHEEL_UPDATER_BRIDGE_IDLE;
}
