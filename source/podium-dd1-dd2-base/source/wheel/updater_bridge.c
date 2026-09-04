#include "wheel/updater_bridge.h"

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

/** @brief Updater frame, opcode, size, and service-tick constants. */
enum {
    WHEEL_UPDATER_FRAME_MARKER = 0x5a,           /**< Updater frame marker byte. */
    WHEEL_UPDATER_RETRY_OPCODE = 0xa1,           /**< Retry-response opcode. */
    WHEEL_UPDATER_ACKNOWLEDGEMENT_OPCODE = 0xa2, /**< Acknowledgement-response opcode. */
    WHEEL_UPDATER_VARIABLE_OPCODE = 0xa4,        /**< Variable-payload response opcode. */
    WHEEL_UPDATER_FIXED_OPCODE = 0xa7,           /**< Fixed-payload response opcode. */
    WHEEL_UPDATER_FIXED_PAYLOAD_SIZE = 8,        /**< Fixed response payload length. */
    WHEEL_UPDATER_LENGTH_SIZE = 2,               /**< Variable-payload length field size. */
    WHEEL_UPDATER_METADATA_SIZE = 2,             /**< Variable-payload metadata size. */
    WHEEL_UPDATER_READ_DELAY_TICKS = 2,        /**< Service ticks before the first response read. */
    WHEEL_UPDATER_RETRY_TIMEOUT_TICKS = 0x7d0, /**< Retry-response service-tick limit. */
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
 * Retains the response bytes until the owning transport service takes them.
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
 * Copies only an exact-length fragment that fits in the remaining response capacity. A zero-length
 * fragment is valid with no data pointer because it still completes the remote read transaction.
 *
 * @param[in,out] bridge Updater bridge receiving the fragment.
 * @param[in] io Completed wheel read result.
 * @param[in] length Required fragment size.
 * @return True when the complete fragment was stored; otherwise false.
 */
static bool append_fragment(WheelUpdaterBridge *bridge, WheelUpdaterIo io, uint8_t length) {
    if (io.length != length ||
        (uint16_t)bridge->response_length + length > sizeof(bridge->response)) {
        return false;
    }
    if (length != 0) {
        if (io.data == NULL) {
            return false;
        }
        memcpy(bridge->response + bridge->response_length, io.data, length);
    }
    bridge->response_length += length;
    return true;
}

/**
 * @brief Initializes an updater bridge exchange.
 *
 * Clears retained request, response, timing, and parser state. This is the only operation that
 * resets the service counter outside its protocol phase transitions.
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
 * caller's buffer, and schedules the complete request as one wheel write.
 *
 * @param[in,out] bridge Idle updater bridge accepting the request.
 * @param[in] request Updater request beginning with marker 0x5A.
 * @param[in] length Request length from two through 63 bytes.
 * @param[in] response_probe True when the exchange is the route-discovery probe.
 * @return True when the request was accepted; otherwise false.
 */
static bool start_exchange(WheelUpdaterBridge *bridge, const uint8_t *request, uint8_t length,
                           bool response_probe) {
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
    bridge->response_probe = response_probe;
    bridge->phase = WHEEL_UPDATER_BRIDGE_WRITE_REQUEST;
    return true;
}

bool wheel_updater_bridge_start(WheelUpdaterBridge *bridge, const uint8_t *request,
                                uint8_t length) {
    return start_exchange(bridge, request, length, false);
}

bool wheel_updater_bridge_start_probe(WheelUpdaterBridge *bridge, const uint8_t *request,
                                      uint8_t length) {
    return start_exchange(bridge, request, length, true);
}

/**
 * @brief Advances the retry-response service-tick counter.
 *
 * Uses the post-increment and strict greater-than comparison implemented by the reference
 * bridge, so the response remains active through tick 0x7D0 and completes on the next tick.
 *
 * @param[in,out] bridge Active updater bridge with retry-response handling enabled.
 * @return True when the retry-response service-tick limit has elapsed.
 */
static bool retry_timeout_expired(WheelUpdaterBridge *bridge) {
    uint16_t previous_ticks = bridge->service_ticks++;
    return previous_ticks > WHEEL_UPDATER_RETRY_TIMEOUT_TICKS;
}

/**
 * @brief Advances one updater request and response exchange.
 *
 * Keeps normal transport failures retryable, reads a route probe immediately after its request
 * write, waits two bridge service ticks before a normal response read, recognizes response opcodes
 * 0xA1, 0xA2, 0xA4, and 0xA7, retains all response fragments through an 0xA1 continuation, and
 * executes a zero-length variable-payload read. The retry response uses the official 16-bit
 * service-tick timeout. Starting an exchange and completing its write preserve that counter; the
 * read-delay and retry-continuation transitions own its resets. A route probe keeps terminal
 * failure behavior and ignores stray non-marker bytes without restarting the request.
 *
 * @param[in,out] bridge Active updater bridge to advance.
 * @param[in] io Transport completion state; service timing uses bridge invocation ticks.
 * @return Next wheel write or read operation, or no operation while waiting or response-ready.
 */
WheelUpdaterOperation wheel_updater_bridge_step(WheelUpdaterBridge *bridge, WheelUpdaterIo io) {
    if (bridge == NULL || bridge->phase == WHEEL_UPDATER_BRIDGE_IDLE ||
        bridge->phase == WHEEL_UPDATER_BRIDGE_RESPONSE_READY) {
        return (WheelUpdaterOperation){0};
    }

    if (bridge->phase == WHEEL_UPDATER_BRIDGE_READ_HEADER && bridge->retry_response &&
        io.status != WHEEL_UPDATER_IO_COMPLETE && retry_timeout_expired(bridge)) {
        bridge->retry_response = false;
        return finish_response(bridge);
    }

    if (io.status == WHEEL_UPDATER_IO_FAILED) {
        if (bridge->response_probe) {
            bridge->phase = WHEEL_UPDATER_BRIDGE_IDLE;
            return (WheelUpdaterOperation){0};
        }
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
        bridge->retry_response = false;
        if (bridge->response_probe) {
            bridge->phase = WHEEL_UPDATER_BRIDGE_READ_HEADER;
            return current_operation(bridge);
        }
        bridge->phase = WHEEL_UPDATER_BRIDGE_READ_DELAY;
        return (WheelUpdaterOperation){0};
    }

    if (bridge->phase == WHEEL_UPDATER_BRIDGE_READ_DELAY) {
        uint16_t previous_ticks = bridge->service_ticks++;
        if (previous_ticks < WHEEL_UPDATER_READ_DELAY_TICKS - 1) {
            return (WheelUpdaterOperation){0};
        }
        bridge->service_ticks = 0;
        bridge->phase = WHEEL_UPDATER_BRIDGE_READ_HEADER;
        return current_operation(bridge);
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
            if (bridge->response_probe) {
                bridge->phase = WHEEL_UPDATER_BRIDGE_IDLE;
                return (WheelUpdaterOperation){0};
            }
            if (bridge->retry_response) {
                bridge->retry_response = false;
                return finish_response(bridge);
            }
            return current_operation(bridge);
        }
        if (io.data[0] != WHEEL_UPDATER_FRAME_MARKER) {
            if (bridge->response_probe) {
                return current_operation(bridge);
            }
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
        if (bridge->response_probe && opcode != WHEEL_UPDATER_FIXED_OPCODE) {
            bridge->phase = WHEEL_UPDATER_BRIDGE_IDLE;
            return (WheelUpdaterOperation){0};
        }
        bridge->response[bridge->response_length + 1] = opcode;
        if (opcode == WHEEL_UPDATER_RETRY_OPCODE) {
            bridge->response_length += 2;
            bridge->retry_response = true;
            bridge->service_ticks = 0;
            bridge->phase = WHEEL_UPDATER_BRIDGE_READ_HEADER;
            return current_operation(bridge);
        }
        if (opcode == WHEEL_UPDATER_ACKNOWLEDGEMENT_OPCODE) {
            bridge->response_length += 2;
            bridge->retry_response = false;
            return finish_response(bridge);
        }
        if (opcode == WHEEL_UPDATER_FIXED_OPCODE) {
            bridge->response_length += 2;
            bridge->retry_response = false;
            bridge->phase = WHEEL_UPDATER_BRIDGE_READ_FIXED_PAYLOAD;
            return current_operation(bridge);
        }
        if (opcode == WHEEL_UPDATER_VARIABLE_OPCODE) {
            bridge->response_length += 2;
            bridge->retry_response = false;
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
 * Exposes the retained response and returns the bridge to idle for the next updater request.
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
