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
 * Sets or clears the active state, queues response 2 or 0xFF for the two remote-tuning wheel modes,
 * and latches downstream active-state synchronization when an adapter is connected or an active
 * session is cleared.
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
 * Retains menu selections 1 through 6 and multi-position selections 1 through 11. Setup selections
 * use values 1 through 6, with extended mode routing them only while local setup selection is
 * allowed. Extended values 1 through 5 replace the reported setup page, while value 6 preserves
 * the prior page. Encoder selection is retained without range conversion in legacy mode.
 * Unsupported selection kinds clear the three non-encoder selections.
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
            service->vendor_response_pending = true;
        }
        break;
    case REMOTE_TUNING_COMMAND_MULTI_POSITION:
        if (selection_valid(value, REMOTE_TUNING_MULTI_POSITION_SELECTION_MAXIMUM)) {
            service->multi_position_selection = value;
            service->vendor_response_pending = true;
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
            service->vendor_response_pending = true;
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
 * latch and queue response 5. Every refresh packet also schedules downstream refresh
 * synchronization.
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
    service->refresh_sync_pending = true;
}

/**
 * @brief Initializes the host remote-tuning service.
 *
 * Clears session state, downstream work latches, and all 32 retained control records.
 *
 * @param[out] service Remote-tuning service to initialize.
 */
void usb_remote_tuning_service_init(UsbRemoteTuningService *service) {
    memset(service, 0, sizeof(*service));
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
