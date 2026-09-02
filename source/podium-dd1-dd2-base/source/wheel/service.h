#ifndef OPENTEC_BASE_WHEEL_SERVICE_H
#define OPENTEC_BASE_WHEEL_SERVICE_H

#include <stdbool.h>
#include <stdint.h>

#include "profile/tuning_entry.h"
#include "serial/service.h"
#include "wheel/adapter_commands.h"
#include "wheel/auxiliary_output.h"
#include "wheel/display_output.h"
#include "wheel/display_overlay.h"
#include "wheel/protocol.h"
#include "wheel/rotary_input.h"
#include "wheel/vibration.h"

/** @brief Dimensions of normalized wheel button and rotary input state. */
enum {
    WHEEL_BUTTON_BANK_COUNT = 3,            /**< Number of normalized button banks. */
    WHEEL_SCAN_SAMPLE_DEPTH = 3,            /**< Number of samples retained by scan filtering. */
    WHEEL_MULTI_POSITION_CHANNEL_COUNT = 3, /**< Number of multi-position rotary channels. */
};

/** @brief Request type currently owned by the wheel serial service. */
typedef enum {
    WHEEL_SERVICE_REQUEST_NONE,     /**< No wheel request is active. */
    WHEEL_SERVICE_REQUEST_PROTOCOL, /**< A command-two protocol request is active. */
    WHEEL_SERVICE_REQUEST_BUTTONS,  /**< A command-three button scan request is active. */
} WheelServiceRequest;

/** @brief State transition produced by alternative-shifter input. */
typedef enum {
    WHEEL_ALTERNATIVE_SHIFTER_UNCHANGED, /**< The activation chord caused no transition. */
    WHEEL_ALTERNATIVE_SHIFTER_ENABLED,   /**< The activation chord enabled alternative shifter mode.
                                          */
    WHEEL_ALTERNATIVE_SHIFTER_DISABLED, /**< The activation chord disabled alternative shifter mode.
                                         */
} WheelAlternativeShifterEvent;

/** @brief One logical multi-position rotary channel. */
typedef struct {
    uint8_t position;       /**< Latest rotary selector position. */
    WheelRotaryEvent event; /**< Debounced event produced for the latest position. */
    bool active;            /**< True when this channel is available in the current wheel mode. */
} WheelMultiPositionChannel;

/** @brief Normalized multi-position rotary input from a wheel or adapter. */
typedef struct {
    WheelMultiPositionChannel
        channels[WHEEL_MULTI_POSITION_CHANNEL_COUNT]; /**< Logical rotary channels. */
    bool
        remap_selectors; /**< True when selector positions use the extended remote-tuning layout. */
} WheelMultiPositionInput;

/** @brief Normalized attached-wheel fields shared by host input formats. */
typedef struct {
    uint16_t secondary_buttons;  /**< Two secondary button bytes in little-endian order. */
    uint8_t directional_buttons; /**< Directional button byte. */
    uint8_t clutch_paddles[2];   /**< Two clutch-paddle bytes. */
    int8_t tuning_input;         /**< Signed tuning input byte. */
    uint8_t motion;              /**< Motion byte copied from the normalized tuning-input field. */
    uint8_t packed_rotary_positions; /**< Packed rotary-position byte. */
    uint8_t auxiliary_report[3];     /**< Three normalized auxiliary-report bytes. */
    uint8_t axis_availability;       /**< Availability bits for the reported wheel axes. */
    bool axis_report_enabled;        /**< True when the current wheel report enables axis output. */
} WheelInputSnapshot;

/** @brief Complete attached-wheel protocol, output, and input service state. */
typedef struct {
    SerialService *transport;                    /**< Shared serial service for wheel requests. */
    WheelProtocol protocol;                      /**< Negotiated wheel protocol state. */
    WheelAdapterCommandService adapter_commands; /**< Extended-adapter command state. */
    WheelRotaryInput rotary_input;               /**< Debounce state for direct rotary selectors. */
    WheelDisplayOutput display_output; /**< Display output currently sent to wheel packets. */
    WheelDisplayOutput default_display_output;  /**< Retained normal display page. */
    WheelDisplayOutput display_override_output; /**< Retained interaction-owned display page. */
    WheelDisplayOverlay display_overlay;        /**< Timed command display presentation. */
    WheelAuxiliaryOutput auxiliary_output;      /**< Auxiliary report and output-option state. */
    uint8_t adapter_display_state;              /**< Last nonzero adapter system-display state. */
    uint8_t request[SERIAL_PACKET_MAX_PAYLOAD_SIZE]; /**< Current serial request payload. */
    uint8_t button_banks[WHEEL_BUTTON_BANK_COUNT];   /**< Filtered button banks for scan mode. */
    uint8_t primary_scan_samples[WHEEL_SCAN_SAMPLE_DEPTH]
                                [WHEEL_BUTTON_BANK_COUNT]; /**< Primary scan-filter history. */
    uint8_t secondary_scan_samples[WHEEL_SCAN_SAMPLE_DEPTH]
                                  [WHEEL_BUTTON_BANK_COUNT]; /**< Secondary scan-filter history. */
    uint32_t protocol_deadline_ms;           /**< Monotonic deadline for protocol activity. */
    uint32_t protocol_transport_deadline_ms; /**< Earliest next logical protocol request time. */
    uint16_t
        protocol_transport_interval_ms; /**< Interval assigned by the current protocol phase. */
    uint32_t alternative_shifter_deadline_ms; /**< Monotonic debounce deadline for alternative
                                                 shifter. */
    uint8_t scan_phase;                       /**< Current command-three scan phase. */
    uint8_t primary_scan_sample_index;        /**< Next primary scan-filter history slot. */
    uint8_t secondary_scan_sample_index;      /**< Next secondary scan-filter history slot. */
    WheelServiceRequest request_kind;         /**< Kind of request currently held in request. */
    bool protocol_deadline_active;            /**< Whether protocol_deadline_ms is active. */
    bool protocol_transport_deadline_active;  /**< Whether the logical transport deadline is active.
                                               */
    bool protocol_exchange_completed; /**< Whether a completed command-two exchange is latched. */
    bool bridge_recovery_pending; /**< Whether unknown selection timed out into USB bridge recovery.
                                   */
    bool display_override_active; /**< Whether display_override_output owns the visible page. */
    bool alternative_shifter_enabled;   /**< Whether alternative shifter mode is enabled. */
    bool alternative_shifter_debounced; /**< Whether the current activation chord was debounced. */
    bool status_memory_startup_pending; /**< Whether mode 0x0A or 0x1C awaits status memory. */
    bool tuning_menu_override_enabled;  /**< Whether startup status overrides capability data. */
    bool tuning_menu_override_value;    /**< Startup status tuning-menu availability result. */
} WheelService;

