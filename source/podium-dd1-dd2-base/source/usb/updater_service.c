#include "usb/updater_service.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "transfer/command.h"
#include "usb/device.h"
#include "usb/operating_mode_command.h"
#include "usb/updater_identity.h"
#include "usb/updater_protocol.h"
#include "wheel/updater_aux_service.h"
#include "wheel/updater_command_service.h"
#include "wheel/updater_direct_service.h"

enum {
    USB_UPDATER_SERVICE_INTERVAL_MS = 10,
    USB_UPDATER_PROBE_SIZE = 2,
    USB_UPDATER_PROBE_RESPONSE_SIZE = 10,
    USB_UPDATER_PROBE_COMMAND_OFFSET = 2,
};

static const uint8_t probe[USB_UPDATER_PROBE_SIZE] = {0x5a, 0xa7};

/**
 * @brief Reports whether a runtime mode uses the auxiliary bus.
 *
 * Selects the direct auxiliary updater endpoint for standard and recovery auxiliary modes.
 *
 * @param[in] mode Runtime updater route.
 * @return True for either auxiliary mode; otherwise false.
 */
static bool auxiliary_route(UsbRuntimeMode mode) {
    return mode == USB_RUNTIME_MODE_AUXILIARY || mode == USB_RUNTIME_MODE_AUXILIARY_RECOVERY;
}

/**
 * @brief Reports whether a runtime mode uses the raw attached-wheel link.
 *
 * Selects raw UART transport only for status bridge mode.
 *
 * @param[in] mode Runtime updater route.
 * @return True for the raw status bridge; otherwise false.
 */
static bool direct_route(UsbRuntimeMode mode) { return mode == USB_RUNTIME_MODE_STATUS_BRIDGE; }

/**
 * @brief Selects the shared command target for an updater runtime mode.
 *
 * Routes protocol bridge mode to target 0x12 and USB bridge and protocol recovery modes to target
 * 0x11.
 *
 * @param[in] mode Runtime updater route.
 * @return Shared command target for the selected route.
 */
static WheelUpdaterTarget command_target(UsbRuntimeMode mode) {
    return mode == USB_RUNTIME_MODE_PROTOCOL_BRIDGE ? WHEEL_UPDATER_TARGET_PROTOCOL
                                                    : WHEEL_UPDATER_TARGET_USB;
}

/**
 * @brief Reports whether an updater exchange owns its selected transport.
 *
 * Queries only the route initialized for the active runtime mode.
 *
 * @param[in] service Updater session to inspect.
 * @return True while its transport owns an exchange; otherwise false.
 */
static bool exchange_active(const UsbUpdaterService *service) {
    if (auxiliary_route(service->runtime_mode)) {
        return wheel_updater_aux_service_active(&service->route.auxiliary);
    }
    if (direct_route(service->runtime_mode)) {
        return wheel_updater_direct_service_active(&service->route.direct);
    }
    return wheel_updater_command_service_active(&service->route.command);
}

/**
 * @brief Starts one request on the selected updater route.
 *
 * Dispatches to the auxiliary bus, raw UART, or mode-specific shared command target.
 *
 * @param[in,out] service Idle updater session accepting the request.
 * @param[in] request Marker-prefixed updater request.
 * @param[in] length Request byte count.
 * @return True when the selected route accepted the request; otherwise false.
 */
static bool start_exchange(UsbUpdaterService *service, const uint8_t *request, uint8_t length) {
    if (auxiliary_route(service->runtime_mode)) {
        return wheel_updater_aux_service_start(&service->route.auxiliary, request, length);
    }
    if (direct_route(service->runtime_mode)) {
        return wheel_updater_direct_service_start(&service->route.direct, request, length);
    }
    return wheel_updater_command_service_start(
        &service->route.command, command_target(service->runtime_mode), request, length);
}

/**
 * @brief Advances the selected updater transport.
 *
 * Services only the auxiliary-bus, raw, or shared-command adapter initialized for the route.
 *
 * @param[in,out] service Active updater session.
 * @param[in] now_ms Current monotonic time in milliseconds.
 */
static void run_exchange(UsbUpdaterService *service, uint32_t now_ms) {
    if (auxiliary_route(service->runtime_mode)) {
        wheel_updater_aux_service_run(&service->route.auxiliary, now_ms);
    } else if (direct_route(service->runtime_mode)) {
        wheel_updater_direct_service_run(&service->route.direct, now_ms);
    } else {
        wheel_updater_command_service_run(&service->route.command, now_ms);
    }
}

/**
 * @brief Takes a complete response from the selected updater transport.
 *
 * Queries only the transport adapter initialized for the current runtime route.
 *
 * @param[in,out] service Updater session holding a route response.
 * @param[out] response Complete response bytes.
 * @param[out] length Complete response length.
 * @return True when the route produced a response; otherwise false.
 */
static bool take_exchange_response(UsbUpdaterService *service, const uint8_t **response,
                                   uint8_t *length) {
    if (auxiliary_route(service->runtime_mode)) {
        return wheel_updater_aux_service_take_response(&service->route.auxiliary, response, length);
    }
    if (direct_route(service->runtime_mode)) {
        return wheel_updater_direct_service_take_response(&service->route.direct, response, length);
    }
    return wheel_updater_command_service_take_response(&service->route.command, response, length);
}

