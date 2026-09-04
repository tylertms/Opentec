#ifndef OPENTEC_BASE_USB_UPDATER_SERVICE_H
#define OPENTEC_BASE_USB_UPDATER_SERVICE_H

#include <stdbool.h>
#include <stdint.h>

#include "board/identity.h"
#include "system/runtime_bridge.h"
#include "transfer/command.h"
#include "usb/device.h"
#include "usb/operating_mode_command.h"
#include "usb/updater_identity.h"
#include "usb/updater_protocol.h"
#include "wheel/updater_aux_service.h"
#include "wheel/updater_command_service.h"
#include "wheel/updater_direct_service.h"

/** @brief Result of the updater route probe used during a runtime transition. */
typedef enum {
    USB_UPDATER_PROBE_IDLE,     /**< No route probe is active. */
    USB_UPDATER_PROBE_PENDING,  /**< The route probe is in progress. */
    USB_UPDATER_PROBE_COMPLETE, /**< The route probe returned the expected response. */
    USB_UPDATER_PROBE_FAILED,   /**< The route probe ended without the expected response. */
} UsbUpdaterProbeStatus;

/** @brief Live inputs used to service updater USB requests. */
typedef struct {
    uint32_t now_ms;            /**< Current monotonic time in milliseconds. */
    BoardVariant board_variant; /**< Base hardware variant. */
    uint8_t wheel_mode;         /**< Attached-wheel protocol mode. */
    bool adapter_connected;     /**< Whether the attached wheel is connected through an adapter. */
} UsbUpdaterServiceInput;

/** @brief Mutually exclusive updater transport selected by the runtime route. */
typedef union {
    WheelUpdaterAuxService auxiliary;   /**< Auxiliary-bus updater transport. */
    WheelUpdaterCommandService command; /**< Shared-command updater transport. */
    WheelUpdaterDirectService direct;   /**< Raw attached-wheel updater transport. */
} UsbUpdaterTransport;

/** @brief Updater USB session, route, probe, and retained response state. */
typedef struct {
    UsbUpdaterTransport route;           /**< Selected updater transport state. */
    CommandTransport *command_transport; /**< Shared command transport for command routes. */
    const uint8_t *route_response; /**< Latest response buffer supplied by the selected route. */
    UsbDeviceUpdaterPacket host_packet;     /**< Most recently received updater USB packet. */
    UsbUpdaterRequest host_request;         /**< Decoded request corresponding to #host_packet. */
    UsbUpdaterIdentityInput identity_input; /**< Inputs used for the latest identity response. */
    uint8_t pending_response[WHEEL_UPDATER_BRIDGE_MAX_RESPONSE_SIZE]; /**< Response awaiting USB
                                                                         transmission. */
    uint8_t identity[USB_UPDATER_DEVICE_IDENTITY_SIZE]; /**< Encoded four-byte updater identity. */
    uint32_t usb_deadline_ms;                           /**< Next updater USB service deadline. */
    UsbRuntimeMode runtime_mode;        /**< Currently selected updater runtime route. */
    UsbUpdaterProbeStatus probe_status; /**< Current route probe result. */
    uint8_t response_selector;          /**< Identity selector learned from the route probe. */
    uint8_t route_response_length;      /**< Number of bytes in #route_response. */
    uint8_t pending_response_length;    /**< Number of bytes in #pending_response. */
    bool exchange_is_probe; /**< Whether the active route exchange is the discovery probe. */
    bool usb_active;        /**< Whether updater USB requests are enabled. */
    bool reset_requested;   /**< Whether a guarded host reset request is latched. */
} UsbUpdaterService;

/**
 * @brief Initializes updater USB session state.
 *
 * Attaches the shared command transport, selects automatic identity response behavior, and leaves
 * the route and USB service inactive.
 *
 * @param[out] service Updater service to initialize.
 * @param[in] transport Shared command transport used by non-direct routes.
 */
void usb_updater_service_init(UsbUpdaterService *service, CommandTransport *transport);

/**
 * @brief Selects and initializes an updater transport route.
 *
 * Accepts runtime modes one through six while idle, clears prior probe and pending-response state,
 * and initializes the auxiliary bus, raw UART, or matching shared command adapter. Internal
 * protocol-recovery mode six uses the USB updater command target.
 *
 * @param[in,out] service Idle updater service selecting a route.
 * @param[in] mode Requested updater runtime mode.
 * @return `true` when the route was selected; otherwise `false`.
 */
