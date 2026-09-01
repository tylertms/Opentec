#ifndef OPENTEC_BASE_USB_DEVICE_H
#define OPENTEC_BASE_USB_DEVICE_H

#include <stdbool.h>
#include <stdint.h>

#include "board/identity.h"
#include "usb/console_descriptor.h"
#include "usb/input_report.h"
#include "usb/playstation_authentication.h"
#include "usb/playstation_input.h"
#include "usb/xbox_gip_response.h"
#include "usb/xbox_gip_session.h"

/** @brief USB device report sizes and HID report type identifiers. */
enum {
    USB_DEVICE_REPORT_SIZE = 64,           /**< Maximum native HID report size in bytes. */
    USB_DEVICE_UPDATER_RESPONSE_SIZE = 66, /**< Maximum updater response size in bytes. */
    USB_DEVICE_HID_REPORT_INPUT = 1,       /**< HID input report type. */
    USB_DEVICE_HID_REPORT_OUTPUT = 2,      /**< HID output report type. */
    USB_DEVICE_HID_REPORT_FEATURE = 3,     /**< HID feature report type. */
};

/** @brief Primary USB operating modes exposed by the wheel-base device. */
typedef enum {
    USB_OPERATING_MODE_FANATEC = 0,               /**< Native Fanatec HID mode. */
    USB_OPERATING_MODE_FANATEC_COMPATIBILITY = 1, /**< Fanatec compatibility HID mode. */
    USB_OPERATING_MODE_DRIVING_FORCE_EX = 2,      /**< Driving Force EX compatibility mode. */
    USB_OPERATING_MODE_DRIVING_FORCE_PRO = 3,     /**< Driving Force Pro compatibility mode. */
    USB_OPERATING_MODE_G27 = 4,                   /**< G27 compatibility mode. */
    USB_OPERATING_MODE_UPDATER = 5,               /**< Motor updater CDC mode. */
    USB_OPERATING_MODE_XBOX_GIP = 6,              /**< Xbox GIP console mode. */
    USB_OPERATING_MODE_PLAYSTATION = 7,           /**< PlayStation console mode. */
} UsbOperatingMode;

/** @brief Host-to-device HID output report retained for application processing. */
typedef struct {
    uint8_t report_type;                  /**< HID report type. */
    uint8_t report_id;                    /**< HID report identifier. */
    uint8_t length;                       /**< Number of valid payload bytes. */
    uint8_t data[USB_DEVICE_REPORT_SIZE]; /**< Report payload bytes. */
} UsbDeviceOutputReport;

/** @brief One motor-updater packet retained from the host. */
typedef struct {
    uint8_t length;                       /**< Number of valid packet bytes. */
    uint8_t data[USB_DEVICE_REPORT_SIZE]; /**< Packet payload bytes. */
} UsbDeviceUpdaterPacket;

/**
 * @brief Initializes the wheel-base USB device.
 *
 * Prepares the selected board variant, descriptor state, endpoint state, and USB controller for
 * host enumeration.
 *
 * @param[in] variant Wheel-base hardware variant.
 */
void usb_device_init(BoardVariant variant);

/**
 * @brief Prepares the wheel-base USB device.
 *
 * Builds the native Fanatec descriptor profile, initializes console service data, resets transfer
 * state, and prepares the USB controller without attaching it to the host.
 *
 * @param[in] variant Wheel-base hardware variant.
 */
void usb_device_prepare(BoardVariant variant);

/**
 * @brief Prepares the detached wheel-base updater USB device.
 *
 * Builds the EBLDC updater descriptor profile, initializes console service data, resets transfer
 * state, and prepares the USB controller without attaching it to the host.
 *
 * @param[in] variant Wheel-base hardware variant.
 * @return True when the updater descriptor profile was prepared; otherwise false.
 */
bool usb_device_prepare_updater(BoardVariant variant);

/**
 * @brief Selects the primary USB input-report operating mode.
 *
 * Builds the descriptor profile and selects a native or compatibility input-report mode.
 *
 * @param[in] mode Primary USB input-report mode.
 * @return True when the mode has a complete descriptor profile; otherwise false.
 */
bool usb_device_set_input_mode(UsbInputReportMode mode);

/**
 * @brief Returns the active primary USB input-report operating mode.
 *
 * Reports the native or compatibility input mode currently selected by the device.
 *
 * @return Active primary USB input-report mode.
 */
UsbInputReportMode usb_device_input_mode(void);

/**
 * @brief Selects the active USB operating mode.
 *
 * Builds and applies the descriptor profile for the requested native, compatibility, updater, or
 * console mode.
 *
 * @param[in] mode USB operating-mode selector.
 * @return True when the mode has a complete transport profile; otherwise false.
 */
