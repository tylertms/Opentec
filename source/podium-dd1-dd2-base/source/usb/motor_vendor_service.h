#ifndef OPENTEC_BASE_USB_MOTOR_VENDOR_SERVICE_H
#define OPENTEC_BASE_USB_MOTOR_VENDOR_SERVICE_H

#include <stdbool.h>
#include <stdint.h>

#include "motor/command_channel.h"
#include "motor/command_mailbox.h"
#include "usb/motor_command_upload.h"
#include "usb/motor_response_download.h"

/** @brief Actions produced while servicing the USB motor vendor bridge. */
typedef enum {
    USB_MOTOR_VENDOR_ACTION_NONE = 0,             /**< No action is required. */
    USB_MOTOR_VENDOR_ACTION_CLAIM = 1 << 0,       /**< Claim the shared motor transport. */
    USB_MOTOR_VENDOR_ACTION_RELEASE = 1 << 1,     /**< Release the shared motor transport. */
    USB_MOTOR_VENDOR_ACTION_RESTART = 1 << 2,     /**< Reset the motor command channel. */
    USB_MOTOR_VENDOR_ACTION_WRITE_MOTOR = 1 << 3, /**< Write motor_packet to the motor transport. */
    USB_MOTOR_VENDOR_ACTION_WRITE_USB = 1 << 4,   /**< Write usb_packet as a USB acknowledgement. */
    USB_MOTOR_VENDOR_ACTION_RESPONSE_READY =
        1 << 5, /**< A motor response is ready for USB download. */
} UsbMotorVendorAction;

/** @brief Caller-owned storage attached to the USB motor vendor service. */
typedef struct {
    uint8_t *upload_assembly;           /**< Storage for segmented USB command assembly. */
    uint16_t upload_assembly_capacity;  /**< Capacity of upload_assembly in bytes. */
    uint8_t *application_data;          /**< Storage for a motor response forwarded to USB. */
    uint16_t application_data_capacity; /**< Capacity of application_data in bytes. */
} UsbMotorVendorServiceBuffers;

/** @brief State bridging USB motor commands and the motor command channel. */
typedef struct {
    UsbMotorCommandUpload upload;         /**< USB command upload state. */
    UsbMotorResponseDownload download;    /**< USB response download state. */
    MotorCommandChannel *channel;         /**< Attached motor command channel. */
    UsbMotorVendorServiceBuffers buffers; /**< Attached caller-owned storage. */
    uint16_t response_length;             /**< Retained forwarded response length in bytes. */
    uint8_t usb_sequence;                 /**< Sequence used for the current USB response. */
    bool response_active;                 /**< True while a retained response can be downloaded. */
} UsbMotorVendorService;

/** @brief Actions and packets produced by one USB motor vendor service operation. */
typedef struct {
    UsbMotorVendorAction actions; /**< Bitmask of actions required by the caller. */
    const uint8_t *motor_packet;  /**< Motor packet to write when WRITE_MOTOR is set. */
    uint16_t motor_packet_length; /**< Number of bytes in motor_packet. */
    uint32_t motor_status;        /**< Status returned by the motor mailbox operation. */
    uint8_t usb_packet_length;    /**< Number of bytes in usb_packet when WRITE_USB is set. */
    MotorCommandMailboxExchangeEvent mailbox_event; /**< Mailbox exchange result. */
} UsbMotorVendorServiceResult;

/**
 * @brief Initializes the USB motor vendor service.
 *
 * Attaches an initialized motor command channel and caller-owned upload and response storage, then
 * resets the USB upload and response state.
 *
 * @param[out] service Service state to initialize.
 * @param[in] channel Initialized motor command channel used by the service.
 * @param[in] buffers Caller-owned storage for command assembly and response data.
 * @return True when all pointers are non-null and both storage capacities are nonzero; otherwise
 * false.
 */
bool usb_motor_vendor_service_init(UsbMotorVendorService *service, MotorCommandChannel *channel,
                                   const UsbMotorVendorServiceBuffers *buffers);

/**
 * @brief Accepts one USB motor vendor request.
 *
 * Claims motor-command report traffic with a valid report identifier and length, handles restart
 * and release controls, and translates accepted commands into USB acknowledgement and motor-channel
 * write actions.
 *
 * @param[in,out] service Active motor vendor service.
 * @param[in] request Received motor-command feature packet.
 * @param[in] length Number of bytes in request; it must equal USB_FEATURE_UPLOAD_PACKET_SIZE.
 * @param[out] usb_packet Buffer for an optional upload acknowledgement.
 * @return Actions and packet data produced by the request; no actions for invalid input.
 */