bool usb_updater_service_select_mode(UsbUpdaterService *service, UsbRuntimeMode mode);

/**
 * @brief Starts an updater-backed runtime bridge transition atomically.
 *
 * Stages the runtime bridge transition before selecting its updater route. A busy bridge or a
 * route that cannot be selected leaves both state objects and the action result unchanged.
 *
 * @param[in,out] service Updater service receiving the selected route.
 * @param[in,out] bridge Runtime bridge accepting the transition.
 * @param[in] mode Requested updater runtime mode.
 * @param[out] actions Initial runtime bridge actions for the owning services.
 * @return `true` when both the route and transition were accepted; otherwise `false`.
 */
bool usb_updater_service_start_runtime_bridge(UsbUpdaterService *service, RuntimeBridge *bridge,
                                              UsbRuntimeMode mode, uint16_t *actions);

/**
 * @brief Selects auxiliary updater recovery after startup discovery fails.
 *
 * Initializes auxiliary recovery mode and satisfies its normal-controller shutdown prerequisite
 * because the startup discovery window already established that the normal endpoint is unavailable.
 *
 * @param[in,out] service Idle updater service selecting startup recovery.
 * @return `true` when the auxiliary recovery route was prepared; otherwise `false`.
 */
bool usb_updater_service_select_startup_recovery(UsbUpdaterService *service);

/**
 * @brief Requests the prerequisite auxiliary shutdown handshake.
 *
 * Forwards the request only while an auxiliary updater route is selected.
 *
 * @param[in,out] service Configured updater service receiving the request.
 */
void usb_updater_service_request_auxiliary_handshake(UsbUpdaterService *service);

/**
 * @brief Reports whether the selected auxiliary route completed its shutdown handshake.
 *
 * Rejects non-auxiliary and null services without inspecting inactive union storage.
 *
 * @param[in] service Configured updater service to inspect.
 * @return `true` after the auxiliary handshake succeeds; otherwise `false`.
 */
bool usb_updater_service_auxiliary_handshake_complete(const UsbUpdaterService *service);

/**
 * @brief Starts the updater route discovery probe.
 *
 * Sends the two-byte 0x5A/0xA6 request used before updater USB activation and reports the probe
 * pending until its ten-byte 0x5A/0xA7 response completes or the route rejects it.
 *
 * @param[in,out] service Idle configured updater route.
 * @return `true` when the probe started; otherwise `false`.
 */
bool usb_updater_service_start_probe(UsbUpdaterService *service);

/**
 * @brief Enables or disables updater USB request service.
 *
 * Applies the runtime transition's updater USB activation state without changing its selected
 * route, probe result, or response selector.
 *
 * @param[in,out] service Configured updater service.
 * @param[in] active `true` to accept updater USB requests; `false` to stop accepting them.
 */
void usb_updater_service_set_usb_active(UsbUpdaterService *service, bool active);

/**
 * @brief Advances updater transport and USB request service.
 *
 * Services an active route on every call. After USB activation, polls only after each strict
 * ten-millisecond deadline, waits for both the updater input stream and route to become idle,
 * publishes retained responses, and accepts one supported host request.
 *
 * @param[in,out] service Configured updater service to advance.
 * @param[in] input Current time, board variant, wheel mode, and adapter state.
 */
void usb_updater_service_run(UsbUpdaterService *service, const UsbUpdaterServiceInput *input);

/**
 * @brief Returns the current updater route probe result.
 *
 * Exposes idle, pending, complete, or failed state without consuming it.
 *
 * @param[in] service Updater service to inspect.
 * @return Current probe result, or idle for a null service.
 */
UsbUpdaterProbeStatus usb_updater_service_probe_status(const UsbUpdaterService *service);

/**
 * @brief Takes a guarded updater USB reset request.
 *
 * Clears the one-shot reset latch after exposing it to the runtime owner.
 *
 * @param[in,out] service Updater service holding the reset latch.
 * @return `true` once for each accepted F8/09/01/FE request; otherwise `false`.
 */
bool usb_updater_service_take_reset(UsbUpdaterService *service);

#endif
