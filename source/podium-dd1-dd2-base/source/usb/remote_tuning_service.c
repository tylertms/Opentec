#include "usb/remote_tuning_service.h"

#include <stddef.h>
#include <string.h>

#include "wheel/protocol.h"

/** @brief Remote-tuning command, selection, and report constants. */
enum {
    REMOTE_TUNING_PACKET_ACTIVE = 2,          /**< Active-state packet type. */
    REMOTE_TUNING_PACKET_SELECTION = 4,       /**< Selection packet type. */
    REMOTE_TUNING_PACKET_REFRESH = 5,         /**< Refresh packet type. */
    REMOTE_TUNING_COMMAND_MENU = 1,           /**< Menu selection command kind. */
    REMOTE_TUNING_COMMAND_MULTI_POSITION = 2, /**< Multi-position selection command kind. */
    REMOTE_TUNING_COMMAND_SETUP = 3,          /**< Setup selection command kind. */
    REMOTE_TUNING_COMMAND_ENCODER = 4,        /**< Encoder selection command kind. */
    REMOTE_TUNING_ITM_RECORD_ROUTE = 2,       /**< Route owned by the ITM consumer. */
    REMOTE_TUNING_MENU_SELECTION_MAXIMUM = 6, /**< Largest accepted menu selection. */
    REMOTE_TUNING_MULTI_POSITION_SELECTION_MAXIMUM =
        11,                                    /**< Largest accepted multi-position selection. */
    REMOTE_TUNING_SETUP_SELECTION_MAXIMUM = 6, /**< Largest accepted setup selection. */
    REMOTE_TUNING_TELEMETRY_CLEAR_SELECTION =
        11,                             /**< Multi-position value that clears the ITM set. */
    REMOTE_TUNING_HOST_REPORT_ID = 5,   /**< Host telemetry report identifier. */
    REMOTE_TUNING_HOST_REPORT_TYPE = 1, /**< Host telemetry report type. */
    REMOTE_TUNING_HOST_REPORT_NATIVE_MARKER = 0xff,      /**< Native host report marker. */
    REMOTE_TUNING_HOST_REPORT_PLAYSTATION_MARKER = 0x35, /**< PlayStation host report marker. */
    REMOTE_TUNING_HOST_REPORT_XBOX_MARKER = 0x36,        /**< Xbox host report marker. */
    REMOTE_TUNING_HOST_REPORT_HEADER_SIZE = 3, /**< Bytes reserved for the host report header. */
    REMOTE_TUNING_HOST_REPORT_RECORD_COUNT =
        12, /**< Maximum five-byte records in one host report. */
    REMOTE_TUNING_PHYSICAL_SELECTION_MAXIMUM =
        USB_REMOTE_TUNING_ITM_SET_COUNT, /**< Largest ITM set selected by physical controls. */
    REMOTE_TUNING_STANDARD_NEXT_BUTTON = 0x04,     /**< Standard-wheel next button bit. */
    REMOTE_TUNING_STANDARD_PREVIOUS_BUTTON = 0x02, /**< Standard-wheel previous button bit. */
    REMOTE_TUNING_STANDARD_BUTTON_MASK = 0x0f,     /**< Standard-wheel navigation button mask. */
    REMOTE_TUNING_ENCODER_INPUT_FIRST = 1,         /**< First physical rotary position. */
    REMOTE_TUNING_ENCODER_INPUT_LAST = 12,         /**< Last physical rotary position. */
    REMOTE_TUNING_ENCODER_SELECTION_FIRST = 1,     /**< First legacy encoder selection. */
    REMOTE_TUNING_ENCODER_SELECTION_LAST = 5,      /**< Last legacy encoder selection. */
    REMOTE_TUNING_NAVIGATION_NEXT = 0x10,          /**< Setup-navigation next motion code. */
    REMOTE_TUNING_NAVIGATION_PREVIOUS = 0x20,      /**< Setup-navigation previous motion code. */
    REMOTE_TUNING_NAVIGATION_NEUTRAL = 0x30,       /**< Setup-navigation neutral motion code. */
    REMOTE_TUNING_SETUP_PAGE_COUNT = 6,            /**< Number of extended setup pages. */
    WHEEL_MODE_STANDARD = 0x10,                    /**< Standard attached-wheel mode value. */
};