/**
 * @brief Initializes attached-wheel service state.
 *
 * Attaches the shared serial transport and clears protocol, adapter, display, scan, rotary, and
 * scheduling state.
 *
 * @param[out] service Wheel service to initialize.
 * @param[in,out] transport Shared serial service for wheel traffic.
 */
void wheel_service_init(WheelService *service, SerialService *transport);

/**
 * @brief Resets attached-adapter command state.
 *
 * Clears incomplete adapter work and queues the retained adapter display state again.
 *
 * @param[in,out] service Wheel service whose adapter commands are reset; null is ignored.
 */
void wheel_service_reset_adapter_commands(WheelService *service);

/**
 * @brief Advances attached-wheel protocol traffic.
 *
 * Processes a completed wheel request and starts the next protocol or button-scan request when
 * scheduling permits it.
 *
 * @param[in,out] service Wheel service to advance.
 * @param[in] now_ms Current monotonic time in milliseconds.
 * @param[in] start_allowed Whether a new wheel request may start.
 */
void wheel_service_run(WheelService *service, uint32_t now_ms, bool start_allowed);

/**
 * @brief Starts an independent command-two exchange.
 *
 * Submits the current protocol response even when normal wheel input uses command-three scanning.
 *
 * @param[in,out] service Wheel service to update.
 * @param[in] now_ms Current monotonic time in milliseconds.
 * @return True when the exchange entered the serial scheduler; otherwise false.
 */
bool wheel_service_start_protocol_exchange(WheelService *service, uint32_t now_ms);

/**
 * @brief Takes the completed command-two exchange notification.
 *
 * Consumes the one-shot notification latched after a successful protocol response.
 *
 * @param[in,out] service Wheel service holding the notification.
 * @return True when a completion notification was pending; otherwise false.
 */
bool wheel_service_take_protocol_exchange_completed(WheelService *service);

/**
 * @brief Takes an unknown-selection bridge recovery notification.
 *
 * Consumes the one-shot notification raised after the official command deadline expires while the
 * wheel remains in mode-selection state.
 *
 * @param[in,out] service Wheel service holding the recovery notification.
 * @return True when bridge recovery was pending; otherwise false.
 */
bool wheel_service_take_bridge_recovery(WheelService *service);

/**
 * @brief Reports whether unknown-selection bridge recovery is pending.
 *
 * Checks the one-shot recovery notification without consuming it.
 *
 * @param[in] service Wheel service to inspect.
 * @return True while bridge recovery awaits the runtime owner; otherwise false.
 */
bool wheel_service_bridge_recovery_pending(const WheelService *service);

/**
 * @brief Reports whether selected-wheel startup is waiting for status memory.
 *
 * @param[in] service Wheel service to inspect.
 * @return True after selecting mode 0x0A or 0x1C until startup status completes.
 */
bool wheel_service_status_memory_startup_pending(const WheelService *service);

/**
 * @brief Completes selected-wheel status-memory startup.
 *
 * Releases the protocol timeout gate and applies the recovered tuning-menu availability result.
 * Calls without a pending mode-0x0A or mode-0x1C startup are ignored.
 *
 * @param[in,out] service Wheel service awaiting startup status.
 * @param[in] available Recovered tuning-menu availability.
 */
void wheel_service_finish_status_memory_startup(WheelService *service, bool available);

/**
 * @brief Advances attached-adapter command work.
 *
 * Transfers pending wheel display data to the adapter command service and advances its shared
 * command transport operations.
 *
 * @param[in,out] service Wheel service containing adapter state.
 * @param[in,out] transport Shared command transport for adapter operations.
 */
void wheel_service_run_adapter_commands(WheelService *service, CommandTransport *transport);

/**
 * @brief Takes a completed adapter host-control batch.
 *
 * Copies and consumes the retained fixed-size host-control payload from the adapter command
 * service.
 *
 * @param[in,out] service Wheel service retaining adapter command state.
 * @param[out] output Destination for the adapter host-control payload.
 * @return True when a completed payload was copied; otherwise false.
 */
bool wheel_service_take_adapter_host_controls(WheelService *service,
                                              uint8_t output[WHEEL_ADAPTER_HOST_CONTROLS_SIZE]);

/**
 * @brief Queues the adapter's remote-tuning active state.
 *
 * Retains an active state only when an adapter is connected and the wheel does not provide its own
 * tuning display; all other conditions queue an inactive state.
 *
 * @param[in,out] service Wheel service receiving the state.
 * @param[in] active Current host remote-tuning session state.
 */
void wheel_service_queue_adapter_remote_tuning_active(WheelService *service, bool active);

/**
 * @brief Queues the adapter refresh state.
 *
 * Retains the latest refresh state for transmission at the next command-transport boundary.
 *
 * @param[in,out] service Wheel service receiving the state.
 * @param[in] active Adapter refresh state.
 */
void wheel_service_queue_adapter_refresh_state(WheelService *service, bool active);

