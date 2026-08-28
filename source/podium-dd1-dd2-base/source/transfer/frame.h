#ifndef PODIUM_DD1_DD2_BASE_TRANSFER_FRAME_H
#define PODIUM_DD1_DD2_BASE_TRANSFER_FRAME_H

#include <stdint.h>

enum {
    TRANSFER_FRAME_START = 0x3c,
    TRANSFER_FRAME_END = 0x3e,
    TRANSFER_FRAME_MAX_PAYLOAD_SIZE = 125,
    TRANSFER_FRAME_MAX_SEND_PAYLOAD_SIZE = 124,
    TRANSFER_FRAME_MAX_RECEIVED_SIZE = 135,
    TRANSFER_FRAME_MAX_ENCODED_SIZE = 256,
};

typedef struct {
    uint16_t command;
    uint8_t payload[TRANSFER_FRAME_MAX_PAYLOAD_SIZE];
    uint8_t payload_length;
} TransferFrame;

typedef enum {
    TRANSFER_FRAME_VALID,
    TRANSFER_FRAME_INVALID_LENGTH,
    TRANSFER_FRAME_INVALID_BOUNDARY,
    TRANSFER_FRAME_INVALID_ESCAPE,
    TRANSFER_FRAME_INVALID_CHECKSUM,
} TransferFrameResult;

uint8_t transfer_command_group(uint16_t command);
uint8_t transfer_command_type(uint16_t command);
uint8_t transfer_command_sequence(uint16_t command);
uint8_t transfer_command_progress(uint16_t command);
uint8_t transfer_command_parameter(uint16_t command);
uint16_t transfer_empty_command(void);
uint16_t transfer_data_command(uint8_t group, uint8_t sequence, uint8_t parameter);
uint16_t transfer_status_command(uint8_t group, uint8_t parameter);
uint16_t transfer_progress_command(uint8_t group, uint8_t parameter, uint8_t sequence);
uint16_t transfer_frame_encode_values(uint16_t command, const uint8_t *payload,
                                      uint8_t payload_length,
                                      uint8_t output[TRANSFER_FRAME_MAX_ENCODED_SIZE]);
uint16_t transfer_frame_encode(const TransferFrame *frame,
                               uint8_t output[TRANSFER_FRAME_MAX_ENCODED_SIZE]);
TransferFrameResult transfer_frame_decode(const uint8_t *input, uint16_t input_length,
                                          TransferFrame *frame);

#endif