typedef struct {
    uint16_t key;
    uint8_t format;
    bool secondary;
} UsbRemoteTuningItmField;

typedef struct {
    UsbRemoteTuningItmField fields[USB_REMOTE_TUNING_ITM_FIELD_COUNT];
    uint8_t count;
} UsbRemoteTuningItmDefinition;

static const UsbRemoteTuningItmDefinition itm_definitions[USB_REMOTE_TUNING_ITM_SET_COUNT] = {
    [REMOTE_TELEMETRY_SPEED - 1] = {{{1, 0x34, true}}, 1},
    [REMOTE_TELEMETRY_RPM - 1] = {{{2, 0x06, false}, {3, 0x06, false}}, 2},
    [REMOTE_TELEMETRY_GEAR - 1] = {{{4, 0xa2, false}}, 1},
    [REMOTE_TELEMETRY_POSITION - 1] = {{{501, 0x24, true}}, 1},
    [REMOTE_TELEMETRY_LAP - 1] = {{{505, 0x24, true}}, 1},
    [REMOTE_TELEMETRY_FUEL - 1] = {{{5, 0x18, true}, {6, 0x18, true}}, 2},
    [REMOTE_TELEMETRY_DRS - 1] = {{{14, 0x41, false}, {15, 0x12, false}}, 2},
    [REMOTE_TELEMETRY_DRIVER_AIDS - 1] = {{{18, 0x22, false}, {20, 0x22, false}}, 2},
    [REMOTE_TELEMETRY_ERS - 1] = {{{9, 0x09, false}}, 1},
    [REMOTE_TELEMETRY_DELTA - 1] = {{{516, 0x1a, false}}, 1},
};

static uint8_t format_unsigned(char *output, uint32_t value) {
    char reversed[10];
    uint8_t count = 0;
    do {
        reversed[count++] = (char)('0' + value % 10u);
        value /= 10u;
    } while (value != 0);
    for (uint8_t index = 0; index < count; index++) {
        output[index] = reversed[count - index - 1u];
    }
    return count;
}

static uint8_t format_signed(char *output, int32_t value) {
    uint8_t offset = 0;
    uint32_t magnitude = (uint32_t)value;
    if (value < 0) {
        output[offset++] = '-';
        magnitude = (uint32_t)(-(value + 1)) + 1u;
    }
    return (uint8_t)(offset + format_unsigned(output + offset, magnitude));
}

static uint8_t format_padded(char *output, int32_t value, uint8_t width) {
    bool negative = value < 0;
    uint32_t magnitude = negative ? (uint32_t)(-(value + 1)) + 1u : (uint32_t)value;
    char reversed[10];
    uint8_t digits = 0;
    while (magnitude != 0) {
        reversed[digits++] = (char)('0' + magnitude % 10u);
        magnitude /= 10u;
    }
    while ((uint8_t)(digits + (negative ? 1u : 0u)) < width) {
        reversed[digits++] = '0';
    }
    uint8_t written = 0;
    if (negative) {
        output[written++] = '-';
    }
    while (digits != 0) {
        output[written++] = reversed[--digits];
    }
    return written;
}

static uint8_t format_integer(char *output, int32_t value, uint8_t width) {
    return width == 0 ? format_signed(output, value) : format_padded(output, value, width);
}

static uint8_t format_fixed(char *output, float value, uint8_t precision) {
    if (value < 0.0f) {
        value = -value;
    }
    uint32_t scale = 1;
    for (uint8_t index = 0; index < precision; index++) {
        scale *= 10u;
    }
    uint32_t scaled = (uint32_t)(value * (float)(scale * 10u));
    scaled = scaled / 10u + (scaled % 10u > 4u ? 1u : 0u);
    uint8_t written = format_unsigned(output, scaled / scale);
    if (precision == 0) {
        return written;
    }
    output[written++] = '.';
    written += format_padded(output + written, (int32_t)(scaled % scale), precision);
    return written;
}