bool usb_device_set_operating_mode(UsbOperatingMode mode);

/**
 * @brief Selects Xbox GIP mode for an attached wheel.
 *
 * Stores the wheel digest, resolves the Xbox product identity, and activates the Xbox descriptor
 * profile when the selected wheel mode is supported.
 *
 * @param[in] wheel_mode Attached-wheel operating-mode selector.
 * @param[in] digest Eight-byte attached-wheel status digest.
 * @return True when the wheel mode has a supported Xbox identity; otherwise false.
 */
bool usb_device_set_xbox_mode(uint8_t wheel_mode, const uint8_t digest[USB_XBOX_GIP_DIGEST_SIZE]);

/**
 * @brief Selects the default PlayStation USB mode.
 *
 * Activates PlayStation mode using the default wheel mode.
 *
 * @return True when the PlayStation profile was activated; otherwise false.
 */
bool usb_device_set_playstation_mode(void);

/**
 * @brief Selects PlayStation USB mode for a specific base mode.
 *
 * Stores the selected wheel mode and activates the corresponding PlayStation descriptor profile.
 *
 * @param[in] wheel_mode Selected PlayStation base mode.
 * @return True when the selected PlayStation profile was activated; otherwise false.
 */
bool usb_device_set_playstation_wheel_mode(uint8_t wheel_mode);

/**
 * @brief Returns the active USB operating mode.
 *
 * Reports the descriptor and transport mode selected by the device.
 *
 * @return Active USB operating mode.
 */
UsbOperatingMode usb_device_operating_mode(void);

/**
 * @brief Services pending USB controller and mode-specific transfers.
 *
 * Processes controller events and advances endpoint transfers for the active operating mode.
 */
void usb_device_service(void);

/**
 * @brief Reports whether the USB device has an active configuration.
 *
 * Checks the endpoint-zero configuration state.
 *
 * @return True when the device is configured; otherwise false.
 */
bool usb_device_configured(void);

/**
 * @brief Takes one pending host output report.
 *
 * Transfers the pending HID output report to the caller.
 *
 * @param[out] report Destination for report type, identifier, payload, and length.
 * @return True when a pending report was returned; otherwise false.
 */
bool usb_device_take_output(UsbDeviceOutputReport *report);

/**
 * @brief Publishes one native USB feature-report snapshot.
 *
 * Retains a supported feature report for a later host request.
 *
 * @param[in] report_id Feature report identifier.
 * @param[in] report Encoded feature-report bytes.
 * @param[in] length Number of encoded bytes.
 * @return True when the report identifier, pointer, and length are supported; otherwise false.
 */
bool usb_device_publish_feature_report(uint8_t report_id, const uint8_t *report, uint8_t length);

/**
 * @brief Takes a completed host request for a native feature report.
 *
 * Clears the one-shot request latch for the matching supported report identifier.
 *
 * @param[in] report_id Feature report identifier to inspect.
 * @return True when a matching request was pending; otherwise false.
 */
bool usb_device_take_feature_report_request(uint8_t report_id);

/**
 * @brief Sends a primary native or compatibility HID input report.
 *
 * Suppresses an unchanged report and submits a changed payload through the primary endpoint.
 *
 * @param[in] report Encoded HID input report.
 * @param[in] length Number of report bytes from one through 64.
 * @return True when the report was unchanged or accepted by the active endpoint; otherwise false.
 */
bool usb_device_send_input(const uint8_t *report, uint8_t length);

/**
 * @brief Queues the current Xbox GIP controller state.
 *
 * Encodes one state packet with the next shared sequence value for endpoint transmission.
 *
 * @param[in] snapshot Current logical Xbox controller state.
 * @return True when the state packet was queued; otherwise false.
 */
bool usb_device_queue_xbox_input(const UsbXboxGipInputSnapshot *snapshot);

/**
 * @brief Queues the Xbox GIP wheel capability response.
 *
 * Encodes the fixed capability packet with the next shared sequence.
 *
 * @return True when the capability response was queued; otherwise false.
 */
bool usb_device_queue_xbox_capabilities(void);

/**
 * @brief Queues the Xbox GIP attached-device status response.
 *
 * Encodes the current base and attached-device state with the next shared sequence.
 *
 * @param[in] status Current logical attached-device status.
 * @return True when the extended-status response was queued; otherwise false.
 */
bool usb_device_queue_xbox_extended_status(const UsbXboxGipExtendedStatus *status);

/**
 * @brief Queues an Xbox GIP command transfer-status response.
 *
 * Echoes the triggering packet type and group with the current shared sequence.
 *
 * @param[in] request First two bytes of the triggering command packet.
 * @return True when the transfer-status response was queued; otherwise false.
 */