UsbMotorVendorServiceResult usb_motor_vendor_service_accept_usb(
    UsbMotorVendorService *service, const uint8_t request[USB_FEATURE_UPLOAD_PACKET_SIZE],
    uint8_t length, uint8_t usb_packet[USB_FEATURE_UPLOAD_PACKET_SIZE]);

/**
 * @brief Accepts one USB motor vendor request through the mailbox.
 *
 * Applies USB request handling, transport ownership, mailbox reset/release, and motor-packet
 * queuing for the shared command transport.
 *
 * @param[in,out] service Active motor vendor service.
 * @param[in,out] exchange Mailbox exchange state used for motor packets.
 * @param[in,out] transport Shared command transport whose ownership is managed.
 * @param[in] request Received motor-command feature packet.
 * @param[in] length Number of bytes in request; it must equal USB_FEATURE_UPLOAD_PACKET_SIZE.
 * @param[out] usb_packet Buffer for an optional upload acknowledgement.
 * @return Actions and mailbox result produced by the request; a failed mailbox event when exchange
 * or transport is null.
 */
UsbMotorVendorServiceResult usb_motor_vendor_service_accept_usb_mailbox(
    UsbMotorVendorService *service, MotorCommandMailboxExchange *exchange,
    CommandTransport *transport, const uint8_t request[USB_FEATURE_UPLOAD_PACKET_SIZE],
    uint8_t length, uint8_t usb_packet[USB_FEATURE_UPLOAD_PACKET_SIZE]);

/**
 * @brief Accepts one packet from the motor command channel.
 *
 * Routes the packet through motor protocol handling and publishes any motor write or USB response
 * action generated by the channel.
 *
 * @param[in,out] service Active motor vendor service.
 * @param[in] packet Received motor command packet.
 * @param[in] length Number of bytes in packet.
 * @return Actions and packet data produced by the channel packet.
 */
UsbMotorVendorServiceResult usb_motor_vendor_service_accept_motor(UsbMotorVendorService *service,
                                                                  const uint8_t *packet,
                                                                  uint16_t length);

/**
 * @brief Runs one motor mailbox exchange for the USB vendor service.
 *
 * Polls and processes the mailbox while the USB motor owner holds the transport, then publishes any
 * motor write, USB response, status, or mailbox action produced by the exchange.
 *
 * @param[in,out] service Active motor vendor service.
 * @param[in,out] exchange Mailbox exchange state to run.
 * @param[in,out] transport Shared command transport owned by the USB motor service.
 * @return Actions and exchange state produced while the service owns the transport; no actions when
 * input is invalid or ownership is absent.
 */
UsbMotorVendorServiceResult
usb_motor_vendor_service_run_mailbox(UsbMotorVendorService *service,
                                     MotorCommandMailboxExchange *exchange,
                                     CommandTransport *transport);

/**
 * @brief Prepares a motor response USB packet without advancing it.
 *
 * Encodes a candidate response packet and restores the download cursor so the caller can discard
 * the candidate without consuming response progress.
 *
 * @param[in,out] service Active motor vendor service.
 * @param[out] packet Buffer for the candidate response packet.
 * @return Number of bytes prepared when a response is active and packet storage is valid; otherwise
 * zero.
 */
uint8_t usb_motor_vendor_service_prepare_response(UsbMotorVendorService *service,
                                                  uint8_t packet[USB_MOTOR_RESPONSE_PACKET_SIZE]);

/**
 * @brief Builds the next motor response USB packet.
 *
 * Advances response framing and clears the retained response after the terminal packet is produced.
 *
 * @param[in,out] service Active motor vendor service.
 * @param[out] packet Buffer for the next response packet.
 * @return Number of bytes produced when a response packet is available; otherwise zero.
 */
uint8_t usb_motor_vendor_service_next_response(UsbMotorVendorService *service,
                                               uint8_t packet[USB_MOTOR_RESPONSE_PACKET_SIZE]);

/**
 * @brief Acknowledges progress for the active motor response.
 *
 * Validates the acknowledgement against the active response download and releases it for the next
 * response packet when the transfer boundary matches.
 *
 * @param[in,out] service Active motor vendor service.
 * @param[in] acknowledgement Received response acknowledgement packet.
 * @return True when the acknowledgement matches active response progress; otherwise false.
 */
bool usb_motor_vendor_service_acknowledge_response(
    UsbMotorVendorService *service,
    const uint8_t acknowledgement[USB_MOTOR_RESPONSE_ACKNOWLEDGEMENT_SIZE]);

#endif