static uint8_t format_time(char *output, float value, uint8_t precision) {
    if (precision == 1) {
        output[0] = value > 0.0f ? '+' : '-';
        uint8_t written = (uint8_t)(1u + format_fixed(output + 1, value, 2));
        output[written++] = 's';
        while (written < 8) {
            output[written++] = ' ';
        }
        return written;
    }
    if (precision != 0 && precision != 2) {
        return 0;
    }
    uint32_t seconds = (uint32_t)value;
    uint8_t written = 0;
    if (seconds > 3599u) {
        written += format_padded(output + written, (int32_t)(seconds / 3600u), 3);
        output[written++] = ':';
    }
    written += format_padded(output + written, (int32_t)(seconds / 60u % 60u), 2);
    output[written++] = ':';
    written += format_padded(output + written, (int32_t)(seconds % 60u), 2);
    if (seconds <= 3599u) {
        output[written++] = '.';
        uint32_t fraction =
            precision == 0 ? (uint32_t)(value * 1000.0f) % 1000u : (uint32_t)(value * 10.0f) % 10u;
        written += format_padded(output + written, (int32_t)fraction, precision == 0 ? 3 : 1);
    }
    return written;
}

static void format_gear(char output[USB_REMOTE_TUNING_ITM_TEXT_SIZE], uint8_t value) {
    output[0] = value >= 1 && value <= 9 ? (char)('0' + value)
                : value == 0             ? 'n'
                : value == UINT8_MAX     ? 'r'
                                         : 0;
    output[1] = 0;
}

static void format_itm_value(char output[USB_REMOTE_TUNING_ITM_TEXT_SIZE], uint8_t format,
                             const uint8_t *payload, uint8_t length) {
    memset(output, 0, USB_REMOTE_TUNING_ITM_TEXT_SIZE);
    uint8_t kind = format & 0x0fu;
    uint8_t width = format >> 4u;
    uint8_t written = 0;
    if (kind == 1) {
        written = length < USB_REMOTE_TUNING_ITM_TEXT_SIZE - 1u
                      ? length
                      : USB_REMOTE_TUNING_ITM_TEXT_SIZE - 1u;
        memcpy(output, payload, written);
    } else if (kind == 2 && length >= 1) {
        written = format_integer(output, payload[0], width);
    } else if (kind == 3 && length >= 1) {
        written = format_integer(output, (int8_t)payload[0], width);
    } else if ((kind == 4 || kind == 9) && length >= 2) {
        uint16_t value = payload[0] | (uint16_t)payload[1] << 8;
        if (kind == 9 && width == 4) {
            if (value > 999) {
                value = 999;
            }
            written = format_padded(output, value / 10u, 2);
            output[written++] = '.';
            written += format_padded(output + written, value % 10u, 1);
        } else if (kind == 4 && width == 0x0f) {
            output[written++] = '/';
            written += format_padded(output + written, value, 3);
        } else {
            written = format_integer(output, value, width);
        }
    } else if (kind == 5 && length >= 2) {
        written = format_integer(output, (int16_t)(payload[0] | (uint16_t)payload[1] << 8), width);
    } else if (kind == 6 && length >= 4) {
        written = format_integer(output,
                                 (int32_t)((uint32_t)payload[0] | (uint32_t)payload[1] << 8 |
                                           (uint32_t)payload[2] << 16 | (uint32_t)payload[3] << 24),
                                 width);
    } else if (kind == 7 && length >= 4) {
        written = format_integer(output,
                                 (int32_t)((uint32_t)payload[0] | (uint32_t)payload[1] << 8 |
                                           (uint32_t)payload[2] << 16 | (uint32_t)payload[3] << 24),
                                 width);
    } else if (kind == 8 && length >= 4) {
        float value;
        memcpy(&value, payload, sizeof(value));
        written = format_fixed(output, value, width);
        while (written <= 4) {
            output[written++] = ' ';
        }
    } else if (kind == 10 && length >= 4) {
        float value;
        memcpy(&value, payload, sizeof(value));
        written = format_time(output, value, width);
    }
    if (kind == 9 && written < USB_REMOTE_TUNING_ITM_TEXT_SIZE - 1u) {
        output[written++] = '%';
    }
    output[written] = 0;
}

