#ifndef OPENTEC_BASE_I2C_PROBE_H
#define OPENTEC_BASE_I2C_PROBE_H

#include <stdbool.h>
#include <stdint.h>

typedef enum {
    I2C_PROBE_RESPONSE_RETRY,
    I2C_PROBE_RESPONSE_BUSY,
    I2C_PROBE_RESPONSE_ACCEPTED,
    I2C_PROBE_RESPONSE_REJECTED,
} I2cProbeResponseResult;

typedef enum {
    I2C_PROBE_VALID,
    I2C_PROBE_CHECKSUM_ERROR,
    I2C_PROBE_MALFORMED_RESPONSE,
} I2cProbeValidationResult;

typedef struct {
    uint8_t retry_count;
} I2cProbeHandshake;

typedef enum {
    I2C_PROBE_BEGIN_SESSION = 1,
    I2C_PROBE_READ_STARTUP_STATUS = 2,
    I2C_PROBE_READ_SIGNATURE = 3,
    I2C_PROBE_READ_CONFIRMATION = 4,
    I2C_PROBE_READ_READY_STATUS = 5,
    I2C_PROBE_WRITE_CHUNK = 9,
    I2C_PROBE_WRITE_CHECKED_CHUNK = 10,
    I2C_PROBE_READ_CHUNK = 11,
    I2C_PROBE_READ_CHECKED_CHUNK = 12,
    I2C_PROBE_FINISH_TRANSFER = 13,
    I2C_PROBE_FINISH_CHECKED_TRANSFER = 14,
} I2cProbeCommand;

typedef struct {
    uint8_t selector;
    uint8_t response_length;
} I2cProbeRequest;

enum {
    I2C_PROBE_TRANSFER_CHUNK_CAPACITY = 64,
    I2C_PROBE_TRANSFER_WRITE_CAPACITY = I2C_PROBE_TRANSFER_CHUNK_CAPACITY + 8,
    I2C_PROBE_TRANSFER_WRITE_SIZE = 0x100,
    I2C_PROBE_TRANSFER_READ_SIZE = 0x410,
};

typedef struct {
    uint8_t phase;
    uint8_t chunk_index;
    const uint8_t *chunk;
    uint8_t chunk_length;
} I2cProbeTransferInput;

typedef struct {
    uint8_t selector;
    uint8_t write_data[I2C_PROBE_TRANSFER_WRITE_CAPACITY];
    uint8_t write_length;
    uint8_t response_length;
    uint8_t response_payload_offset;
    uint8_t response_payload_length;
    uint8_t response_integrity_offset;
    uint8_t response_integrity_length;
} I2cProbeTransferFrame;

typedef struct {
    const uint8_t *payload;
    uint8_t payload_length;
} I2cProbeTransferResponse;

typedef enum {
    I2C_PROBE_TRANSFER_WRITING,
    I2C_PROBE_TRANSFER_READING,
    I2C_PROBE_TRANSFER_FINISHING,
    I2C_PROBE_TRANSFER_COMPLETE,
} I2cProbeTransferStage;

typedef struct {
    I2cProbeTransferStage stage;
    uint8_t phase;
    uint8_t chunk_index;
    bool checked;
} I2cProbeTransferSequence;

typedef struct {
    I2cProbeCommand command;
    uint16_t buffer_offset;
    uint8_t phase;
    uint8_t chunk_index;
    uint8_t chunk_length;
} I2cProbeTransferStep;

typedef enum {
    I2C_PROBE_EXCHANGE_WAIT_READY,
    I2C_PROBE_EXCHANGE_QUEUE_COMMAND,
    I2C_PROBE_EXCHANGE_WAIT_ACCEPTANCE,
    I2C_PROBE_EXCHANGE_WAIT_RESPONSE,
    I2C_PROBE_EXCHANGE_COMPLETE,
    I2C_PROBE_EXCHANGE_FAILED,
} I2cProbeExchangeStage;

typedef enum {
    I2C_PROBE_EXCHANGE_PENDING,
    I2C_PROBE_EXCHANGE_SUCCEEDED,
    I2C_PROBE_EXCHANGE_COMMAND_ERROR,
    I2C_PROBE_EXCHANGE_CHECKSUM_ERROR,
    I2C_PROBE_EXCHANGE_RESPONSE_ERROR,
} I2cProbeExchangeResult;

typedef struct {
    I2cProbeExchangeStage stage;
    I2cProbeExchangeResult result;
    I2cProbeHandshake readiness;
} I2cProbeExchange;

typedef struct {
    uint8_t declared_length;
    uint8_t status;
    const uint8_t *payload;
    uint8_t payload_length;
} I2cProbeStartupResponse;

typedef struct {
    I2cProbeCommand command;
    uint8_t completed_attempts;
    uint32_t retry_after_ms;
    bool waiting;
    bool complete;
} I2cProbeStartup;

void i2c_probe_handshake_init(I2cProbeHandshake *handshake);
I2cProbeResponseResult i2c_probe_handshake_evaluate(I2cProbeHandshake *handshake, uint8_t response);
I2cProbeResponseResult i2c_probe_command_response_evaluate(uint8_t response);
uint8_t i2c_probe_checksum(const uint8_t *payload, uint8_t payload_length);
bool i2c_probe_request_encode(I2cProbeCommand command, I2cProbeRequest *request);
const I2cProbeRequest *i2c_probe_request_lookup(I2cProbeCommand command);
bool i2c_probe_transfer_encode(I2cProbeCommand command, const I2cProbeTransferInput *input,
                               I2cProbeTransferFrame *frame);
I2cProbeValidationResult
i2c_probe_transfer_response_parse(const I2cProbeTransferFrame *frame, const uint8_t *response,
                                  uint8_t response_length,
                                  I2cProbeTransferResponse *parsed_response);
void i2c_probe_transfer_sequence_init(I2cProbeTransferSequence *sequence, bool checked);
bool i2c_probe_transfer_sequence_current(const I2cProbeTransferSequence *sequence,
                                         I2cProbeTransferStep *step);
bool i2c_probe_transfer_sequence_accept(I2cProbeTransferSequence *sequence);
void i2c_probe_exchange_init(I2cProbeExchange *exchange);
bool i2c_probe_exchange_status(I2cProbeExchange *exchange, uint8_t response);
bool i2c_probe_exchange_command_queued(I2cProbeExchange *exchange);
bool i2c_probe_exchange_finalize(I2cProbeExchange *exchange, I2cProbeValidationResult validation);
void i2c_probe_startup_init(I2cProbeStartup *startup);
bool i2c_probe_startup_current(I2cProbeStartup *startup, uint32_t now_ms, I2cProbeCommand *command);
bool i2c_probe_startup_accept(I2cProbeStartup *startup, I2cProbeCommand command,
                              const I2cProbeStartupResponse *response, uint32_t now_ms);

#endif