bool usb_device_queue_xbox_transfer_status(const uint8_t request[2]);

/**
 * @brief Queues one Xbox GIP application response.
 *
 * Copies the response, replaces its envelope sequence, and retains it for endpoint transmission.
 *
 * @param[in] report Complete application response packet.
 * @param[in] length Number of response bytes from three through 64.
 * @return True when the response was queued; otherwise false.
 */
bool usb_device_queue_xbox_response(const uint8_t *report, uint8_t length);

/**
 * @brief Queues one raw Xbox vendor report.
 *
 * Copies all 64 report bytes without changing its protocol fields.
 *
 * @param[in] report Complete raw Xbox vendor report.
 * @return True when the vendor report was queued; otherwise false.
 */
bool usb_device_queue_xbox_vendor_report(const uint8_t report[USB_DEVICE_REPORT_SIZE]);

/**
 * @brief Sends one native vendor HID report.
 *
 * Submits the requested bytes to the primary endpoint without suppressing repeated responses.
 *
 * @param[in] report Native vendor response bytes.
 * @param[in] length Number of response bytes from one through 64.
 * @return True when the configured native HID endpoint accepted the transfer; otherwise false.
 */
bool usb_device_send_vendor_report(const uint8_t *report, uint8_t length);

/**
 * @brief Takes one received motor-updater packet.
 *
 * Transfers ownership of the oldest retained packet to the caller.
 *
 * @param[out] packet Destination for packet bytes and length.
 * @return True when a packet was available; otherwise false.
 */
bool usb_device_take_updater_packet(UsbDeviceUpdaterPacket *packet);

/**
 * @brief Reports whether the motor-updater input stream can accept a response.
 *
 * Checks that no response is retained and no updater input transfer is active.
 *
 * @return True when the updater stream is idle; otherwise false.
 */
bool usb_device_updater_channel_idle(void);

/**
 * @brief Queues one complete motor-updater response.
 *
 * Retains the response while the endpoint service emits bulk packets and any required terminator.
 *
 * @param[in] data Complete response bytes to retain.
 * @param[in] length Number of response bytes from one through 66.
 * @return True when the idle updater stream accepted the response; otherwise false.
 */
bool usb_device_queue_updater_response(const uint8_t *data, uint8_t length);

/**
 * @brief Takes pending Xbox GIP session actions.
 *
 * Returns all accepted session actions accumulated since the previous take and clears the action
 * set.
 *
 * @return Pending Xbox GIP session actions.
 */
UsbXboxGipSessionAction usb_device_take_xbox_session_actions(void);

/**
 * @brief Takes a completed PlayStation authentication request.
 *
 * Copies the assembled host challenge for processing by the secure-element service.
 *
 * @param[out] request Completed authentication challenge.
 * @return True when a completed request was available; otherwise false.
 */
bool usb_device_take_playstation_authentication_request(
    uint8_t request[USB_PLAYSTATION_AUTHENTICATION_REQUEST_SIZE]);

/**
 * @brief Publishes a PlayStation authentication response.
 *
 * Makes the secure-element response available through the PlayStation authentication feature path.
 *
 * @param[in] response Complete authentication response.
 * @param[in] response_length Number of response bytes.
 * @return True when the response was accepted in PlayStation mode; otherwise false.
 */
bool usb_device_publish_playstation_authentication_response(const uint8_t *response,
                                                            uint16_t response_length);

/**
 * @brief Reports whether PlayStation response fragments remain available.
 *
 * Reports true while the host can still consume an authentication response feature report.
 *
 * @return True while a PlayStation response is being retrieved; otherwise false.
 */
bool usb_device_playstation_authentication_response_active(void);

/**
 * @brief Reports a PlayStation authentication failure.
 *
 * Changes the authentication status feature report to the response-error state while PlayStation
 * mode is active.
 */
void usb_device_fail_playstation_authentication(void);

/**
 * @brief Publishes one PlayStation remote-tuning feature report.
 *
 * Retains a report-35 payload while PlayStation mode is active and the response slot is empty.
 *
 * @param[in] report Complete 64-byte report-35 payload.
 * @return True when the response slot accepted the report; otherwise false.
 */
bool usb_device_publish_playstation_remote_tuning_report(
    const uint8_t report[USB_DEVICE_REPORT_SIZE]);

/**
 * @brief Encodes and sends the current PlayStation input state.
 *
 * Suppresses an unchanged report and submits a changed report through the PlayStation input
 * endpoint.
 *
 * @param[in] state Current logical PlayStation controls and axes.
 * @return True when the report was unchanged or accepted by the endpoint; otherwise false.
 */
bool usb_device_send_playstation_input(const UsbPlaystationInputState *state);

#endif