static void initialize_itm_page(UsbRemoteTuningItmPage *page) {
    static const char *const
        initial[USB_REMOTE_TUNING_ITM_SET_COUNT][USB_REMOTE_TUNING_ITM_FIELD_COUNT] = {
            {"---", "", "---", "-- ", "--:--.-", "--:--.---", ""},
            {"---", "", "-.-  ", "--- ", "", "", "---    "},
            {"---", "", "- ", "- ", "-     ", "---", "--.-%"},
            {"---", "", "--:--.---", "--:--.---", "-.--    ", "-.--    ", ""},
            {"---", "", "---", "---", "---", "---", ""},
            {"---", "", "---", "---", "", "", ""},
            {"---", "", "---", "---", "", "", ""},
            {"---", "", "---", "---", "", "", ""},
            {"---", "", "---", "", "", "", ""},
            {"---", "", "---", "", "", "", ""},
        };
    static const char *const
        secondary[USB_REMOTE_TUNING_ITM_SET_COUNT][USB_REMOTE_TUNING_ITM_FIELD_COUNT] = {
            {"", "", "/ ---", "/ --", "", "", ""}, {"", "", "--- ", "", "", "", ""},
            {"", "", "", "", "", "C", ""},         {"", "", "", "", "", "", ""},
            {"", "", "C", "C", "C", "C", ""},      {"", "", "", "", "", "", ""},
            {"", "", "", "", "", "", ""},          {"", "", "", "", "", "", ""},
            {"", "", "", "", "", "", ""},          {"", "", "", "", "", "", ""},
        };
    for (uint8_t index = 0; index < USB_REMOTE_TUNING_ITM_FIELD_COUNT; index++) {
        memcpy(page->values[index], initial[page->page - 1u][index],
               strlen(initial[page->page - 1u][index]) + 1u);
        memcpy(page->secondary_values[index], secondary[page->page - 1u][index],
               strlen(secondary[page->page - 1u][index]) + 1u);
    }
}

static bool select_itm_set(UsbRemoteTuningService *service, uint8_t set) {
    if (set < 1 || set > USB_REMOTE_TUNING_ITM_SET_COUNT) {
        return false;
    }
    if (service->itm_page.page == set && service->telemetry.metric == (RemoteTelemetryMetric)set) {
        return true;
    }
    if (!remote_telemetry_select(&service->telemetry, (RemoteTelemetryMetric)set)) {
        return false;
    }

    memset(&service->itm_page, 0, sizeof(service->itm_page));
    service->itm_page.page = set;
    initialize_itm_page(&service->itm_page);
    service->itm_page.field_count = itm_definitions[set - 1u].count;
    service->itm_page.revision++;
    return true;
}

