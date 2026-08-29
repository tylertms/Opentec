#ifndef OPENTEC_BASE_USB_MOTOR_VENDOR_SERVICE_H
#define OPENTEC_BASE_USB_MOTOR_VENDOR_SERVICE_H

#include <stdbool.h>
#include <stdint.h>

#include "motor/command_application.h"
#include "motor/command_mailbox.h"
#include "motor/command_receiver.h"
#include "usb/motor_command_upload.h"
#include "usb/motor_response_download.h"

typedef enum {
    USB_MOTOR_VENDOR_ACTION_NONE = 0,
    USB_MOTOR_VENDOR_ACTION_CLAIM = 1 << 0,
    USB_MOTOR_VENDOR_ACTION_RELEASE = 1 << 1,
    USB_MOTOR_VENDOR_ACTION_RESTART = 1 << 2,
    USB_MOTOR_VENDOR_ACTION_WRITE_MOTOR = 1 << 3,
    USB_MOTOR_VENDOR_ACTION_WRITE_USB = 1 << 4,
    USB_MOTOR_VENDOR_ACTION_RESPONSE_READY = 1 << 5,
} UsbMotorVendorAction;

typedef struct {
    uint8_t *upload_assembly;
    uint16_t upload_assembly_capacity;
    uint8_t *receive_assembly;
    uint16_t receive_assembly_capacity;
    uint8_t *motor_transmit;
    uint16_t motor_transmit_capacity;
    uint8_t *application_data;
    uint16_t application_data_capacity;
} UsbMotorVendorServiceBuffers;

typedef struct {
    UsbMotorCommandUpload upload;
    UsbMotorResponseDownload download;
    MotorCommandReceiver receiver;
    MotorCommandApplication application;
    MotorCommandMessage message;
    UsbMotorVendorServiceBuffers buffers;
    uint16_t motor_transmit_length;
    uint16_t pending_payload_length;
    uint16_t response_length;
    uint8_t usb_sequence;
    bool command_pending;
    bool response_active;
} UsbMotorVendorService;

typedef struct {
    UsbMotorVendorAction actions;
    const uint8_t *motor_packet;
    uint16_t motor_packet_length;
    uint32_t motor_status;
    uint8_t usb_packet_length;
    MotorCommandMailboxExchangeEvent mailbox_event;
} UsbMotorVendorServiceResult;

bool usb_motor_vendor_service_init(UsbMotorVendorService *service,
                                   const UsbMotorVendorServiceBuffers *buffers);
UsbMotorVendorServiceResult usb_motor_vendor_service_accept_usb(
    UsbMotorVendorService *service, const uint8_t request[USB_FEATURE_UPLOAD_PACKET_SIZE],
    uint8_t length, uint8_t usb_packet[USB_FEATURE_UPLOAD_PACKET_SIZE]);
UsbMotorVendorServiceResult usb_motor_vendor_service_accept_usb_mailbox(
    UsbMotorVendorService *service, MotorCommandMailboxExchange *exchange,
    CommandTransport *transport, const uint8_t request[USB_FEATURE_UPLOAD_PACKET_SIZE],
    uint8_t length, uint8_t usb_packet[USB_FEATURE_UPLOAD_PACKET_SIZE]);
UsbMotorVendorServiceResult usb_motor_vendor_service_accept_motor(UsbMotorVendorService *service,
                                                                  const uint8_t *packet,
                                                                  uint16_t length);
UsbMotorVendorServiceResult
usb_motor_vendor_service_run_mailbox(UsbMotorVendorService *service,
                                     MotorCommandMailboxExchange *exchange,
                                     CommandTransport *transport);
uint8_t usb_motor_vendor_service_prepare_response(UsbMotorVendorService *service,
                                                  uint8_t packet[USB_MOTOR_RESPONSE_PACKET_SIZE]);
uint8_t usb_motor_vendor_service_next_response(UsbMotorVendorService *service,
                                               uint8_t packet[USB_MOTOR_RESPONSE_PACKET_SIZE]);
bool usb_motor_vendor_service_acknowledge_response(
    UsbMotorVendorService *service,
    const uint8_t acknowledgement[USB_MOTOR_RESPONSE_ACKNOWLEDGEMENT_SIZE]);

#endif
