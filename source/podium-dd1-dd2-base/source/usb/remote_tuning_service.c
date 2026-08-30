#include "usb/remote_tuning_service.h"

#include <stddef.h>
#include <string.h>

#include "wheel/protocol.h"

enum {
    REMOTE_TUNING_PACKET_ACTIVE = 2,
    REMOTE_TUNING_PACKET_SELECTION = 4,
    REMOTE_TUNING_PACKET_REFRESH = 5,
    REMOTE_TUNING_COMMAND_MENU = 1,
    REMOTE_TUNING_COMMAND_MULTI_POSITION = 2,
    REMOTE_TUNING_COMMAND_SETUP = 3,
    REMOTE_TUNING_COMMAND_ENCODER = 4,
    REMOTE_TUNING_MENU_SELECTION_MAXIMUM = 6,
    REMOTE_TUNING_MULTI_POSITION_SELECTION_MAXIMUM = 11,
    REMOTE_TUNING_SETUP_SELECTION_MAXIMUM = 6,
    REMOTE_TUNING_TELEMETRY_CLEAR_SELECTION = 11,
    REMOTE_TUNING_HOST_REPORT_ID = 5,
    REMOTE_TUNING_HOST_REPORT_TYPE = 1,
    REMOTE_TUNING_HOST_REPORT_NATIVE_MARKER = 0xff,
    REMOTE_TUNING_HOST_REPORT_PLAYSTATION_MARKER = 0x35,
    REMOTE_TUNING_HOST_REPORT_XBOX_MARKER = 0x36,
    REMOTE_TUNING_HOST_REPORT_HEADER_SIZE = 3,
    REMOTE_TUNING_HOST_REPORT_RECORD_COUNT = 12,
};

/**
 * @brief Tests a one-based remote-tuning selection.
 *
 * Accepts values from one through the supplied inclusive maximum.
 *
 * @param[in] value Requested selection.
 * @param[in] maximum Largest supported selection.
 * @return True when the selection is in range.
 */
static bool selection_valid(uint8_t value, uint8_t maximum) {
    return value != 0 && value <= maximum;
}

/**
 * @brief Applies a pending host telemetry selection.
 *
 * While the remote session is active outside extended wheel mode, values one through ten select
 * the corresponding telemetry metric and value eleven clears the selection. A successful change
 * consumes the shared remote selection fields.
 *
 * @param[in,out] service Remote-tuning session and telemetry state.
 * @param[in] wheel_mode Current attached-wheel mode.
 */
static void apply_telemetry_selection(UsbRemoteTuningService *service, uint8_t wheel_mode) {
    if (!service->active || wheel_mode == WHEEL_MODE_REMOTE_TUNING_EXTENDED ||
        service->command_type != REMOTE_TUNING_COMMAND_MULTI_POSITION ||
        service->multi_position_selection == 0) {
        return;
    }

    uint8_t selection = service->multi_position_selection;
    RemoteTelemetryMetric metric = selection == REMOTE_TUNING_TELEMETRY_CLEAR_SELECTION
                                       ? REMOTE_TELEMETRY_NONE
                                       : (RemoteTelemetryMetric)selection;
    if (!remote_telemetry_select(&service->telemetry, metric)) {
        return;
    }
    service->setup_selection = 0;
    service->menu_selection = 0;
    service->multi_position_selection = 0;
    service->command_type = 0;
}

/**
 * @brief Resolves the report marker for a host transport.
 *
 * Maps native, PlayStation, and Xbox transports to their telemetry control-report marker byte.
 *
 * @param[in] host Host transport framing choice.
 * @return Resolved first report byte, or zero for an unsupported transport.
 */
static uint8_t host_report_marker(UsbRemoteTuningHost host) {
    switch (host) {
    case USB_REMOTE_TUNING_HOST_NATIVE:
        return REMOTE_TUNING_HOST_REPORT_NATIVE_MARKER;
    case USB_REMOTE_TUNING_HOST_PLAYSTATION:
        return REMOTE_TUNING_HOST_REPORT_PLAYSTATION_MARKER;
    case USB_REMOTE_TUNING_HOST_XBOX:
        return REMOTE_TUNING_HOST_REPORT_XBOX_MARKER;
    default:
        return 0;
    }
}

/**
 * @brief Queues a mode-specific remote-tuning response.
 *
 * Routes the response to the legacy transport in wheel mode 0x0E and the extended transport in
 * wheel mode 0x1C. Other wheel modes leave the previous pending response unchanged.
 *
 * @param[in,out] service Remote-tuning session state.
 * @param[in] wheel_mode Current attached-wheel mode.
 * @param[in] response Remote-tuning response code.
 * @param[in] value Single-byte response value.
 */