static void consume_itm_records(UsbRemoteTuningService *service, bool extended_mode,
                                bool *reset_requested) {
    const UsbRemoteTuningItmDefinition *definition =
        service->itm_page.page >= 1 && service->itm_page.page <= USB_REMOTE_TUNING_ITM_SET_COUNT
            ? &itm_definitions[service->itm_page.page - 1u]
            : NULL;
    if (reset_requested != NULL) {
        *reset_requested = false;
    }
    for (uint8_t index = 0; index < USB_REMOTE_TUNING_RECORD_COUNT; index++) {
        UsbRemoteTuningRecord *record = &service->records.records[index];
        if (record->type != REMOTE_TUNING_ITM_RECORD_ROUTE) {
            continue;
        }
        uint8_t channel = record->selector & 0x0fu;
        bool overlay = (record->selector & 0x80u) != 0;
        RemoteTelemetryRecordResult result =
            overlay ? remote_telemetry_apply_overlay(&service->telemetry, channel, record->value,
                                                     record->payload, record->payload_length)
                    : remote_telemetry_apply_primary(&service->telemetry, channel, record->value,
                                                     record->payload, record->payload_length);
        bool page_applied = false;
        if (result != REMOTE_TELEMETRY_RECORD_IGNORED && definition != NULL &&
            channel < definition->count && record->value == definition->fields[channel].key) {
            const UsbRemoteTuningItmField *field = &definition->fields[channel];
            if (overlay) {
                if (field->secondary) {
                    uint8_t length = record->payload_length < USB_REMOTE_TUNING_ITM_TEXT_SIZE - 1u
                                         ? record->payload_length
                                         : USB_REMOTE_TUNING_ITM_TEXT_SIZE - 1u;
                    memset(service->itm_page.secondary_values[channel], 0,
                           sizeof(service->itm_page.secondary_values[channel]));
                    memcpy(service->itm_page.secondary_values[channel], record->payload, length);
                    page_applied = true;
                }
            } else if (service->itm_page.page == REMOTE_TELEMETRY_GEAR && channel == 0 &&
                       record->payload_length != 0) {
                format_gear(service->itm_page.values[channel], record->payload[0]);
                page_applied = true;
            } else {
                format_itm_value(service->itm_page.values[channel], field->format, record->payload,
                                 record->payload_length);
                page_applied = true;
            }
        }
        if (page_applied) {
            service->itm_page.revision++;
        }
        if (result != REMOTE_TELEMETRY_RECORD_IGNORED || page_applied) {
            memset(record, 0, sizeof(*record));
            service->records.count--;
            continue;
        }
        if (extended_mode) {
            if (!overlay && reset_requested != NULL) {
                *reset_requested = true;
            }
            continue;
        }
        memset(record, 0, sizeof(*record));
        service->records.count--;
        break;
    }
}

/**
 * @brief Tests a one-based remote-tuning selection.
 *
 * Accepts values from one through the supplied inclusive maximum.
 *
 * @param[in] value Requested selection.
 * @param[in] maximum Largest supported selection.
 * @return True when the selection is in range; otherwise false.
 */
static bool selection_valid(uint8_t value, uint8_t maximum) {
    return value != 0 && value <= maximum;
}