/**
 * @brief Queues an adapter setup selection.
 *
 * Retains the latest setup selection for transmission by the adapter command service.
 *
 * @param[in,out] service Wheel service receiving the selection.
 * @param[in] selection One-based adapter setup selection.
 */
void wheel_service_queue_adapter_setup_selection(WheelService *service, uint8_t selection);

/**
 * @brief Queues an adapter system display state.
 *
 * Retains and queues a nonzero adapter display state for the standard endpoint.
 *
 * @param[in,out] service Wheel service receiving the display state.
 * @param[in] state Nonzero adapter display state.
 */
void wheel_service_queue_adapter_display_state(WheelService *service, uint8_t state);

/**
 * @brief Queues a native tuning-display command.
 *
 * Accepts the command only when the directly attached wheel supports a native tuning display.
 *
 * @param[in,out] service Wheel service receiving the command.
 * @param[in] command Native tuning-display command.
 * @return True when the command was queued; otherwise false.
 */
bool wheel_service_queue_tuning_display_command(WheelService *service, uint8_t command);

/**
 * @brief Queues the native command for the selected tuning entry.
 *
 * Uses the selected entry command directly except for setup. Setup uses the profile-mode command
 * when profile mode owns slot two, the selected profile command for another manual slot, and zero
 * for the automatic slot.
 *
 * @param[in,out] service Wheel service receiving the command.
 * @param[in] entry Selected local tuning entry.
 * @param[in] bank Current tuning profile bank.
 * @param[in] profile_mode_enabled Whether Advanced profile mode is enabled.
 * @return True when the command was queued; otherwise false.
 */
bool wheel_service_queue_selected_tuning_configuration(WheelService *service, TuningEntry entry,
                                                       const TuningProfileBank *bank,
                                                       bool profile_mode_enabled);

/**
 * @brief Queues a native tuning-display notification.
 *
 * Passes the notification to the wheel output-report scheduler for later transmission.
 *
 * @param[in,out] service Wheel service receiving the notification.
 * @param[in] command Native prompt or confirmation command.
 * @return True when service is non-null and accepted the notification; otherwise false.
 */
bool wheel_service_queue_tuning_display_notification(WheelService *service, uint8_t command);

/**
 * @brief Activates a host-interface presentation.
 *
 * Retains the presentation mode and mirrors supported modes to a connected extended adapter.
 *
 * @param[in,out] service Wheel service receiving the presentation.
 * @param[in] mode Host-interface presentation mode.
 * @return True when the wheel supports tuning-display presentation; otherwise false.
 */
bool wheel_service_activate_interface_presentation(WheelService *service, uint8_t mode);

/**
 * @brief Queues one line for an extended adapter display.
 *
 * Forwards the line only while a connected adapter exposes mode-one text-page support.
 *
 * @param[in,out] service Wheel service receiving the line.
 * @param[in] line One-based display line identifier.
 * @param[in] metadata Display-line presentation metadata.
 * @param[in] text Text bytes to retain.
 * @param[in] length Number of bytes in text.
 * @return True when the adapter command service queued the line; otherwise false.
 */
bool wheel_service_queue_adapter_text_line(WheelService *service, uint8_t line, uint8_t metadata,
                                           const uint8_t *text, uint8_t length);

/**
 * @brief Queues an extended adapter text-page close record.
 *
 * Queues line identifier zero while a connected adapter exposes mode-one text-page support.
 *
 * @param[in,out] service Wheel service receiving the close record.
 * @return True when the close record was queued; otherwise false.
 */
bool wheel_service_queue_adapter_text_close(WheelService *service);
/**
 * @brief Sets the normal attached-wheel display output.
 *
 * Retains the default page and publishes it immediately unless a higher-priority page is active.
 *
 * @param[in,out] service Wheel service retaining display pages.
 * @param[in] output Complete default display output.
 */
void wheel_service_set_display_output(WheelService *service, const WheelDisplayOutput *output);

/**
 * @brief Returns the mutable default display output.
 *
 * Provides direct access to the retained normal page while temporary pages are visible.
 *
 * @param[in,out] service Wheel service retaining the default page.
 * @return Mutable default display output.
 */
WheelDisplayOutput *wheel_service_default_display_output(WheelService *service);

/**
 * @brief Sets an interaction-owned display override.
 *
 * Retains and publishes the override ahead of timed and default display pages.
 *
 * @param[in,out] service Wheel service retaining display pages.
 * @param[in] output Complete interaction display output.
 */
void wheel_service_set_display_override(WheelService *service, const WheelDisplayOutput *output);

/**
 * @brief Clears the interaction-owned display override.
 *
 * Restores the active timed page or retained default page when an override is present.
 *
 * @param[in,out] service Wheel service releasing the display override.
 */
void wheel_service_clear_display_override(WheelService *service);

/**
 * @brief Starts a timed display overlay.
 *
 * Replaces any active overlay, restarts its timer, and publishes its initial page unless an
 * interaction override is active.
 *
 * @param[in,out] service Wheel service receiving the overlay.
 * @param[in] command Command byte selecting the overlay presentation.
 * @param[in] now_ms Current monotonic time in milliseconds.
 */
void wheel_service_begin_display_overlay(WheelService *service, uint8_t command, uint32_t now_ms);

/**
 * @brief Advances the timed display overlay.
 *
 * Publishes changed overlay output unless an interaction override owns the display, and restores
 * lower-priority output when the overlay expires.
 *
 * @param[in,out] service Wheel service advancing the overlay.
 * @param[in] now_ms Current monotonic time in milliseconds.
 * @return True when visible display output changed; otherwise false.
 */
bool wheel_service_update_display_overlay(WheelService *service, uint32_t now_ms);

/**
 * @brief Reports whether the timed display overlay is active.
 *
 * Reads overlay ownership without advancing its timer.
 *
 * @param[in] service Wheel service to inspect.
 * @return True while the overlay is active; otherwise false.
 */