static void queue_response(UsbRemoteTuningService *service, uint8_t wheel_mode,
                           RemoteTuningResponseCode response, uint8_t value) {
    if (wheel_mode == WHEEL_MODE_REMOTE_TUNING_LEGACY) {
        service->pending_response = (RemoteTuningResponse){
            .link = REMOTE_TUNING_LINK_LEGACY,
            .code = response,
            .value = value,
        };
    } else if (wheel_mode == WHEEL_MODE_REMOTE_TUNING_EXTENDED) {
        service->pending_response = (RemoteTuningResponse){
            .link = REMOTE_TUNING_LINK_EXTENDED,
            .code = response,
            .value = value,
        };
    }
}

/**
 * @brief Applies a remote-tuning active-state packet.
 *
 * Sets or clears the active state, applies a retained telemetry selection when activated, clears
 * telemetry subscriptions when deactivated, and queues response 2 or 0xFF for the two
 * remote-tuning wheel modes. Downstream active-state synchronization is latched when an adapter is
 * connected or an active session is cleared.
 *
 * @param[in,out] service Remote-tuning session state.
 * @param[in] active Requested active state.
 * @param[in] wheel_mode Current attached-wheel mode.
 * @param[in] adapter_connected True while an attached adapter is connected.
 */
static void apply_active(UsbRemoteTuningService *service, bool active, uint8_t wheel_mode,
                         bool adapter_connected) {
    if (!active && service->active) {
        service->active_sync_pending = true;
    }
    service->active = active;
    if (active) {
        apply_telemetry_selection(service, wheel_mode);
    } else {
        (void)remote_telemetry_select(&service->telemetry, REMOTE_TELEMETRY_NONE);
    }
    queue_response(service, wheel_mode,
                   active ? REMOTE_TUNING_RESPONSE_ACTIVE : REMOTE_TUNING_RESPONSE_INACTIVE,
                   active ? 1 : 0);
    if (adapter_connected) {
        service->active_sync_pending = true;
    }
}

/**
 * @brief Applies a remote-tuning selection packet.
 *
 * Retains menu selections 1 through 6 and multi-position selections 1 through 11. An active
 * non-extended session consumes multi-position values as telemetry metric choices. Setup
 * selections use values 1 through 6, with extended mode routing them only while local setup
 * selection is allowed. Extended values 1 through 5 replace the reported setup page, while value
 * 6 preserves the prior page. Encoder selection is retained without range conversion in legacy
 * mode. Unsupported selection kinds clear the three non-encoder selections.
 *
 * @param[in,out] service Remote-tuning session and pending work.
 * @param[in] command Selection kind.
 * @param[in] value Requested selection value.
 * @param[in] wheel_mode Current attached-wheel mode.
 * @param[in] setup_selection_allowed Allows an extended setup-page selection.
 */
static void apply_selection(UsbRemoteTuningService *service, uint8_t command, uint8_t value,
                            uint8_t wheel_mode, bool setup_selection_allowed) {
    service->command_type = command;
    switch (command) {
    case REMOTE_TUNING_COMMAND_MENU:
        if (selection_valid(value, REMOTE_TUNING_MENU_SELECTION_MAXIMUM)) {
            service->menu_selection = value;
        }
        break;
    case REMOTE_TUNING_COMMAND_MULTI_POSITION:
        if (selection_valid(value, REMOTE_TUNING_MULTI_POSITION_SELECTION_MAXIMUM)) {
            service->multi_position_selection = value;
            apply_telemetry_selection(service, wheel_mode);
        }
        break;
    case REMOTE_TUNING_COMMAND_SETUP:
        if (wheel_mode == WHEEL_MODE_REMOTE_TUNING_EXTENDED) {
            if (setup_selection_allowed &&
                selection_valid(value, REMOTE_TUNING_SETUP_SELECTION_MAXIMUM)) {
                service->setup_index = value;
                service->encoder_counter = value;
                service->command_type = 0;
                if (value <= 5) {
                    service->setup_page = value;
                }
                queue_response(service, wheel_mode, REMOTE_TUNING_RESPONSE_SETUP,
                               service->setup_page);
            }
            break;
        }
        service->setup_sync_pending = true;
        service->command_type = 0;
        if (selection_valid(value, REMOTE_TUNING_SETUP_SELECTION_MAXIMUM)) {
            service->setup_selection = value;
        } else {
            service->setup_sync_pending = false;
        }
        break;
    case REMOTE_TUNING_COMMAND_ENCODER:
        if (wheel_mode == WHEEL_MODE_REMOTE_TUNING_LEGACY) {
            service->encoder_selection = value;
            service->encoder_counter = value;
            queue_response(service, wheel_mode, REMOTE_TUNING_RESPONSE_SETUP, value);
            break;
        }
        service->setup_selection = 0;
        service->menu_selection = 0;
        service->multi_position_selection = 0;
        break;
    default:
        service->setup_selection = 0;
        service->menu_selection = 0;
        service->multi_position_selection = 0;
        break;
    }
}

