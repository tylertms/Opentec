#ifndef OPENTEC_BASE_USB_UPDATER_SERVICE_H
#define OPENTEC_BASE_USB_UPDATER_SERVICE_H

#include <stdbool.h>
#include <stdint.h>

#include "board/identity.h"
#include "transfer/command.h"
#include "usb/device.h"
#include "usb/operating_mode_command.h"
#include "usb/updater_identity.h"
#include "usb/updater_protocol.h"
#include "wheel/updater_command_service.h"
#include "wheel/updater_direct_service.h"

/** @brief Result of the updater route probe used during a runtime transition. */
typedef enum {
    USB_UPDATER_PROBE_IDLE,
    USB_UPDATER_PROBE_PENDING,
    USB_UPDATER_PROBE_COMPLETE,
    USB_UPDATER_PROBE_FAILED,
} UsbUpdaterProbeStatus;

/** @brief Live inputs used to service updater USB requests. */
typedef struct {
    uint32_t now_ms;
    BoardVariant board_variant;
    uint8_t wheel_mode;
    bool adapter_connected;
} UsbUpdaterServiceInput;

/** @brief Mutually exclusive updater transport selected by the runtime route. */
typedef union {
    WheelUpdaterCommandService command;
    WheelUpdaterDirectService direct;
} UsbUpdaterTransport;

/** @brief Updater USB session, route, probe, and retained response state. */
typedef struct {
    UsbUpdaterTransport route;
    CommandTransport *command_transport;
    const uint8_t *route_response;
    UsbDeviceUpdaterPacket host_packet;
    UsbUpdaterRequest host_request;
    UsbUpdaterIdentityInput identity_input;
    uint8_t pending_response[WHEEL_UPDATER_BRIDGE_MAX_RESPONSE_SIZE];
    uint8_t identity[USB_UPDATER_DEVICE_IDENTITY_SIZE];
    uint32_t usb_deadline_ms;
    UsbRuntimeMode runtime_mode;
    UsbUpdaterProbeStatus probe_status;
    uint8_t response_selector;
    uint8_t route_response_length;
    uint8_t pending_response_length;
    bool exchange_is_probe;
    bool usb_active;
    bool reset_requested;
} UsbUpdaterService;

void usb_updater_service_init(UsbUpdaterService *service, CommandTransport *transport);
bool usb_updater_service_select_mode(UsbUpdaterService *service, UsbRuntimeMode mode);
bool usb_updater_service_start_probe(UsbUpdaterService *service);
void usb_updater_service_set_usb_active(UsbUpdaterService *service, bool active);
void usb_updater_service_run(UsbUpdaterService *service, const UsbUpdaterServiceInput *input);
UsbUpdaterProbeStatus usb_updater_service_probe_status(const UsbUpdaterService *service);
bool usb_updater_service_take_reset(UsbUpdaterService *service);

#endif