bool wheel_service_display_overlay_active(const WheelService *service);
/**
 * @brief Sets attached-wheel vibration output.
 *
 * Converts both vibration channel amplitudes into the shared auxiliary report representation.
 *
 * @param[in,out] service Wheel service receiving vibration output.
 * @param[in] output Two attached-wheel vibration channel amplitudes.
 */
void wheel_service_set_vibration_output(WheelService *service, const WheelVibrationOutput *output);

/**
 * @brief Sets the shared auxiliary report.
 *
 * Applies the two report bytes to vibration, alternate-packet, scan, and adapter output state.
 *
 * @param[in,out] service Wheel service receiving the report.
 * @param[in] report Two-byte auxiliary report in little-endian order.
 */
void wheel_service_set_auxiliary_report(WheelService *service, uint16_t report);

/**
 * @brief Sets the attached-wheel auxiliary output option.
 *
 * Normalizes the option to Boolean state and updates suppression of auxiliary display output for
 * the alternate packet.
 *
 * @param[in,out] service Wheel service receiving the option.
 * @param[in] option Auxiliary output option; zero enables output and any nonzero value disables it.
 */
void wheel_service_set_auxiliary_output_option(WheelService *service, uint8_t option);

/**
 * @brief Sets exclusive auxiliary output ownership.
 *
 * Enables the local tuning presentation's highest-priority auxiliary-band encoding. Disabling
 * ownership also clears transient auxiliary-band latches.
 *
 * @param[in,out] service Wheel service receiving the ownership state.
 * @param[in] enabled True while local tuning owns auxiliary output.
 */
void wheel_service_set_auxiliary_exclusive_mode(WheelService *service, bool enabled);

/**
 * @brief Sets the legacy axis bytes.
 *
 * Copies both axis bytes into each packet-family output that carries legacy axes.
 *
 * @param[in,out] service Wheel service receiving the axes.
 * @param[in] axes Two legacy-axis bytes in attached-wheel response order.
 */
void wheel_service_set_legacy_axes(WheelService *service, const uint8_t axes[2]);

/**
 * @brief Sets legacy pedal status bytes.
 *
 * Retains both status bytes for the next legacy protocol response.
 *
 * @param[in,out] service Wheel service receiving pedal status.
 * @param[in] first First pedal status byte.
 * @param[in] second Second pedal status byte.
 */
void wheel_service_set_legacy_pedal_status(WheelService *service, uint8_t first, uint8_t second);

/**
 * @brief Resets host-controlled protocol outputs.
 *
 * Clears legacy axes and auxiliary output, attempts to queue zero-valued compact reports, and
 * clears the default display page when no higher-priority display page is active and the wheel
 * does not provide a tuning display.
 *
 * @param[in,out] service Wheel service whose host outputs are reset.
 */
void wheel_service_reset_host_protocol_outputs(WheelService *service);

/**
 * @brief Sets attached adapter input state.
 *
 * Retains adapter buttons, axes, rotary positions, mode, connection state, and pending motion.
 *
 * @param[in,out] service Wheel service receiving adapter input.
 * @param[in] adapter Adapter input to retain.
 */
void wheel_service_set_adapter(WheelService *service, const WheelAdapterInput *adapter);

/**
 * @brief Sets the host capability state.
 *
 * Applies the capability to subsequent wheel responses and retains it across connection discovery.
 *
 * @param[in,out] service Wheel service receiving the capability state.
 * @param[in] enabled True to advertise the host capability.
 */
void wheel_service_set_host_capability(WheelService *service, bool enabled);

/**
 * @brief Configures attached-wheel axis processing.
 *
 * Applies interface mode, profile paddle mode, bite-point percentage, and current time to later
 * wheel input processing.
 *
 * @param[in,out] service Wheel service to configure.
 * @param[in] interface_mode Active host interface mode.
 * @param[in] paddle_mode Active profile analog-paddle mode.
 * @param[in] bite_point_percent Active profile bite-point percentage.
 * @param[in] now_ms Current monotonic time in milliseconds.
 */
void wheel_service_configure_axis_processing(WheelService *service, uint8_t interface_mode,
                                             uint8_t paddle_mode, uint8_t bite_point_percent,
                                             uint32_t now_ms);

/**
 * @brief Takes a completed bite-point adjustment.
 *
 * Consumes the adjusted active-profile percentage after the paddle adjustment gesture ends.
 *
 * @param[in,out] service Wheel service holding protocol adjustment state.
 * @param[out] updated_percent Receives the percentage to persist.
 * @return True when a completed adjustment was available; otherwise false.
 */
bool wheel_service_take_bite_point(WheelService *service, uint8_t *updated_percent);

/**
 * @brief Takes a pending bite-point report update.
 *
 * Consumes one accepted percentage change for presentation in the primary input report.
 *
 * @param[in,out] service Wheel service holding protocol report state.
 * @param[out] updated_percent Receives the percentage to publish.
 * @return True when a new percentage was available; otherwise false.
 */
bool wheel_service_take_bite_point_report(WheelService *service, uint8_t *updated_percent);

/**
 * @brief Reads the active bite-point adjustment.
 *
 * Reports the live percentage while the analog paddle remains in adjustment mode.
 *
 * @param[in] service Wheel service holding protocol adjustment state.
 * @param[out] percent Receives the current bite-point percentage.
 * @return True while adjustment is active and percent is populated; otherwise false.
 */
bool wheel_service_bite_point_adjustment(const WheelService *service, uint8_t *percent);

/**
 * @brief Queues a remote-tuning response.
 *
 * Retains the response only when its selected link matches the negotiated remote-tuning mode and
 * the response is representable by the protocol.
 *
 * @param[in,out] service Wheel service owning protocol output.
 * @param[in] response Remote-tuning link, response code, and value.
 * @return True when the response was accepted; otherwise false.
 */