/**
 * @brief Applies a remote-tuning refresh packet.
 *
 * Latches an explicit value-one refresh request. Legacy and extended wheel modes force the refresh
 * latch and queue response 5. An active non-extended session clears its telemetry selection, and
 * every refresh packet schedules downstream refresh synchronization.
 *
 * @param[in,out] service Remote-tuning session and pending work.
 * @param[in] value Refresh request value.
 * @param[in] wheel_mode Current attached-wheel mode.
 */
static void apply_refresh(UsbRemoteTuningService *service, uint8_t value, uint8_t wheel_mode) {
    if (value == 1 || wheel_mode == WHEEL_MODE_REMOTE_TUNING_LEGACY ||
        wheel_mode == WHEEL_MODE_REMOTE_TUNING_EXTENDED) {
        service->refresh_requested = true;
    }
    if (wheel_mode == WHEEL_MODE_REMOTE_TUNING_LEGACY ||
        wheel_mode == WHEEL_MODE_REMOTE_TUNING_EXTENDED) {
        queue_response(service, wheel_mode, REMOTE_TUNING_RESPONSE_REFRESH,
                       service->refresh_requested ? 1 : 0);
    }
    if (service->active && wheel_mode != WHEEL_MODE_REMOTE_TUNING_EXTENDED) {
        (void)remote_telemetry_select(&service->telemetry, REMOTE_TELEMETRY_NONE);
    }
    service->refresh_sync_pending = true;
}

/**
 * @brief Initializes the host remote-tuning service.
 *
 * Clears session state, downstream work latches, all 32 retained control records, and telemetry
 * mappings and queues.
 *
 * @param[out] service Remote-tuning service to initialize.
 */
void usb_remote_tuning_service_init(UsbRemoteTuningService *service) {
    memset(service, 0, sizeof(*service));
    remote_telemetry_init(&service->telemetry);
}

/**
 * @brief Applies one host remote-tuning packet.
 *
 * Extends the session deadline by 60 seconds for every opcode-five packet, retains record packets,
 * and applies active, selection, or refresh state when the required argument bytes are present.
 * Unknown packet types still extend the session deadline.
 *
 * @param[in,out] service Remote-tuning session and retained records.
 * @param[in] command Decoded vendor command.
 * @param[in] now_ms Current monotonic time in milliseconds.
 * @param[in] wheel_mode Current attached-wheel mode.
 * @param[in] setup_selection_allowed Allows an extended setup-page selection.
 * @param[in] adapter_connected True while an attached adapter is connected.
 * @return True when the command uses the remote-tuning vendor route.
 */
bool usb_remote_tuning_service_apply(UsbRemoteTuningService *service,
                                     const UsbVendorCommand *command, uint32_t now_ms,
                                     uint8_t wheel_mode, bool setup_selection_allowed,
                                     bool adapter_connected) {
    if (service == NULL || command == NULL || command->kind != USB_VENDOR_COMMAND_REMOTE_TUNING ||
        command->arguments == NULL || command->length == 0) {
        return false;
    }

    service->session_deadline_ms = now_ms + USB_REMOTE_TUNING_SESSION_TIMEOUT_MS;
    if (usb_remote_tuning_records_apply(&service->records, command)) {
        return true;
    }

    switch (command->arguments[0]) {
    case REMOTE_TUNING_PACKET_ACTIVE:
        if (command->length >= 2) {
            apply_active(service, command->arguments[1] != 0, wheel_mode, adapter_connected);
        }
        break;
    case REMOTE_TUNING_PACKET_SELECTION:
        if (command->length >= 3) {
            apply_selection(service, command->arguments[1], command->arguments[2], wheel_mode,
                            setup_selection_allowed);
        }
        break;
    case REMOTE_TUNING_PACKET_REFRESH:
        if (command->length >= 2) {
            apply_refresh(service, command->arguments[1], wheel_mode);
        }
        break;
    }
    return true;
}