/**
 * @brief Applies a pending host telemetry selection.
 *
 * While the remote session is active outside extended wheel mode, values one through ten select
 * the corresponding ITM set and value eleven clears the set. A successful change consumes the
 * shared remote selection fields and queues route-two controls for the host.
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
    bool selected = selection == REMOTE_TUNING_TELEMETRY_CLEAR_SELECTION
                        ? remote_telemetry_select(&service->telemetry, REMOTE_TELEMETRY_NONE)
                        : select_itm_set(service, selection);
    if (!selected) {
        return;
    }
    if (selection == REMOTE_TUNING_TELEMETRY_CLEAR_SELECTION) {
        service->itm_page = (UsbRemoteTuningItmPage){0};
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
 * Sets or clears the active state, applies a retained telemetry selection when activated, requests
 * clearing telemetry subscriptions when deactivated, and queues response 2 or 0xFF for the two
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
        service->itm_page = (UsbRemoteTuningItmPage){0};
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
 * non-extended session consumes multi-position values as ITM set choices. Setup
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
 * latch and queue response 5. An active non-extended session requests clearing its telemetry
 * selection, and every refresh packet schedules downstream refresh synchronization.
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
        service->itm_page = (UsbRemoteTuningItmPage){0};
    }
    service->refresh_sync_pending = true;
}

void usb_remote_tuning_service_init(UsbRemoteTuningService *service) {
    memset(service, 0, sizeof(*service));
    remote_telemetry_init(&service->telemetry);
}

bool usb_remote_tuning_service_update_physical_selection(UsbRemoteTuningService *service,
                                                         uint8_t wheel_mode, bool profile_mode,
                                                         bool tuning_display_supported,
                                                         bool adapter_connected,
                                                         int8_t tuning_input,
                                                         uint8_t auxiliary_buttons) {
    if (service == NULL) {
        return false;
    }
    (void)adapter_connected;

    RemoteTelemetryMetric selection = service->telemetry.metric;
    if (service->active && tuning_display_supported && profile_mode) {
        if (wheel_mode != WHEEL_MODE_STANDARD) {
            if (tuning_input != service->physical_previous_input) {
                if (tuning_input > 0) {
                    selection =
                        selection >= (RemoteTelemetryMetric)REMOTE_TUNING_PHYSICAL_SELECTION_MAXIMUM
                            ? REMOTE_TELEMETRY_NONE
                            : (RemoteTelemetryMetric)(selection + 1);
                } else if (tuning_input < 0) {
                    selection = selection == REMOTE_TELEMETRY_NONE
                                    ? REMOTE_TELEMETRY_DELTA
                                    : (RemoteTelemetryMetric)(selection - 1);
                }
            }
            service->physical_previous_input = tuning_input;
        } else {
            uint8_t flags = auxiliary_buttons & REMOTE_TUNING_STANDARD_BUTTON_MASK;
            if (service->physical_input_released) {
                if ((flags & REMOTE_TUNING_STANDARD_NEXT_BUTTON) != 0) {
                    service->physical_button_flags = flags;
                    service->physical_input_released = false;
                    selection =
                        selection >= (RemoteTelemetryMetric)REMOTE_TUNING_PHYSICAL_SELECTION_MAXIMUM
                            ? REMOTE_TELEMETRY_NONE
                            : (RemoteTelemetryMetric)(selection + 1);
                } else if ((flags & REMOTE_TUNING_STANDARD_PREVIOUS_BUTTON) != 0) {
                    service->physical_button_flags = flags;
                    service->physical_input_released = false;
                    selection = selection == REMOTE_TELEMETRY_NONE
                                    ? REMOTE_TELEMETRY_DELTA
                                    : (RemoteTelemetryMetric)(selection - 1);
                }
            }
            if (service->physical_button_flags != flags) {
                service->physical_input_released = true;
            }
        }
    } else if (!service->active && selection != REMOTE_TELEMETRY_NONE) {
        selection = REMOTE_TELEMETRY_NONE;
    }

    if (selection == service->telemetry.metric) {
        return false;
    }
    bool selected = remote_telemetry_select(&service->telemetry, selection);
    if (selected && selection != REMOTE_TELEMETRY_NONE) {
        memset(&service->itm_page, 0, sizeof(service->itm_page));
        service->itm_page.page = (uint8_t)selection;
        initialize_itm_page(&service->itm_page);
        service->itm_page.field_count = itm_definitions[selection - 1u].count;
        service->itm_page.revision++;
    } else if (selected) {
        service->itm_page = (UsbRemoteTuningItmPage){0};
    }
    return selected;
}

bool usb_remote_tuning_service_update_legacy_encoder(UsbRemoteTuningService *service,
                                                     uint8_t wheel_mode, uint8_t rotary_position) {
    if (service == NULL) {
        return false;
    }
    bool changed = false;
    if (service->active && wheel_mode == WHEEL_MODE_REMOTE_TUNING_LEGACY &&
        service->physical_rotary_initialized &&
        rotary_position != service->physical_rotary_position) {
        bool increase = service->physical_rotary_position < rotary_position;
        if (service->physical_rotary_position == REMOTE_TUNING_ENCODER_INPUT_LAST &&
            rotary_position == REMOTE_TUNING_ENCODER_INPUT_FIRST) {
            increase = true;
        } else if (service->physical_rotary_position == REMOTE_TUNING_ENCODER_INPUT_FIRST &&
                   rotary_position == REMOTE_TUNING_ENCODER_INPUT_LAST) {
            increase = false;
        }
        uint16_t counter = service->encoder_counter;
        counter = increase ? counter + 1 : counter - 1;
        if (counter > REMOTE_TUNING_ENCODER_SELECTION_LAST) {
            counter = REMOTE_TUNING_ENCODER_SELECTION_FIRST;
        } else if (counter == 0 || counter > UINT8_MAX) {
            counter = REMOTE_TUNING_ENCODER_SELECTION_LAST;
        }
        service->encoder_counter = counter;
        service->encoder_selection = (uint8_t)counter;
        queue_response(service, wheel_mode, REMOTE_TUNING_RESPONSE_SETUP,
                       service->encoder_selection);
        changed = true;
    }
    service->physical_rotary_position = rotary_position;
    service->physical_rotary_initialized = true;
    return changed;
}

bool usb_remote_tuning_service_update_setup_navigation(UsbRemoteTuningService *service,
                                                       uint8_t wheel_mode, bool profile_mode,
                                                       uint8_t motion) {
    if (service == NULL || wheel_mode != WHEEL_MODE_REMOTE_TUNING_EXTENDED || !profile_mode) {
        return false;
    }
    uint8_t previous = service->physical_navigation_input;
    if (motion == previous) {
        return false;
    }
    service->physical_navigation_input = motion;
    if (motion == REMOTE_TUNING_NAVIGATION_NEUTRAL) {
        return false;
    }
    if (motion == REMOTE_TUNING_NAVIGATION_NEXT) {
        service->setup_page = service->setup_page >= REMOTE_TUNING_SETUP_PAGE_COUNT - 1
                                  ? 0
                                  : (uint8_t)(service->setup_page + 1);
    } else if (motion == REMOTE_TUNING_NAVIGATION_PREVIOUS) {
        service->setup_page = service->setup_page == 0 ? REMOTE_TUNING_SETUP_PAGE_COUNT - 1
                                                       : (uint8_t)(service->setup_page - 1);
    } else {
        return false;
    }
    queue_response(service, wheel_mode, REMOTE_TUNING_RESPONSE_NEXT_SETUP_PAGE,
                   service->setup_page);
    return true;
}

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

bool usb_remote_tuning_service_take_forward_batch(
    UsbRemoteTuningService *service, uint8_t wheel_mode,
    uint8_t output[USB_REMOTE_TUNING_FORWARD_BATCH_SIZE], uint8_t *length) {
    (void)wheel_mode;
    if (service == NULL) {
        return false;
    }
    return usb_remote_tuning_records_take_forward_batch(&service->records, output, length);
}

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

bool usb_remote_tuning_service_take_telemetry_report(UsbRemoteTuningService *service,
                                                     uint8_t wheel_mode,
                                                     uint8_t output[REMOTE_TELEMETRY_REPORT_SIZE]) {
    if (service == NULL || output == NULL) {
        return false;
    }
    if (!service->active) {
        return false;
    }
    apply_telemetry_selection(service, wheel_mode);
    bool reset_requested = false;
    bool extended_mode = wheel_mode == WHEEL_MODE_REMOTE_TUNING_EXTENDED;
    consume_itm_records(service, extended_mode, &reset_requested);
    if (reset_requested) {
        service->refresh_requested = false;
        service->pending_response = (RemoteTuningResponse){0};
    }
    if (extended_mode) {
        return false;
    }
    return service->active && remote_telemetry_take_report(&service->telemetry, output);
}

/**
 * @brief Returns the active retained intelligent-telemetry-mode page.
 *
 * @param[in] service Remote-tuning service to inspect.
 * @return Active page, or null when the session or page is inactive.
 */
const UsbRemoteTuningItmPage *
usb_remote_tuning_service_itm_page(const UsbRemoteTuningService *service) {
    return service == NULL || !service->active || service->itm_page.page == 0 ? NULL
                                                                              : &service->itm_page;
}