bool wheel_service_queue_remote_tuning_response(WheelService *service,
                                                const RemoteTuningResponse *response);

/**
 * @brief Queues a system-owned remote-tuning response.
 *
 * Retains the response in the system-control priority slot across connection discovery when its
 * link, code, and value are supported by the protocol.
 *
 * @param[in,out] service Wheel service owning protocol output.
 * @param[in] response Semantic system-control response.
 * @return True when the response was accepted; otherwise false.
 */
bool wheel_service_queue_system_control_response(WheelService *service,
                                                 const RemoteTuningResponse *response);

/**
 * @brief Reports whether a remote-tuning response is pending.
 *
 * Checks protocol output state without consuming the queued response.
 *
 * @param[in] service Wheel service to inspect.
 * @return True when a supported response is pending; otherwise false.
 */
bool wheel_service_remote_tuning_response_pending(const WheelService *service);
/**
 * @brief Applies an auxiliary-output operating-mode command.
 *
 * Handles auxiliary option, code-mode, and two-byte auxiliary-report commands.
 *
 * @param[in,out] service Wheel service and output state to update.
 * @param[in] command Decoded F8 09 operating-mode command.
 * @return True when command selects an auxiliary-output operation; otherwise false.
 */
bool wheel_service_apply_auxiliary_output_command(WheelService *service,
                                                  const UsbOperatingModeCommand *command);

/**
 * @brief Applies a multi-position reporting command.
 *
 * Passes the host override command to the retained wheel capability state.
 *
 * @param[in,out] service Wheel service and capability state to update.
 * @param[in] command Decoded F8 09 operating-mode command.
 * @return True when command selects the multi-position override; otherwise false.
 */
bool wheel_service_apply_multi_position_command(WheelService *service,
                                                const UsbOperatingModeCommand *command);

/**
 * @brief Applies a compact output-report command.
 *
 * Routes report-two and report-one commands to the negotiated wheel output and matching adapter
 * command queue.
 *
 * @param[in,out] service Wheel service and output state to update.
 * @param[in] command Decoded F8 09 operating-mode command.
 * @return True when command selects a compact output report, including a consumed gated command;
 * otherwise false.
 */
bool wheel_service_apply_packed_report_command(WheelService *service,
                                               const UsbOperatingModeCommand *command);

/**
 * @brief Applies a report-six operating-mode command.
 *
 * Copies command parameters zero and three into the retained report-six payload and queues the
 * matching adapter write.
 *
 * @param[in,out] service Wheel service and output state to update.
 * @param[in] command Decoded F8 09 operating-mode command.
 * @return True when command carries a report-six update; otherwise false.
 */
bool wheel_service_apply_report_six_command(WheelService *service,
                                            const UsbOperatingModeCommand *command);

/**
 * @brief Applies an interface-mode gate command.
 *
 * Stores the interface-mode gate selected by operating-mode opcode 0x0E parameter zero.
 *
 * @param[in,out] service Wheel service and output state to update.
 * @param[in] command Decoded F8 09 operating-mode command.
 * @return True when command carries an interface-mode gate update; otherwise false.
 */
bool wheel_service_apply_interface_mode_command(WheelService *service,
                                                const UsbOperatingModeCommand *command);

/**
 * @brief Updates the local interface-mode gate.
 *
 * Samples the mode-specific secondary-button chord and advances its debounce state.
 *
 * @param[in,out] service Wheel service and interface-gate state to update.
 * @param[in] now_ms Current monotonic time in milliseconds.
 */
void wheel_service_update_interface_mode_gate(WheelService *service, uint32_t now_ms);

/**
 * @brief Resolves the effective multi-position reporting mode.
 *
 * Combines the configured profile mode with host override, negotiated wheel mode, and request
 * readiness.
 *
 * @param[in] service Wheel service and capability state to inspect.
 * @param[in] configured_mode Multi-position mode configured by the active profile.
 * @return Effective multi-position reporting mode, or encoder mode when service is null.
 */
uint8_t wheel_service_multi_position_mode(const WheelService *service,
                                          TuningMultiPositionMode configured_mode);

/**
 * @brief Reports whether multi-position input is supported.
 *
 * Applies negotiated wheel-mode and current input-transport rules.
 *
 * @param[in] service Wheel service and protocol state to inspect.
 * @return True when multi-position input is supported; otherwise false.
 */
bool wheel_service_multi_position_supported(const WheelService *service);

/**
 * @brief Builds the current multi-position rotary input.
 *
 * Selects direct wheel positions or adapter selectors, advances rotary debounce state, and marks
 * the extended selector layout when required. Direct packed input also advances the fourth rotary
 * channel, which is exposed separately for native legacy accessory reporting.
 *
 * @param[in,out] service Wheel service and rotary state to update.
 * @param[in] now_ms Current monotonic time in milliseconds.
 * @param[out] input Multi-position input to populate.
 * @return True when a supported wheel request is available; otherwise false.
 */
bool wheel_service_multi_position_input(WheelService *service, uint32_t now_ms,
                                        WheelMultiPositionInput *input);

/**
 * @brief Returns one debounced direct-wheel rotary event.
 *
 * Reads the latest event from the requested rotary channel without consuming it. The fourth
 * channel is populated from the high nibble of the packed direct-wheel rotary byte and is not part
 * of the three selector channels in WheelMultiPositionInput.
 *
 * @param[in] service Wheel service and rotary state to inspect.
 * @param[in] channel Zero-based rotary channel index.
 * @return Current event, or WHEEL_ROTARY_EVENT_NONE for an invalid service or channel.
 */
WheelRotaryEvent wheel_service_rotary_event(const WheelService *service, uint8_t channel);

/**
 * @brief Applies a host-provided wheel output report.
 *
 * Retains an accepted report for the negotiated wheel mode and forwards it to the corresponding
 * adapter command queue.
 *
 * @param[in,out] service Wheel service and output state to update.
 * @param[in] arguments Action byte followed by report payload.
 */