/**
 * @brief Completes a probe or retains a host-request response.
 *
 * Accepts the ten-byte 0x5A/0xA7 probe response, updates the later identity selector from its
 * command byte, and retains ordinary bridge responses until the updater USB input stream is idle.
 *
 * @param[in,out] service Updater session completing an exchange.
 * @param[in] response Complete route response.
 * @param[in] length Complete response length.
 */
static void finish_exchange(UsbUpdaterService *service, const uint8_t *response, uint8_t length) {
    bool valid_probe = length == USB_UPDATER_PROBE_RESPONSE_SIZE && response[0] == probe[0] &&
                       response[1] == probe[1];
    if (valid_probe) {
        service->response_selector = usb_updater_identity_selector(
            service->runtime_mode, response[USB_UPDATER_PROBE_COMMAND_OFFSET]);
    }
    if (service->exchange_is_probe) {
        service->exchange_is_probe = false;
        service->probe_status = valid_probe ? USB_UPDATER_PROBE_COMPLETE : USB_UPDATER_PROBE_FAILED;
        return;
    }
    memcpy(service->pending_response, response, length);
    service->pending_response_length = length;
}

/**
 * @brief Services an active route and collects its terminal result.
 *
 * Advances one transport iteration, consumes a complete response, or marks a probe failed when its
 * route terminates without the required response.
 *
 * @param[in,out] service Updater session to advance.
 * @param[in] now_ms Current monotonic time in milliseconds.
 */
static void service_exchange(UsbUpdaterService *service, uint32_t now_ms) {
    if (!exchange_active(service)) {
        return;
    }
    run_exchange(service, now_ms);
    if (take_exchange_response(service, &service->route_response,
                               &service->route_response_length)) {
        finish_exchange(service, service->route_response, service->route_response_length);
    } else if (service->exchange_is_probe && !exchange_active(service)) {
        service->exchange_is_probe = false;
        service->probe_status = USB_UPDATER_PROBE_FAILED;
    }
}

/**
 * @brief Reports whether the updater USB polling deadline has passed.
 *
 * Implements the strict greater-than deadline gate used by the ten-millisecond updater service.
 *
 * @param[in] now_ms Current monotonic time in milliseconds.
 * @param[in] deadline_ms Recorded service deadline.
 * @return True only after the deadline; otherwise false.
 */
static bool usb_service_due(uint32_t now_ms, uint32_t deadline_ms) {
    return (int32_t)(now_ms - deadline_ms) > 0;
}

/**
 * @brief Handles one decoded updater USB request.
 *
 * Starts bridge requests on the selected route, prepares a six-byte device-information response,
 * or latches the guarded reset request for the runtime owner.
 *
 * @param[in,out] service Active updater USB session.
 * @param[in] input Current runtime identity inputs.
 * @param[in] request Decoded updater request.
 */
static void handle_request(UsbUpdaterService *service, const UsbUpdaterServiceInput *input,
                           const UsbUpdaterRequest *request) {
    if (request->kind == USB_UPDATER_REQUEST_BRIDGE) {
        service->exchange_is_probe = false;
        (void)start_exchange(service, request->data, request->length);
        return;
    }
    if (request->kind == USB_UPDATER_REQUEST_RESET) {
        service->reset_requested = true;
        return;
    }

    service->identity_input = (UsbUpdaterIdentityInput){
        .runtime_mode = service->runtime_mode,
        .board_variant = input->board_variant,
        .wheel_mode = input->wheel_mode,
        .response_selector = service->response_selector,
        .adapter_connected = input->adapter_connected,
    };
    usb_updater_identity_select(&service->identity_input, service->identity);
    usb_updater_protocol_encode_device_info(service->identity, service->pending_response);
    service->pending_response_length = USB_UPDATER_DEVICE_INFO_RESPONSE_SIZE;
}

/**
 * @brief Initializes updater USB session state.
 *
 * Attaches the shared command transport, selects automatic identity response behavior, and leaves
 * route and USB service inactive.
 *
 * @param[out] service Updater service to initialize.
 * @param[in,out] transport Shared command transport used by non-direct routes.
 */
void usb_updater_service_init(UsbUpdaterService *service, CommandTransport *transport) {
    if (service == NULL) {
        return;
    }
    *service = (UsbUpdaterService){
        .command_transport = transport,
        .response_selector = USB_UPDATER_IDENTITY_AUTOMATIC,
    };
}

/**
 * @brief Selects and initializes an updater transport route.
 *
 * Accepts runtime modes one through six while idle, clears prior probe and host-response state,
 * and initializes the auxiliary bus, raw UART, or matching shared command adapter.
 *
 * @param[in,out] service Idle updater service selecting a route.
 * @param[in] mode Requested updater runtime mode.
 * @return True when the route was selected; otherwise false.
 */