/**
 * @brief Takes the pending attached-wheel remote-tuning response.
 *
 * Takes a pending state response first. Otherwise, builds the next bounded legacy or extended
 * record response for the current wheel mode. Session state and downstream synchronization
 * latches remain unchanged.
 *
 * @param[in,out] service Remote-tuning session that owns the pending response.
 * @param[in] wheel_mode Current attached-wheel mode.
 * @param[out] response Pending attached-wheel response.
 * @return True when a response was taken.
 */
bool usb_remote_tuning_service_take_response(UsbRemoteTuningService *service, uint8_t wheel_mode,
                                             RemoteTuningResponse *response) {
    if (service == NULL || response == NULL) {
        return false;
    }
    if (service->pending_response.code != REMOTE_TUNING_RESPONSE_NONE) {
        *response = service->pending_response;
        service->pending_response = (RemoteTuningResponse){0};
        return true;
    }

    RemoteTuningLink link = REMOTE_TUNING_LINK_NONE;
    if (wheel_mode == WHEEL_MODE_REMOTE_TUNING_LEGACY) {
        link = REMOTE_TUNING_LINK_LEGACY;
    } else if (wheel_mode == WHEEL_MODE_REMOTE_TUNING_EXTENDED) {
        link = REMOTE_TUNING_LINK_EXTENDED;
    }
    return usb_remote_tuning_records_take_response(&service->records, link, response);
}

/**
 * @brief Takes a remote-tuning active state destined for the adapter.
 *
 * Returns the current session state and consumes its downstream synchronization latch.
 *
 * @param[in,out] service Remote-tuning service retaining the state.
 * @param[in] synchronization_allowed True when local tuning presentation permits synchronization.
 * @param[out] active Current session state.
 * @return True when a pending state was taken.
 */
bool usb_remote_tuning_service_take_adapter_active(UsbRemoteTuningService *service,
                                                   bool synchronization_allowed, bool *active) {
    if (service == NULL || active == NULL || !synchronization_allowed ||
        !service->active_sync_pending) {
        return false;
    }
    *active = service->active;
    service->active_sync_pending = false;
    return true;
}

/**
 * @brief Takes the refresh state destined for the adapter.
 *
 * Promotes a pending refresh request into the persistent adapter state, clears the request latch,
 * and consumes the downstream synchronization latch. Packets without a refresh request resend the
 * previously promoted state.
 *
 * @param[in,out] service Remote-tuning service retaining refresh state.
 * @param[out] active Adapter refresh state.
 * @return True when a pending state was taken.
 */
bool usb_remote_tuning_service_take_adapter_refresh_state(UsbRemoteTuningService *service,
                                                          bool *active) {
    if (service == NULL || active == NULL || !service->refresh_sync_pending) {
        return false;
    }
    if (service->refresh_requested) {
        service->adapter_refresh_state = true;
        service->refresh_requested = false;
    }
    *active = service->adapter_refresh_state;
    service->refresh_sync_pending = false;
    return true;
}

/**
 * @brief Takes a setup selection destined for the adapter.
 *
 * Returns the retained one-based setup selection and consumes the shared setup, menu, and
 * multi-position selections after the downstream adapter service accepts the handoff.
 *
 * @param[in,out] service Remote-tuning service retaining the selection.
 * @param[out] selection One-based setup selection.
 * @return True when a pending selection was taken.
 */
bool usb_remote_tuning_service_take_adapter_setup_selection(UsbRemoteTuningService *service,
                                                            uint8_t *selection) {
    if (service == NULL || selection == NULL || !service->setup_sync_pending) {
        return false;
    }
    *selection = service->setup_selection;
    service->setup_selection = 0;
    service->menu_selection = 0;
    service->multi_position_selection = 0;
    service->setup_sync_pending = false;
    return true;
}

/**
 * @brief Queues adapter-originated telemetry controls for the host.
 *
 * Splits the 30-byte adapter area into six five-byte records, ignores entries whose first two
 * bytes are both zero, and appends every other entry to the shared host control queue. Records
 * that exceed the queue capacity are dropped independently.
 *
 * @param[in,out] service Remote-tuning service that owns the host control queue.
 * @param[in] input Complete adapter control area.
 * @return Number of records appended to the host queue.
 */