void wheel_service_apply_output_report(WheelService *service, const uint8_t *arguments);
/**
 * @brief Queues a complete report-seventeen payload.
 *
 * Retains the tuning-menu report and restarts its segmented wheel transfer.
 *
 * @param[in,out] service Wheel service owning protocol output.
 * @param[in] payload Complete report-seventeen payload.
 */
void wheel_service_queue_report_seventeen(
    WheelService *service, const uint8_t payload[WHEEL_OUTPUT_REPORT_SEVENTEEN_SIZE]);

/**
 * @brief Queues an H-pattern calibration state for the attached wheel.
 *
 * Retains the official three-byte type-0x16 shifter-state payload until the next active protocol
 * response.
 *
 * @param[in,out] service Wheel service owning protocol output.
 * @param[in] state Three-byte shifter-state payload.
 */
void wheel_service_queue_shifter_calibration_state(WheelService *service, const uint8_t state[3]);

/**
 * @brief Queues a remote telemetry report.
 *
 * Retains the payload when no earlier telemetry report is pending in the protocol output scheduler.
 *
 * @param[in,out] service Wheel service owning protocol output.
 * @param[in] payload Complete remote-telemetry payload.
 * @return True when the report was accepted; otherwise false.
 */
bool wheel_service_queue_remote_telemetry(
    WheelService *service, const uint8_t payload[WHEEL_OUTPUT_REMOTE_TELEMETRY_SIZE]);

/**
 * @brief Reports whether remote telemetry is pending.
 *
 * Checks the protocol output scheduler without consuming its queued report.
 *
 * @param[in] service Wheel service to inspect.
 * @return True while a telemetry report remains queued; otherwise false.
 */
bool wheel_service_remote_telemetry_pending(const WheelService *service);

/**
 * @brief Sets attached-wheel button illumination.
 *
 * Retains the active-profile illumination setting for compatible remote-tuning wheel modes.
 *
 * @param[in,out] service Wheel service owning output state.
 * @param[in] enabled True to enable button illumination.
 */
void wheel_service_set_button_illumination(WheelService *service, bool enabled);

/**
 * @brief Sets mode-nine display character translation.
 *
 * Retains whether later mode-nine display data is translated to protocol characters.
 *
 * @param[in,out] service Wheel service owning protocol state.
 * @param[in] enabled True to enable character translation.
 */
void wheel_service_set_display_character_mode(WheelService *service, bool enabled);

/**
 * @brief Sets legacy display rotation output.
 *
 * Retains the enable flag and signed angle for legacy remote-tuning responses.
 *
 * @param[in,out] service Wheel service owning protocol state.
 * @param[in] enabled True to include the display angle.
 * @param[in] angle Signed angle in hundredths of a degree.
 */
void wheel_service_set_display_rotation(WheelService *service, bool enabled, int16_t angle);

/**
 * @brief Queues an attached-wheel system status code.
 *
 * Retains the code across connection discovery for publication in the next active command-two
 * exchange.
 *
 * @param[in,out] service Wheel service owning protocol output.
 * @param[in] code System status code to publish.
 */
void wheel_service_queue_system_status(WheelService *service, uint16_t code);

/**
 * @brief Returns the current attached-wheel button banks.
 *
 * Selects decoded packet-family buttons or the filtered scan-mode banks.
 *
 * @param[in] service Wheel service to inspect.
 * @return Pointer to three current button bytes.
 */
const uint8_t *wheel_service_buttons(const WheelService *service);

/**
 * @brief Copies normalized attached-wheel input.
 *
 * Reads directional, secondary-button, clutch, tuning, motion, rotary, auxiliary, and axis-report
 * fields from the current supported request view. During command-three scanning, returns a
 * button-only fallback snapshot assembled from the filtered scan banks so console and tuning
 * consumers retain current button input while no thirty-byte protocol request exists.
 *
 * @param[in] service Wheel service to inspect.
 * @param[out] snapshot Normalized input destination.
 * @return True when a supported request is available and copied; otherwise false.
 */
bool wheel_service_input_snapshot(const WheelService *service, WheelInputSnapshot *snapshot);

/**
 * @brief Updates alternative-shifter mode.
 *
 * Evaluates the profile activation chord, applies mode-specific controls, debounces the chord, and
 * mirrors the resulting latch into protocol output.
 *
 * @param[in,out] service Wheel service and alternative-shifter state to update.
 * @param[in] profile_context_pending Whether profile activation context is available.
 * @param[in] now_ms Current monotonic time in milliseconds.
 * @return WHEEL_ALTERNATIVE_SHIFTER_ENABLED or WHEEL_ALTERNATIVE_SHIFTER_DISABLED when state
 * changes, otherwise WHEEL_ALTERNATIVE_SHIFTER_UNCHANGED.
 */
WheelAlternativeShifterEvent wheel_service_update_alternative_shifter(WheelService *service,
                                                                      bool profile_context_pending,
                                                                      uint32_t now_ms);
/**
 * @brief Reports whether alternative-shifter mode is enabled.
 *
 * Reads the debounced mode state without changing its activation latch.
 *
 * @param[in] service Wheel service to inspect.
 * @return True while alternative-shifter mode is enabled; otherwise false.
 */
bool wheel_service_alternative_shifter_enabled(const WheelService *service);

/**
 * @brief Returns the attached-wheel axis-limit value.
 *
 * Reads the axis-limit byte retained from the current supported input report.
 *
 * @param[in] service Wheel service to inspect.
 * @return Current axis-limit value, or zero when unavailable.
 */
uint8_t wheel_service_axis_limit(const WheelService *service);

/**
 * @brief Returns the attached-wheel mode-button byte.
 *
 * Reads the mode-button field retained from the current supported input report. During
 * command-three scanning, returns the three marker bits from the active primary or secondary scan
 * request.
 *
 * @param[in] service Wheel service to inspect.
 * @return Current mode-button byte, or zero when unavailable.
 */
