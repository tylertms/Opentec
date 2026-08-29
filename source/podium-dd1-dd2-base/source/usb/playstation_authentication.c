#include "usb/playstation_authentication.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

enum {
    PLAYSTATION_AUTHENTICATION_UPLOAD_REPORT = 0xf0,
    PLAYSTATION_AUTHENTICATION_DOWNLOAD_REPORT = 0xf1,
    PLAYSTATION_AUTHENTICATION_STATUS_REPORT = 0xf2,
    PLAYSTATION_AUTHENTICATION_FORMAT_REPORT = 0xf3,
    PLAYSTATION_AUTHENTICATION_DATA_OFFSET = 4,
    PLAYSTATION_AUTHENTICATION_CHECKSUM_OFFSET = 60,
    PLAYSTATION_AUTHENTICATION_CHECKSUM_INPUT_SIZE = 60,
    PLAYSTATION_AUTHENTICATION_STATUS_CHECKSUM_OFFSET = 12,
    PLAYSTATION_AUTHENTICATION_STATUS_CHECKSUM_INPUT_SIZE = 12,
    PLAYSTATION_AUTHENTICATION_CHUNK_SIZE = 0x38,
    PLAYSTATION_AUTHENTICATION_FINAL_REQUEST_INDEX = 4,
};

/**
 * @brief Calculates a PlayStation authentication report checksum.
 *
 * Applies the reflected CRC-32 polynomial with an all-ones initial value and final complement.
 *
 * @param[in] data Bytes covered by the checksum.
 * @param[in] length Number of bytes to process.
 * @return CRC-32 value for the supplied bytes.
 */
static uint32_t calculate_crc32(const uint8_t *data, uint8_t length) {
    uint32_t checksum = UINT32_MAX;
    for (uint8_t index = 0; index < length; index++) {
        checksum ^= data[index];
        for (uint8_t bit = 0; bit < 8; bit++) {
            checksum = checksum >> 1 ^ (0xedb88320u & (uint32_t)-(int32_t)(checksum & 1u));
        }
    }
    return ~checksum;
}

/**
 * @brief Reads a little-endian checksum from a feature report.
 *
 * Combines four consecutive report bytes into the checksum value carried on the control pipe.
 *
 * @param[in] data First byte of the encoded checksum.
 * @return Decoded 32-bit checksum.
 */
static uint32_t read_checksum(const uint8_t *data) {
    return (uint32_t)data[0] | (uint32_t)data[1] << 8 | (uint32_t)data[2] << 16 |
           (uint32_t)data[3] << 24;
}

/**
 * @brief Writes a little-endian checksum into a feature report.
 *
 * Stores the least-significant checksum byte first in the four-byte report field.
 *
 * @param[out] data First byte of the encoded checksum.
 * @param[in] checksum Checksum value to encode.
 */
static void write_checksum(uint8_t *data, uint32_t checksum) {
    data[0] = (uint8_t)checksum;
    data[1] = (uint8_t)(checksum >> 8);
    data[2] = (uint8_t)(checksum >> 16);
    data[3] = (uint8_t)(checksum >> 24);
}

/**
 * @brief Initializes PlayStation authentication transport state.
 *
 * Clears both transfer payloads and selects the 56-byte fragment format advertised to the host.
 *
 * @param[out] authentication Authentication transport state.
 */
void usb_playstation_authentication_init(UsbPlaystationAuthentication *authentication) {
    if (authentication == 0) {
        return;
    }
    *authentication = (UsbPlaystationAuthentication){
        .receive_chunk_size = PLAYSTATION_AUTHENTICATION_CHUNK_SIZE,
        .transmit_chunk_size = PLAYSTATION_AUTHENTICATION_CHUNK_SIZE,
    };
}

/**
 * @brief Builds the PlayStation authentication transfer-format report.
 *
 * Returns report F3 with 56-byte upload and download fragments and disables optional CRC fields.
 *
 * @param[in,out] authentication Authentication transport state.
 * @param[out] report Eight-byte feature report.
 */
void usb_playstation_authentication_format_report(
    UsbPlaystationAuthentication *authentication,
    uint8_t report[USB_PLAYSTATION_AUTHENTICATION_FORMAT_REPORT_SIZE]) {
    if (authentication == 0 || report == 0) {
        return;
    }
    memset(report, 0, USB_PLAYSTATION_AUTHENTICATION_FORMAT_REPORT_SIZE);
    report[0] = PLAYSTATION_AUTHENTICATION_FORMAT_REPORT;
    report[2] = PLAYSTATION_AUTHENTICATION_CHUNK_SIZE;
    report[3] = PLAYSTATION_AUTHENTICATION_CHUNK_SIZE;
    authentication->receive_chunk_size = report[2] & 0x7fu;
    authentication->transmit_chunk_size = report[3] & 0x7fu;
    authentication->checksum_enabled = (report[3] & 0x80u) != 0;
}

