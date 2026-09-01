#ifndef OPENTEC_BASE_USB_PLAYSTATION_AUTHENTICATION_H
#define OPENTEC_BASE_USB_PLAYSTATION_AUTHENTICATION_H

#include <stdbool.h>
#include <stdint.h>

/** @brief Sizes of PlayStation authentication reports and payloads. */
enum {
    USB_PLAYSTATION_AUTHENTICATION_REPORT_SIZE =
        64, /**< Upload and download report size in bytes. */
    USB_PLAYSTATION_AUTHENTICATION_REQUEST_SIZE =
        0x100, /**< Complete authentication request size. */
    USB_PLAYSTATION_AUTHENTICATION_RESPONSE_SIZE =
        0x410, /**< Complete authentication response size. */
    USB_PLAYSTATION_AUTHENTICATION_STATUS_REPORT_SIZE = 16, /**< Status report size in bytes. */
    USB_PLAYSTATION_AUTHENTICATION_FORMAT_REPORT_SIZE = 8,  /**< Format report size in bytes. */
};

/** @brief Status values returned in PlayStation authentication report F2. */
typedef enum {
    USB_PLAYSTATION_AUTHENTICATION_IDLE =
        0, /**< No authentication response is being transferred. */
    USB_PLAYSTATION_AUTHENTICATION_RESPONSE_ACTIVE = 1, /**< A response transfer has started. */
    USB_PLAYSTATION_AUTHENTICATION_PENDING = 0x10, /**< A complete request awaits processing. */
    USB_PLAYSTATION_AUTHENTICATION_CHECKSUM_ERROR =
        0xf0, /**< The received request checksum failed. */
    USB_PLAYSTATION_AUTHENTICATION_RESPONSE_ERROR =
        0xf2, /**< Authentication response processing failed. */
} UsbPlaystationAuthenticationStatus;

/** @brief State for assembling requests and exposing a retained authentication response. */
typedef struct {
    uint8_t request[USB_PLAYSTATION_AUTHENTICATION_REQUEST_SIZE]; /**< Assembled authentication
                                                                     request bytes. */
    const uint8_t *response;     /**< Caller-owned response bytes retained during download. */
    uint8_t sequence;            /**< Sequence value used by status and response reports. */
    uint8_t response_index;      /**< Index of the next response fragment. */
    uint8_t receive_chunk_size;  /**< Request fragment payload size from the format report. */
    uint8_t transmit_chunk_size; /**< Response fragment payload size from the format report. */
    UsbPlaystationAuthenticationStatus status; /**< Current authentication status. */
    bool checksum_enabled; /**< True when request and response CRC fields are enabled. */
    bool request_ready;    /**< True when a complete request is ready to be taken. */
    bool response_ready;   /**< True while response fragments remain to be reported. */
} UsbPlaystationAuthentication;

/**
 * @brief Initializes PlayStation authentication state.
 *
 * Clears request and response state, selects 56-byte request and response fragments, and disables
 * optional CRC fields.
 *
 * @param[out] authentication Authentication state to initialize.
 */
void usb_playstation_authentication_init(UsbPlaystationAuthentication *authentication);

/**
 * @brief Builds the PlayStation authentication format report.
 *
 * Writes report F3 advertising 56-byte upload and download fragments without optional CRC fields,
 * and stores those settings in authentication.
 *
 * @param[in,out] authentication Authentication state whose transfer settings are updated.
 * @param[out] report Buffer for USB_PLAYSTATION_AUTHENTICATION_FORMAT_REPORT_SIZE bytes.
 */
void usb_playstation_authentication_format_report(
    UsbPlaystationAuthentication *authentication,
    uint8_t report[USB_PLAYSTATION_AUTHENTICATION_FORMAT_REPORT_SIZE]);

/**
 * @brief Accepts one PlayStation authentication request fragment.
 *
 * Validates the report identifier, fragment index, and optional CRC, then copies the fragment into
 * the request buffer. Fragment four marks the complete 256-byte request ready for processing.
 *
 * @param[in,out] authentication Authentication state receiving the fragment.
 * @param[in] report Sixty-four-byte authentication upload report.
 * @return True when the fragment is accepted; otherwise false.
 */
bool usb_playstation_authentication_receive(
    UsbPlaystationAuthentication *authentication,
    const uint8_t report[USB_PLAYSTATION_AUTHENTICATION_REPORT_SIZE]);

/**
 * @brief Takes a completed PlayStation authentication request.
 *
 * Copies the assembled request into request and clears its ready flag so it is returned only once.
 *
 * @param[in,out] authentication Authentication state containing the request.
 * @param[out] request Buffer for USB_PLAYSTATION_AUTHENTICATION_REQUEST_SIZE request bytes.
 * @return True when a complete request was available and copied; otherwise false.
 */
bool usb_playstation_authentication_take_request(
    UsbPlaystationAuthentication *authentication,
    uint8_t request[USB_PLAYSTATION_AUTHENTICATION_REQUEST_SIZE]);

/**
 * @brief Publishes a completed PlayStation authentication response.
 *
 * Retains the caller-owned response pointer, resets fragment progress, and marks the response ready
 * for report F1 downloads. The response storage must remain valid until the final fragment is read.
 *
 * @param[in,out] authentication Authentication state to update.
 * @param[in] response Caller-owned response bytes to retain.
 * @param[in] response_length Number of response bytes; it must equal the defined response size.
 * @return True when the pointers are valid and response_length is accepted; otherwise false.
 */
bool usb_playstation_authentication_publish_response(UsbPlaystationAuthentication *authentication,
                                                     const uint8_t *response,
                                                     uint16_t response_length);

/**
 * @brief Marks PlayStation authentication response processing as failed.
 *
 * Clears pending request and response availability, releases the retained response pointer, and
 * sets the status reported by report F2 to response error.
 *
 * @param[in,out] authentication Authentication state to mark failed.
 */
void usb_playstation_authentication_fail(UsbPlaystationAuthentication *authentication);

/**
 * @brief Builds the PlayStation authentication status report.
 *
 * Writes report F2 with the current sequence and status, adding a CRC over the first twelve bytes
 * when checksum reporting is enabled.
 *
 * @param[in] authentication Authentication state to report.
 * @param[out] report Buffer for USB_PLAYSTATION_AUTHENTICATION_STATUS_REPORT_SIZE bytes.
 */
void usb_playstation_authentication_status_report(
    const UsbPlaystationAuthentication *authentication,
    uint8_t report[USB_PLAYSTATION_AUTHENTICATION_STATUS_REPORT_SIZE]);

/**
 * @brief Builds the next PlayStation authentication response report.
 *
 * Writes report F1 for the current fragment, advances the response index, and clears response
 * availability after the complete response has been emitted.
 *
 * @param[in,out] authentication Authentication state with a response ready for download.
 * @param[out] report Buffer for USB_PLAYSTATION_AUTHENTICATION_REPORT_SIZE bytes.
 * @return True when a response fragment was written; otherwise false.
 */
bool usb_playstation_authentication_response_report(
    UsbPlaystationAuthentication *authentication,
    uint8_t report[USB_PLAYSTATION_AUTHENTICATION_REPORT_SIZE]);

/**
 * @brief Tests whether a PlayStation authentication response remains active.
 *
 * Reports whether the published response still has an unconsumed fragment.
 *
 * @param[in] authentication Authentication state to inspect.
 * @return True when response fragments remain; otherwise false.
 */
bool usb_playstation_authentication_response_active(
    const UsbPlaystationAuthentication *authentication);

#endif