uint8_t wheel_service_mode_buttons(const WheelService *service);

/**
 * @brief Returns attached-wheel clutch-paddle bytes.
 *
 * Exposes the axis-output bytes that feed the two clutch-paddle report fields.
 * Use wheel_service_clutch_paddles_available() when assembling the native Fanatec report.
 *
 * @param[in] service Wheel service to inspect.
 * @return Pointer to two clutch-paddle bytes, or null when no active input packet is available.
 */
const uint8_t *wheel_service_clutch_paddles(const WheelService *service);

/**
 * @brief Reports whether native clutch-paddle fields are available.
 *
 * Matches the official native report gate: modes 0x01, 0x02, 0x03, 0x0A, 0x13, 0x14, and 0x16
 * always expose the fields; other modes expose them only for dual analog paddles or an attached
 * adapter.
 *
 * @param[in] service Wheel service to inspect.
 * @return True when native clutch-paddle values should be copied; otherwise false.
 */
bool wheel_service_clutch_paddles_available(const WheelService *service);

/**
 * @brief Reports whether the attached wheel enables axis reporting.
 *
 * Reads the capability flag retained from the current supported wheel packet family.
 *
 * @param[in] service Wheel service to inspect.
 * @return True while the current input report enables axis output; otherwise false.
 */
bool wheel_service_axis_report_enabled(const WheelService *service);

/**
 * @brief Returns normalized attached-adapter input.
 *
 * Exposes adapter buttons, axes, rotary positions, profile flags, mode, motion, and connection
 * state retained by the wheel protocol.
 *
 * @param[in] service Wheel service to inspect.
 * @return Pointer to retained adapter input.
 */
const WheelAdapterInput *wheel_service_adapter(const WheelService *service);

/**
 * @brief Returns the retained protocol callback report identifier.
 *
 * Maps the startup-selected adapter endpoint to report identifier 0x15 or 0x16. Returns zero only
 * when the wheel service is unavailable or its endpoint selection is invalid.
 *
 * @param[in] service Wheel service containing startup endpoint state.
 * @return Retained protocol callback report identifier, or zero when unavailable.
 */
uint8_t wheel_service_protocol_bridge_report_id(const WheelService *service);

/**
 * @brief Copies attached-wheel axis values.
 *
 * Copies the two 16-bit axis values retained from the current supported packet-family report.
 *
 * @param[in] service Wheel service to inspect.
 * @param[out] values Destination for two axis values.
 * @return True when axis values were available; otherwise false.
 */
bool wheel_service_axis_values(const WheelService *service, uint16_t values[2]);

/**
 * @brief Returns attached-wheel pedal-axis overrides.
 *
 * Exposes the four logical override channels produced by the current wheel axis mode.
 *
 * @param[in] service Wheel service to inspect.
 * @return Pointer to retained pedal and auxiliary override channels.
 */
const WheelAxisOverrides *wheel_service_axis_overrides(const WheelService *service);

/**
 * @brief Copies normalized attached-wheel controls.
 *
 * Copies the eight control bytes retained from the current supported packet-family input report.
 *
 * @param[in] service Wheel service to inspect.
 * @param[out] controls Destination for eight control bytes.
 * @return True when controls were available; otherwise false.
 */
bool wheel_service_controls(const WheelService *service, uint8_t controls[8]);

/**
 * @brief Reports whether extended wheel fields feed host input.
 *
 * Applies authenticated CRC and adapter-mode suppression rules to the current request state.
 *
 * @param[in] service Wheel service and adapter state to inspect.
 * @return True when extended fields contribute to the host input report; otherwise false.
 */
bool wheel_service_extended_report_fields(const WheelService *service);

/**
 * @brief Returns attached-wheel accessory flags.
 *
 * Reads the low-nibble accessory field from the current normalized request view.
 *
 * @param[in] service Wheel service to inspect.
 * @return Current accessory flags, or zero when input is unavailable.
 */
uint8_t wheel_service_accessory_flags(const WheelService *service);

/**
 * @brief Returns queued encoder direction.
 *
 * Inspects motion accumulated by valid wheel reports without consuming it.
 *
 * @param[in] service Wheel service to inspect.
 * @return Negative one, zero, or positive one.
 */
int8_t wheel_service_encoder_direction(const WheelService *service);

/**
 * @brief Takes one queued encoder step.
 *
 * Consumes one signed step from motion accumulated by valid wheel reports.
 *
 * @param[in,out] service Wheel service whose motion is consumed.
 * @return Negative one, zero, or positive one.
 */
int8_t wheel_service_take_encoder_step(WheelService *service);

/**
 * @brief Returns queued motion direction for one axis.
 *
 * Inspects the selected protocol motion counter without consuming it.
 *
 * @param[in] service Wheel service to inspect.
 * @param[in] axis Zero-based auxiliary motion-axis index.
 * @return Negative one, zero, or positive one; zero for an unsupported axis.
 */
int8_t wheel_service_axis_motion_direction(const WheelService *service, uint8_t axis);

/**
 * @brief Takes one queued motion step for one axis.
 *
 * Consumes one signed step from the selected protocol motion counter.
 *
 * @param[in,out] service Wheel service whose motion is consumed.
 * @param[in] axis Zero-based auxiliary motion-axis index.
 * @return Negative one, zero, or positive one; zero for an unsupported axis.
 */
int8_t wheel_service_take_axis_motion(WheelService *service, uint8_t axis);

/**
 * @brief Takes retained remote-tuning controls.
 *
 * Copies and consumes the latest fixed-size remote-tuning control payload.
 *
 * @param[in,out] service Wheel service retaining the control payload.
 * @param[out] output Destination for the 30-byte control payload.
 * @return True when a pending payload was copied; otherwise false.
 */
