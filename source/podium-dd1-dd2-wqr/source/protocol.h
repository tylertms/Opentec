#ifndef OPENTEC_WQR_PROTOCOL_H
#define OPENTEC_WQR_PROTOCOL_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "wqr_frame.h"

/** @brief Buffer sizes used by the WQR protocol service. */
enum {
    WQR_TRANSFER_CAPACITY = 512, /**< Maximum accumulated request payload bytes. */
    WQR_RESPONSE_CAPACITY = 514, /**< Maximum response payload bytes, including I2C overhead. */
    WQR_SPI_TRANSFER_SIZE = 33,  /**< Number of bytes exchanged by one primary SPI transfer. */
    WQR_SPI_RESPONSE_SIZE = 57,  /**< Number of bytes reserved for one SPI response payload. */
    WQR_STATUS_SIZE = 15         /**< Number of bytes in the encoded status payload. */
};

/**
 * @brief Result of one asynchronous WQR platform operation.
 *
 * Platform callbacks return pending until their interrupt-driven operation reaches a terminal
 * state, then return either success or failure.
 */
typedef enum {
    WQR_IO_PENDING,   /**< The operation is still in progress. */
    WQR_IO_SUCCEEDED, /**< The operation completed successfully. */
    WQR_IO_FAILED     /**< The operation completed with an error. */
} wqr_io_result;

/**
 * @brief Platform operations used by the WQR protocol service.
 *
 * Callbacks are optional, and absent callbacks use the protocol's defined fallback behavior. The
 * context member is passed to every installed callback unchanged.
 */
typedef struct {
    void *context; /**< Opaque platform context passed to callbacks. */
    wqr_io_result (*spi_transfer)(
        void *context, const uint8_t *transmit, uint8_t *receive,
        size_t length); /**< Starts or polls one byte-oriented SPI transfer. */
    wqr_io_result (*spi_word)(
        void *context, uint16_t transmit,
        uint16_t *receive); /**< Starts or polls one word-oriented SPI transfer. */
    wqr_io_result (*i2c_write)(void *context, uint8_t address, const uint8_t *data,
                               size_t length); /**< Starts or polls one I2C write. */
    wqr_io_result (*i2c_read)(
        void *context, uint8_t address, uint8_t command, uint8_t *data,
        size_t length);                    /**< Starts or polls one command-addressed I2C read. */
    uint8_t (*read_inputs)(void *context); /**< Reads the three protocol input bits. */
    bool (*transfer_ready)(void *context); /**< Reports whether the peripheral peer is present. */
    bool (*transfer_control_ready)(
        void *context); /**< Reports whether transfer control is acknowledged. */
    void (*set_transfer_control)(void *context,
                                 bool asserted); /**< Drives the local transfer-control output. */
    void (*reset_transfer)(void *context);       /**< Resets platform peripheral-transfer state. */
    void (*request_reset)(void *context); /**< Defers a system reset until the response is sent. */
} wqr_io;

/**
 * @brief Mutable state for one WQR protocol endpoint.
 *
 * The state owns request accumulation and response staging while the platform interface performs
 * the asynchronous peripheral operations used by completed requests.
 */
typedef struct {
    uint8_t receive_payload[WQR_TRANSFER_CAPACITY];    /**< Accumulated request payload bytes. */
    uint8_t transmit_payload[WQR_RESPONSE_CAPACITY];   /**< Staged response payload bytes. */
    uint8_t primary_response[WQR_SPI_RESPONSE_SIZE];   /**< Last primary SPI response payload. */
    uint8_t alternate_response[WQR_SPI_RESPONSE_SIZE]; /**< Last alternate SPI response payload. */
    uint8_t last_fragment[WQR_FRAME_SIZE]; /**< Most recently accepted fragment for duplicate
                                              detection. */
    uint8_t last_request[WQR_FRAME_SIZE];  /**< Most recently accepted terminal request frame for
                                              duplicate detection. */
    wqr_io io;                             /**< Platform callbacks used by the endpoint. */

    size_t receive_length;  /**< Number of valid bytes in receive_payload. */
    size_t transmit_length; /**< Number of valid bytes in transmit_payload. */
    size_t transmit_offset; /**< Offset of the next response fragment in transmit_payload. */
    uint32_t error_count;   /**< Number of protocol errors recorded by the endpoint. */
    volatile uint32_t milliseconds;   /**< Elapsed protocol time in milliseconds. */
    volatile uint32_t seconds;        /**< Elapsed protocol time in seconds. */
    volatile uint32_t transmit_count; /**< Number of response frames sent. */

    int16_t sensor_value;     /**< Converted sensor value included in status responses. */
    uint8_t inputs;           /**< Three-bit digital input value included in status responses. */
    uint8_t payload_type;     /**< Type of the accumulated request payload. */
    uint8_t response_type;    /**< Type of the queued response payload. */
    uint8_t expected_command; /**< Command code carried by the next control response. */
    uint8_t sequence;         /**< Next sequence value expected or emitted. */
    uint8_t transfer_detail;  /**< Detail code describing the active peripheral transfer. */
    uint8_t command_marker;   /**< Reset marker echoed in the status payload when requested. */
    uint8_t transfer_state;   /**< Current peripheral transfer-handshake state. */
    uint8_t control_type;     /**< Queued control response type, ACK or NACK. */
    uint8_t control_payload;  /**< Command code carried by the queued control response. */
    uint8_t control_sequence; /**< Sequence value carried by the queued control response. */

    bool peer_ready_confirmed;       /**< Whether the peer has been detected by the handshake. */
    bool transfer_enabled;           /**< Whether the peripheral transfer handshake is ready. */
    bool payload_pending;            /**< Whether an accepted complete request awaits polling. */
    bool peripheral_transfer_active; /**< Whether a platform peripheral operation is pending. */
    bool response_ready;             /**< Whether a payload response is queued for transmission. */
    bool fragment_open;              /**< Whether more request fragments are expected. */
    volatile bool reset_after_response; /**< Whether a reset is deferred until payload delivery. */
    bool transfer_control_asserted; /**< Current logical level of the local transfer-control output.
                                     */
    bool alternate_spi_active;   /**< Whether alternate SPI zero-word initialization has started. */
    volatile bool control_ready; /**< Whether an ACK or NACK response is queued. */
    bool last_fragment_valid;    /**< Whether last_fragment contains a duplicate-detection frame. */
    bool last_request_valid;     /**< Whether last_request contains a duplicate-detection frame. */
} wqr_protocol;