uint8_t
usb_remote_tuning_service_queue_host_controls(UsbRemoteTuningService *service,
                                              const uint8_t input[REMOTE_TELEMETRY_REPORT_SIZE]) {
    if (service == NULL || input == NULL) {
        return 0;
    }

    uint8_t queued = 0;
    for (uint8_t offset = 0; offset < REMOTE_TELEMETRY_REPORT_SIZE;
         offset += REMOTE_TELEMETRY_SUBSCRIPTION_SIZE) {
        const uint8_t *record = input + offset;
        if ((record[0] != 0 || record[1] != 0) &&
            remote_telemetry_queue_control_record(&service->telemetry, record)) {
            queued++;
        }
    }
    return queued;
}

/**
 * @brief Takes the next generic attached-device command batch.
 *
 * Routes ordinary and legacy-mode route-three records to the generic attached-device transport.
 * Extended remote-tuning mode retains route-three records for its wheel response channel.
 *
 * @param[in,out] service Remote-tuning session containing retained records.
 * @param[in] wheel_mode Current attached-wheel mode.
 * @param[out] output Serialized command records.
 * @param[out] length Produced byte count.
 * @return True when a nonempty forwarding batch was produced.
 */
bool usb_remote_tuning_service_take_forward_batch(
    UsbRemoteTuningService *service, uint8_t wheel_mode,
    uint8_t output[USB_REMOTE_TUNING_FORWARD_BATCH_SIZE], uint8_t *length) {
    if (service == NULL || wheel_mode == WHEEL_MODE_REMOTE_TUNING_EXTENDED) {
        return false;
    }
    return usb_remote_tuning_records_take_forward_batch(&service->records, output, length);
}

/**
 * @brief Takes one host telemetry subscription report.
 *
 * Applies a pending selection, writes the transport marker followed by report identifier five and
 * type one, and consumes up to twelve queued five-byte subscription or clear records. An empty
 * control queue leaves the output unchanged.
 *
 * @param[in,out] service Remote-tuning session and telemetry control queue.
 * @param[in] wheel_mode Current attached-wheel mode.
 * @param[in] host Host transport framing choice.
 * @param[out] output Complete 64-byte host report.
 * @return True when at least one control record was encoded.
 */
bool usb_remote_tuning_service_take_host_report(
    UsbRemoteTuningService *service, uint8_t wheel_mode, UsbRemoteTuningHost host,
    uint8_t output[USB_REMOTE_TUNING_HOST_REPORT_SIZE]) {
    if (service == NULL || output == NULL) {
        return false;
    }
    uint8_t marker = host_report_marker(host);
    if (marker == 0) {
        return false;
    }
    apply_telemetry_selection(service, wheel_mode);
    if (service->telemetry.control_count == 0) {
        return false;
    }

    uint8_t count = 0;
    memset(output, 0, USB_REMOTE_TUNING_HOST_REPORT_SIZE);
    while (count < REMOTE_TUNING_HOST_REPORT_RECORD_COUNT &&
           remote_telemetry_take_control_record(&service->telemetry,
                                                output + REMOTE_TUNING_HOST_REPORT_HEADER_SIZE +
                                                    count * REMOTE_TELEMETRY_SUBSCRIPTION_SIZE)) {
        count++;
    }
    output[0] = marker;
    output[1] = REMOTE_TUNING_HOST_REPORT_ID;
    output[2] = REMOTE_TUNING_HOST_REPORT_TYPE;
    return true;
}

/**
 * @brief Takes the next locally generated attached-wheel telemetry report.
 *
 * Applies any pending selection and consumes all route-two host records outside extended mode,
 * then encodes the next dirty telemetry channel. Extended mode retains those records for its own
 * remote-tuning path.
 *
 * @param[in,out] service Remote-tuning session, records, and telemetry state.
 * @param[in] wheel_mode Current attached-wheel mode.
 * @param[out] output Complete 30-byte attached-wheel telemetry report.
 * @return True when a dirty telemetry channel produced a report.
 */
bool usb_remote_tuning_service_take_telemetry_report(UsbRemoteTuningService *service,
                                                     uint8_t wheel_mode,
                                                     uint8_t output[REMOTE_TELEMETRY_REPORT_SIZE]) {
    if (service == NULL || output == NULL || wheel_mode == WHEEL_MODE_REMOTE_TUNING_EXTENDED) {
        return false;
    }
    apply_telemetry_selection(service, wheel_mode);
    (void)usb_remote_tuning_records_consume_telemetry(&service->records, &service->telemetry);
    return service->active && remote_telemetry_take_report(&service->telemetry, output);
}