bool wheel_service_take_remote_tuning_controls(WheelService *service, uint8_t output[30]);

/**
 * @brief Discards host-visible wheel motion.
 *
 * Clears protocol motion and rotary transition state so tuning navigation cannot emit delayed host
 * input.
 *
 * @param[in,out] service Wheel service whose motion state is cleared; null is ignored.
 */
void wheel_service_discard_host_motion(WheelService *service);
/**
 * @brief Reports whether eligible wheel input is active.
 *
 * Checks packet-family directional, button, and auxiliary input or filtered scan-mode button banks.
 *
 * @param[in] service Wheel service to inspect.
 * @return True while eligible input is active; otherwise false.
 */
bool wheel_service_acknowledgement_input_active(const WheelService *service);

/**
 * @brief Reports whether calibration-advance input is active.
 *
 * Selects adapter or mode-specific wheel button fields for the H-pattern calibration action.
 *
 * @param[in] service Wheel service to inspect.
 * @return True while the calibration-advance input is active; otherwise false.
 */
bool wheel_service_calibration_advance_input_active(const WheelService *service);

/**
 * @brief Reports the protocol button used to complete H-pattern calibration.
 *
 * Selects the protocol-only button mapping for local modes 0x0E, 0x0F, and 0x17 versus every
 * other mode. Adapter advance buttons are intentionally excluded from this completion input.
 *
 * @param[in] service Wheel service to inspect.
 * @return True while the protocol completion button is active; otherwise false.
 */
bool wheel_service_calibration_completion_input_active(const WheelService *service);

/**
 * @brief Reports whether an attached adapter is connected.
 *
 * Reads the adapter connection state retained by the wheel protocol.
 *
 * @param[in] service Wheel service to inspect.
 * @return True while an adapter is connected; otherwise false.
 */
bool wheel_service_adapter_connected(const WheelService *service);

/**
 * @brief Reports whether the adapter requests host capability.
 *
 * Requires adapter connection and the high status bit in its second button byte.
 *
 * @param[in] service Wheel service to inspect.
 * @return True when the adapter-specific capability condition is active; otherwise false.
 */
bool wheel_service_adapter_requests_host_capability(const WheelService *service);

/**
 * @brief Returns attached-wheel capability flags.
 *
 * Reads the capability word assembled from the current attached-wheel report.
 *
 * @param[in] service Wheel service to inspect.
 * @return Current attached-wheel capability flags.
 */
uint16_t wheel_service_capability_flags(const WheelService *service);

/**
 * @brief Reports whether host capability is enabled.
 *
 * Reads the persistent capability state applied to attached-wheel responses.
 *
 * @param[in] service Wheel service to inspect.
 * @return True after host enable and until disable or reset; otherwise false.
 */
bool wheel_service_host_capability_enabled(const WheelService *service);

/**
 * @brief Reports whether attached-wheel calibration is available.
 *
 * Reads effective calibration capability after negotiated mode rules are applied.
 *
 * @param[in] service Wheel service to inspect.
 * @return True when calibration is available; otherwise false.
 */
bool wheel_service_calibration_available(const WheelService *service);

/**
 * @brief Reports whether Torque Key acknowledgement can grant full strength.
 *
 * Blocks acknowledgement while scan polling or wheel calibration controls are active.
 *
 * @param[in] service Wheel service and capability state to inspect.
 * @return True when Torque Key acknowledgement is available; otherwise false.
 */
bool wheel_service_torque_key_acknowledgement_available(const WheelService *service);

/**
 * @brief Reports whether the attached wheel exposes a tuning menu.
 *
 * Applies negotiated mode and retained capability rules to tuning-menu availability.
 *
 * @param[in] service Wheel service and capability state to inspect.
 * @return True when tuning-menu operation is available; otherwise false.
 */
bool wheel_service_tuning_menu_available(const WheelService *service);

/**
 * @brief Reports whether the attached wheel provides a tuning display.
 *
 * Applies negotiated wheel mode and connected adapter mode to tuning-display availability.
 *
 * @param[in] service Wheel service and adapter state to inspect.
 * @return True when tuning-display presentation is supported; otherwise false.
 */
bool wheel_service_tuning_display_supported(const WheelService *service);

/**
 * @brief Reports whether attached-wheel input capability is available.
 *
 * Applies negotiated mode eligibility to the retained input-capability latch.
 *
 * @param[in] service Wheel service and capability state to inspect.
 * @return True when the current wheel mode exposes input capability; otherwise false.
 */
bool wheel_service_input_capability_available(const WheelService *service);

/**
 * @brief Reports whether force-output mode selection is complete.
 *
 * Accepts authenticated, active, and either command-three scan phase as ready for force-output
 * confirmation.
 *
 * @param[in] service Wheel service and protocol phase to inspect.
 * @return True when force-output confirmation may proceed; otherwise false.
 */
bool wheel_service_force_output_ready(const WheelService *service);

/**
 * @brief Reports whether the force-output transition interlock is active.
 *
 * Reads whether the wheel remains in negotiation or has an invalid active command.
 *
 * @param[in] service Wheel service and protocol phase to inspect.
 * @return True while negotiation or an invalid active command requires the transition interlock.
 */
bool wheel_service_force_output_transition_active(const WheelService *service);

/**
 * @brief Returns the negotiated attached-wheel mode.
 *
 * Reads the mode selected by the wheel protocol handshake.
 *
 * @param[in] service Wheel service to inspect.
 * @return Current attached-wheel mode identifier.
 */
uint8_t wheel_service_mode(const WheelService *service);

/**
 * @brief Returns the attached-wheel protocol phase.
 *
 * Reads the current handshake or active-traffic phase.
 *
 * @param[in] service Wheel service to inspect.
 * @return Current attached-wheel protocol phase.
 */
WheelProtocolPhase wheel_service_protocol_phase(const WheelService *service);

#endif