/**
 * @brief Accepts one PlayStation authentication request fragment.
 *
 * Stores the report sequence and indexed payload. Fragment four completes the 256-byte request,
 * resets the response cursor, and exposes the request to the secure-element service.
 *
 * @param[in,out] authentication Authentication transport state.
 * @param[in] report Sixty-four-byte F0 feature report.
 * @return True when the fragment is well formed and accepted; otherwise false.
 */
bool usb_playstation_authentication_receive(
    UsbPlaystationAuthentication *authentication,
    const uint8_t report[USB_PLAYSTATION_AUTHENTICATION_REPORT_SIZE]) {
    if (authentication == 0 || report == 0 ||
        report[0] != PLAYSTATION_AUTHENTICATION_UPLOAD_REPORT ||
        authentication->receive_chunk_size == 0 ||
        report[2] > PLAYSTATION_AUTHENTICATION_FINAL_REQUEST_INDEX) {
        return false;
    }
    if (authentication->checksum_enabled &&
        read_checksum(&report[PLAYSTATION_AUTHENTICATION_CHECKSUM_OFFSET]) !=
            calculate_crc32(report, PLAYSTATION_AUTHENTICATION_CHECKSUM_INPUT_SIZE)) {
        authentication->status = USB_PLAYSTATION_AUTHENTICATION_CHECKSUM_ERROR;
        authentication->request_ready = false;
        return false;
    }

    uint16_t offset = (uint16_t)report[2] * authentication->receive_chunk_size;
    uint16_t remaining = USB_PLAYSTATION_AUTHENTICATION_REQUEST_SIZE - offset;
    uint8_t count = remaining < authentication->receive_chunk_size
                        ? (uint8_t)remaining
                        : authentication->receive_chunk_size;
    memcpy(authentication->request + offset, report + PLAYSTATION_AUTHENTICATION_DATA_OFFSET,
           count);
    authentication->sequence = report[1];

    if (report[2] == PLAYSTATION_AUTHENTICATION_FINAL_REQUEST_INDEX) {
        authentication->response_index = 0;
        authentication->status = USB_PLAYSTATION_AUTHENTICATION_PENDING;
        authentication->request_ready = true;
        authentication->response_ready = false;
    }
    return true;
}

/**
 * @brief Takes a completed PlayStation authentication request.
 *
 * Copies the assembled 256-byte challenge once and retains the pending status while it is handled.
 *
 * @param[in,out] authentication Authentication transport state.
 * @param[out] request Completed authentication challenge.
 * @return True when a completed request was available; otherwise false.
 */
bool usb_playstation_authentication_take_request(
    UsbPlaystationAuthentication *authentication,
    uint8_t request[USB_PLAYSTATION_AUTHENTICATION_REQUEST_SIZE]) {
    if (authentication == 0 || request == 0 || !authentication->request_ready) {
        return false;
    }
    memcpy(request, authentication->request, USB_PLAYSTATION_AUTHENTICATION_REQUEST_SIZE);
    authentication->request_ready = false;
    return true;
}

/**
 * @brief Publishes a completed PlayStation authentication response.
 *
 * Retains the exact 1,040-byte response, resets its fragment cursor, and changes the reported
 * status to idle so the host can begin retrieving report F1. The response must remain available
 * until all fragments have been read.
 *
 * @param[in,out] authentication Authentication transport state.
 * @param[in] response Complete authentication response.
 * @param[in] response_length Number of response bytes.
 * @return True when the complete response was accepted; otherwise false.
 */
bool usb_playstation_authentication_publish_response(UsbPlaystationAuthentication *authentication,
                                                     const uint8_t *response,
                                                     uint16_t response_length) {
    if (authentication == 0 || response == 0 ||
        response_length != USB_PLAYSTATION_AUTHENTICATION_RESPONSE_SIZE) {
        return false;
    }
    authentication->response = response;
    authentication->response_index = 0;
    authentication->status = USB_PLAYSTATION_AUTHENTICATION_IDLE;
    authentication->response_ready = true;
    return true;
}

/**
 * @brief Marks the PlayStation authentication response as failed.
 *
 * Discards pending request and response availability and exposes status F2 to the host.
 *
 * @param[in,out] authentication Authentication transport state.
 */