bool usb_updater_service_select_mode(UsbUpdaterService *service, UsbRuntimeMode mode) {
    if (service == NULL || mode < USB_RUNTIME_MODE_AUXILIARY ||
        mode > USB_RUNTIME_MODE_PROTOCOL_RECOVERY || exchange_active(service) ||
        (!auxiliary_route(mode) && !direct_route(mode) && service->command_transport == NULL)) {
        return false;
    }
    service->runtime_mode = mode;
    service->probe_status = USB_UPDATER_PROBE_IDLE;
    service->response_selector = USB_UPDATER_IDENTITY_AUTOMATIC;
    service->pending_response_length = 0;
    service->exchange_is_probe = false;
    service->usb_active = false;
    service->reset_requested = false;
    if (auxiliary_route(mode)) {
        wheel_updater_aux_service_init(&service->route.auxiliary);
    } else if (direct_route(mode)) {
        wheel_updater_direct_service_init(&service->route.direct);
    } else {
        wheel_updater_command_service_init(&service->route.command, service->command_transport);
    }
    return true;
}

/**
 * @brief Requests the prerequisite auxiliary shutdown handshake.
 *
 * Forwards the request only while an auxiliary updater route is selected.
 *
 * @param[in,out] service Configured updater service receiving the request.
 */
void usb_updater_service_request_auxiliary_handshake(UsbUpdaterService *service) {
    if (service != NULL && auxiliary_route(service->runtime_mode)) {
        wheel_updater_aux_service_request_handshake(&service->route.auxiliary);
    }
}

/**
 * @brief Reports whether the selected auxiliary route completed its shutdown handshake.
 *
 * Rejects non-auxiliary and null services without inspecting inactive union storage.
 *
 * @param[in] service Configured updater service to inspect.
 * @return True after the auxiliary handshake succeeds; otherwise false.
 */
bool usb_updater_service_auxiliary_handshake_complete(const UsbUpdaterService *service) {
    return service != NULL && auxiliary_route(service->runtime_mode) &&
           wheel_updater_aux_service_handshake_complete(&service->route.auxiliary);
}

/**
 * @brief Starts the updater route discovery probe.
 *
 * Sends the two-byte 0x5A/0xA7 request used before updater USB activation and reports the probe
 * pending until its ten-byte response completes or the route rejects it.
 *
 * @param[in,out] service Idle configured updater route.
 * @return True when the probe started; otherwise false.
 */
bool usb_updater_service_start_probe(UsbUpdaterService *service) {
    if (service == NULL || service->probe_status == USB_UPDATER_PROBE_PENDING ||
        exchange_active(service) || !start_exchange(service, probe, sizeof(probe))) {
        return false;
    }
    service->exchange_is_probe = true;
    service->probe_status = USB_UPDATER_PROBE_PENDING;
    return true;
}

/**
 * @brief Enables or disables updater USB request service.
 *
 * Applies the runtime transition's updater USB activation state without changing its selected
 * route, probe result, or response selector.
 *
 * @param[in,out] service Configured updater service.
 * @param[in] active True to accept updater USB requests; false to stop accepting them.
 */
void usb_updater_service_set_usb_active(UsbUpdaterService *service, bool active) {
    if (service != NULL) {
        service->usb_active = active;
    }
}

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
void usb_updater_service_run(UsbUpdaterService *service, const UsbUpdaterServiceInput *input) {
    if (service == NULL || input == NULL) {
        return;
    }
    service_exchange(service, input->now_ms);
    if (!service->usb_active || !usb_service_due(input->now_ms, service->usb_deadline_ms)) {
        return;
    }
    service->usb_deadline_ms = input->now_ms + USB_UPDATER_SERVICE_INTERVAL_MS;
    if (!usb_device_updater_channel_idle()) {
        return;
    }
    if (service->pending_response_length != 0) {
        if (usb_device_queue_updater_response(service->pending_response,
                                              service->pending_response_length)) {
            service->pending_response_length = 0;
        }
        return;
    }
    if (exchange_active(service)) {
        return;
    }

    if (usb_device_take_updater_packet(&service->host_packet) &&
        usb_updater_protocol_decode(service->host_packet.data, service->host_packet.length,
                                    &service->host_request)) {
        handle_request(service, input, &service->host_request);
    }
}

/**
 * @brief Returns the current updater route probe result.
 *
 * Exposes idle, pending, complete, or failed state without consuming it.
 *
 * @param[in] service Updater service to inspect.
 * @return Current probe result, or idle for a null service.
 */
UsbUpdaterProbeStatus usb_updater_service_probe_status(const UsbUpdaterService *service) {
    return service == NULL ? USB_UPDATER_PROBE_IDLE : service->probe_status;
}

/**
 * @brief Takes a guarded updater USB reset request.
 *
 * Clears the one-shot reset latch after exposing it to the runtime owner.
 *
 * @param[in,out] service Updater service holding the reset latch.
 * @return True once for each accepted F8/09/01/FE request; otherwise false.
 */
bool usb_updater_service_take_reset(UsbUpdaterService *service) {
    if (service == NULL || !service->reset_requested) {
        return false;
    }
    service->reset_requested = false;
    return true;
}