/**
 * @brief Initializes the complete WQR protocol state.
 *
 * Clears all runtime state, installs the optional platform interface, initializes command
 * sentinels, and samples the three digital inputs when supported.
 *
 * @param[out] protocol Protocol state to initialize.
 * @param[in] io Platform interface to copy, or null for a callback-free instance.
 */
void wqr_protocol_init(wqr_protocol *protocol, const wqr_io *io);

/**
 * @brief Validates and consumes one WQR request or control frame.
 *
 * Handles retry control, response ACKs, duplicate suppression, canonical fragment sequencing, and
 * bounded payload accumulation. Completed requests are deferred to the polling service.
 *
 * @param[in,out] protocol Protocol state to update.
 * @param[in] frame Complete received WQR frame.
 * @return True when the frame was accepted or recognized as a retry or duplicate; false when it
 * was rejected.
 */
bool wqr_protocol_receive(wqr_protocol *protocol, const uint8_t frame[WQR_FRAME_SIZE]);

/**
 * @brief Builds the next queued WQR response frame.
 *
 * Prioritizes control responses and otherwise selects the current response fragment and flags from
 * the queued payload without advancing transmission state.
 *
 * @param[in] protocol Protocol state containing the response queue.
 * @param[out] frame Complete response frame.
 * @return True when a response frame was available and built.
 */
bool wqr_protocol_response(const wqr_protocol *protocol, uint8_t frame[WQR_FRAME_SIZE]);

/**
 * @brief Tests whether the current transaction can still produce a response.
 *
 * Includes queued control or payload output and request work still waiting on dispatch or a
 * peripheral operation.
 *
 * @param[in] protocol Protocol state to inspect.
 * @return True when response-related work remains active.
 */
bool wqr_protocol_response_expected(const wqr_protocol *protocol);

/**
 * @brief Records successful transmission of the current response frame.
 *
 * Increments the official transmit counter, consumes a queued control response, and requests a
 * deferred system reset after a completed payload response when armed.
 *
 * @param[in,out] protocol Protocol state to update.
 */
void wqr_protocol_response_sent(wqr_protocol *protocol);

/**
 * @brief Services pending protocol work and the transfer handshake.
 *
 * Applies SPI transfer control before dispatch, polls asynchronous peripheral operations, advances
 * connection state, and releases completed request storage.
 *
 * @param[in,out] protocol Protocol state to service.
 */
void wqr_protocol_poll(wqr_protocol *protocol);

/**
 * @brief Advances protocol uptime by one millisecond.
 *
 * Increments the millisecond counter and derives one additional uptime second at each thousandth
 * tick.
 *
 * @param[in,out] protocol Protocol timing state to advance.
 */
void wqr_protocol_tick(wqr_protocol *protocol);

/**
 * @brief Publishes one converted sensor sample in protocol status.
 *
 * Converts the raw ADC sample through the official resistance table and stores the resulting
 * signed value for subsequent status responses.
 *
 * @param[in,out] protocol Protocol status state to update.
 * @param[in] sample Unsigned 12-bit ADC sample.
 */
void wqr_protocol_set_sensor_sample(wqr_protocol *protocol, uint16_t sample);

/**
 * @brief Converts one 12-bit sensor ADC sample to the official temperature value.
 *
 * Calculates divider resistance and linearly interpolates the 5-degree lookup table. Open and
 * out-of-range endpoints return the official sentinel values.
 *
 * @param[in] sample Unsigned ADC sample.
 * @return Interpolated temperature, `-99` below range, or `999` above range or for an open input.
 */
int16_t wqr_sensor_value(uint16_t sample);

#endif