void usb_playstation_authentication_fail(UsbPlaystationAuthentication *authentication) {
    if (authentication == 0) {
        return;
    }
    authentication->status = USB_PLAYSTATION_AUTHENTICATION_RESPONSE_ERROR;
    authentication->request_ready = false;
    authentication->response_ready = false;
    authentication->response = 0;
    authentication->response_index = 0;
}

/**
 * @brief Builds the PlayStation authentication status report.
 *
 * Returns report F2 with the current sequence and authentication status. When enabled, the final
 * four bytes contain a CRC over the first twelve bytes.
 *
 * @param[in] authentication Authentication transport state.
 * @param[out] report Sixteen-byte feature report.
 */
void usb_playstation_authentication_status_report(
    const UsbPlaystationAuthentication *authentication,
    uint8_t report[USB_PLAYSTATION_AUTHENTICATION_STATUS_REPORT_SIZE]) {
    if (authentication == 0 || report == 0) {
        return;
    }
    memset(report, 0, USB_PLAYSTATION_AUTHENTICATION_STATUS_REPORT_SIZE);
    report[0] = PLAYSTATION_AUTHENTICATION_STATUS_REPORT;
    report[1] = authentication->sequence;
    report[2] = (uint8_t)authentication->status;
    if (authentication->checksum_enabled) {
        write_checksum(
            &report[PLAYSTATION_AUTHENTICATION_STATUS_CHECKSUM_OFFSET],
            calculate_crc32(report, PLAYSTATION_AUTHENTICATION_STATUS_CHECKSUM_INPUT_SIZE));
    }
}

/**
 * @brief Builds the next PlayStation authentication response fragment.
 *
 * Returns report F1 with the current sequence and fragment index. The first eighteen fragments
 * contain 56 bytes and the final fragment contains 32 bytes. The first response read changes the
 * status from idle to active.
 *
 * @param[in,out] authentication Authentication transport state.
 * @param[out] report Sixty-four-byte feature report.
 * @return True when a response fragment was available; otherwise false.
 */
bool usb_playstation_authentication_response_report(
    UsbPlaystationAuthentication *authentication,
    uint8_t report[USB_PLAYSTATION_AUTHENTICATION_REPORT_SIZE]) {
    if (authentication == 0 || report == 0 || !authentication->response_ready ||
        authentication->response == 0 || authentication->transmit_chunk_size == 0) {
        return false;
    }

    uint16_t offset =
        (uint16_t)authentication->response_index * authentication->transmit_chunk_size;
    if (offset >= USB_PLAYSTATION_AUTHENTICATION_RESPONSE_SIZE) {
        return false;
    }
    uint16_t remaining = USB_PLAYSTATION_AUTHENTICATION_RESPONSE_SIZE - offset;
    uint8_t count = remaining < authentication->transmit_chunk_size
                        ? (uint8_t)remaining
                        : authentication->transmit_chunk_size;

    memset(report, 0, USB_PLAYSTATION_AUTHENTICATION_REPORT_SIZE);
    report[0] = PLAYSTATION_AUTHENTICATION_DOWNLOAD_REPORT;
    report[1] = authentication->sequence;
    report[2] = authentication->response_index;
    memcpy(report + PLAYSTATION_AUTHENTICATION_DATA_OFFSET, authentication->response + offset,
           count);
    if (authentication->checksum_enabled) {
        write_checksum(&report[PLAYSTATION_AUTHENTICATION_CHECKSUM_OFFSET],
                       calculate_crc32(report, PLAYSTATION_AUTHENTICATION_CHECKSUM_INPUT_SIZE));
    }
    if (authentication->status == USB_PLAYSTATION_AUTHENTICATION_IDLE) {
        authentication->status = USB_PLAYSTATION_AUTHENTICATION_RESPONSE_ACTIVE;
    }
    authentication->response_index++;
    if ((uint32_t)offset + count == USB_PLAYSTATION_AUTHENTICATION_RESPONSE_SIZE) {
        authentication->response_ready = false;
        authentication->response = 0;
    }
    return true;
}

/**
 * @brief Reports whether a PlayStation authentication response remains available.
 *
 * Keeps the response owner informed until the host consumes the final F1 fragment.
 *
 * @param[in] authentication Authentication transport state.
 * @return True while response fragments remain; otherwise false.
 */
bool usb_playstation_authentication_response_active(
    const UsbPlaystationAuthentication *authentication) {
    return authentication != 0 && authentication->response_ready;
}
