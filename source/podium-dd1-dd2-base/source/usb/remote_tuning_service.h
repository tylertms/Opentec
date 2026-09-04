#ifndef OPENTEC_BASE_USB_REMOTE_TUNING_SERVICE_H
#define OPENTEC_BASE_USB_REMOTE_TUNING_SERVICE_H

#include <stdbool.h>
#include <stdint.h>

#include "remote_tuning/response.h"
#include "remote_tuning/telemetry.h"
#include "usb/remote_tuning_records.h"

/** @brief Remote-tuning service limits and host-report sizes. */
enum {
    USB_REMOTE_TUNING_SESSION_TIMEOUT_MS =
        60000, /**< Session deadline extension applied to each valid packet, in milliseconds. */
    USB_REMOTE_TUNING_HOST_REPORT_SIZE =
        64, /**< Size of a host telemetry control report, in bytes. */
};

/**
 * @brief Host transport framing used for telemetry subscription reports.
 *
 * Each value selects the marker byte required by one host transport.
 */
typedef enum {
    USB_REMOTE_TUNING_HOST_NATIVE,      /**< Native USB transport framing. */
    USB_REMOTE_TUNING_HOST_PLAYSTATION, /**< PlayStation USB transport framing. */
    USB_REMOTE_TUNING_HOST_XBOX,        /**< Xbox USB transport framing. */
} UsbRemoteTuningHost;

/** @brief Intelligent-telemetry-mode page dimensions. */
enum {
    USB_REMOTE_TUNING_ITM_SET_COUNT = 10,  /**< Number of official ITM selection sets. */
    USB_REMOTE_TUNING_ITM_FIELD_COUNT = 7, /**< Maximum fields retained for one page. */
    USB_REMOTE_TUNING_ITM_TEXT_SIZE = 16,  /**< Storage bytes for each field value. */
};

/** @brief Retained intelligent-telemetry-mode page supplied by remote tuning. */
typedef struct {
    char values[USB_REMOTE_TUNING_ITM_FIELD_COUNT]
               [USB_REMOTE_TUNING_ITM_TEXT_SIZE]; /**< Primary field text. */
    char secondary_values[USB_REMOTE_TUNING_ITM_FIELD_COUNT]
                         [USB_REMOTE_TUNING_ITM_TEXT_SIZE]; /**< Secondary field text. */
    bool markers[USB_REMOTE_TUNING_ITM_FIELD_COUNT];        /**< Per-field marker state. */
    uint8_t page;                                           /**< Selected one-based ITM set. */
    uint8_t field_count;                                    /**< Number of valid retained fields. */
    uint8_t revision; /**< Revision incremented by accepted page updates. */
} UsbRemoteTuningItmPage;

/**
 * @brief Host remote-tuning session and retained downstream work.
 *
 * The service owns decoded records, telemetry subscriptions, selection latches, physical-input
 * history, and responses waiting for attached-wheel or adapter transport.
 */
typedef struct {
    UsbRemoteTuningRecords records; /**< Retained remote-tuning records awaiting routing. */
    RemoteTelemetry
        telemetry; /**< Selected telemetry metric, dynamic reports, and host-control queue. */
    uint32_t
        session_deadline_ms;  /**< Monotonic deadline extended by accepted remote-tuning packets. */
    uint16_t encoder_counter; /**< Current physical encoder selection counter. */
    uint8_t
        command_type; /**< Current selection command kind used for pending telemetry selection. */
    uint8_t setup_selection; /**< One-based setup selection pending for the adapter. */
    uint8_t menu_selection;  /**< One-based menu selection retained from the host. */
    uint8_t
        multi_position_selection; /**< One-based multi-position selection retained from the host. */
    uint8_t setup_index;          /**< One-based extended setup index received from the host. */
    uint8_t setup_page;           /**< Current setup page value selected for extended navigation. */
    uint8_t encoder_selection;    /**< Most recently selected legacy encoder value. */
    int8_t physical_previous_input;    /**< Previous signed physical tuning input. */
    uint8_t physical_button_flags;     /**< Previous masked standard-wheel navigation buttons. */
    uint8_t physical_rotary_position;  /**< Previous physical rotary position. */
    uint8_t physical_navigation_input; /**< Previous setup-navigation motion code. */
    RemoteTuningResponse pending_response; /**< Response waiting for attached-wheel transmission. */
    bool active;                  /**< True while the host remote-tuning session is active. */
    bool refresh_requested;       /**< True while an adapter refresh request awaits promotion. */
    bool adapter_refresh_state;   /**< Last refresh state promoted for the adapter. */
    bool active_sync_pending;     /**< True while active state awaits adapter synchronization. */
    bool setup_sync_pending;      /**< True while setup selection awaits adapter synchronization. */
    bool refresh_sync_pending;    /**< True while refresh state awaits adapter synchronization. */
    bool physical_input_released; /**< True when a standard-wheel button edge can be consumed; the
                                       initial state accepts the first sampled press. */
    bool
        physical_rotary_initialized; /**< True after a physical rotary position has been sampled. */
    UsbRemoteTuningItmPage itm_page; /**< Retained intelligent-telemetry-mode page. */
} UsbRemoteTuningService;

/**
 * @brief Initializes the host remote-tuning service.
 *
 * Clears the session, retained records, telemetry mappings, pending responses, selection latches,
 * synchronization latches, and physical-input history. The standard-wheel edge latch starts in
 * the released state so the first sampled navigation press is consumed immediately.
 *
 * @param[out] service Service state to initialize.
 */
void usb_remote_tuning_service_init(UsbRemoteTuningService *service);

/**
 * @brief Applies one host remote-tuning packet.
 *
 * Extends the session deadline, retains record packets, and applies active, selection, or refresh
 * state when the command contains the required arguments. The command is claimed for a valid
 * remote-tuning vendor command, including unknown packet types.
 *
 * @param[in,out] service Service state receiving the packet.
 * @param[in] command Decoded remote-tuning vendor command.
 * @param[in] now_ms Current monotonic time in milliseconds.
 * @param[in] wheel_mode Current attached-wheel mode.
 * @param[in] setup_selection_allowed Whether extended setup selection is currently allowed.
 * @param[in] adapter_connected Whether an attached adapter is connected.
 * @return True when the command uses the remote-tuning vendor route; otherwise false.
 */
bool usb_remote_tuning_service_apply(UsbRemoteTuningService *service,
                                     const UsbVendorCommand *command, uint32_t now_ms,
                                     uint8_t wheel_mode, bool setup_selection_allowed,
                                     bool adapter_connected);

/**
 * @brief Takes the next attached-wheel remote-tuning response.
 *
 * Returns a pending state response before taking the next bounded record response for the selected
 * legacy or extended wheel mode.
 *
 * @param[in,out] service Service state containing pending responses and records.
 * @param[in] wheel_mode Current attached-wheel mode.
 * @param[out] response Destination for the response.
 * @return True when a response was taken; otherwise false.
 */
bool usb_remote_tuning_service_take_response(UsbRemoteTuningService *service, uint8_t wheel_mode,
                                             RemoteTuningResponse *response);

/**
 * @brief Returns the retained intelligent-telemetry-mode page.
 *
 * @param[in] service Remote-tuning service to inspect.
 * @return Retained page, or null when service is null.
 */
const UsbRemoteTuningItmPage *
usb_remote_tuning_service_itm_page(const UsbRemoteTuningService *service);

/**
 * @brief Takes pending active state for adapter synchronization.
 *
 * Returns the active state and clears its synchronization latch only when synchronization is both
 * allowed by the caller and pending in the service.
 *
 * @param[in,out] service Service state retaining active synchronization.
 * @param[in] synchronization_allowed Whether the caller permits synchronization.
 * @param[out] active Destination for the current remote-tuning active state.
 * @return True when pending active state was taken; otherwise false.
 */
bool usb_remote_tuning_service_take_adapter_active(UsbRemoteTuningService *service,
                                                   bool synchronization_allowed, bool *active);

/**
 * @brief Takes pending refresh state for adapter synchronization.
 *
 * Promotes a requested refresh when one is pending, returns the retained adapter refresh state,
 * and clears the refresh synchronization latch.
 *
 * @param[in,out] service Service state retaining refresh synchronization.
 * @param[out] active Destination for the adapter refresh state.
 * @return True when pending refresh state was taken; otherwise false.
 */
bool usb_remote_tuning_service_take_adapter_refresh_state(UsbRemoteTuningService *service,
                                                          bool *active);

/**
 * @brief Takes pending setup selection for adapter synchronization.
 *
 * Returns the retained one-based setup selection and clears the setup, menu, and multi-position
 * selection latches.
 *
 * @param[in,out] service Service state retaining setup synchronization.
 * @param[out] selection Destination for the one-based setup selection.
 * @return True when pending setup selection was taken; otherwise false.
 */
bool usb_remote_tuning_service_take_adapter_setup_selection(UsbRemoteTuningService *service,
                                                            uint8_t *selection);

/**
 * @brief Queues adapter-originated telemetry controls for the host.
 *
 * Splits the adapter report into five-byte records, ignores records with zero first and second
 * bytes, and queues every remaining record that fits the telemetry control queue.
 *
 * @param[in,out] service Service state owning the telemetry control queue.
 * @param[in] input Adapter telemetry control report.
 * @return Number of records queued.
 */
uint8_t
usb_remote_tuning_service_queue_host_controls(UsbRemoteTuningService *service,
                                              const uint8_t input[REMOTE_TELEMETRY_REPORT_SIZE]);

/**
 * @brief Takes a generic attached-device command batch.
 *
 * Serializes route-three records unless extended remote-tuning mode reserves them for its wheel
 * response channel.
 *
 * @param[in,out] service Service state containing retained records.
 * @param[in] wheel_mode Current attached-wheel mode.
 * @param[out] output Destination for serialized records.
 * @param[out] length Number of serialized bytes written.
 * @return True when a nonempty batch was taken; otherwise false.
 */
bool usb_remote_tuning_service_take_forward_batch(
    UsbRemoteTuningService *service, uint8_t wheel_mode,
    uint8_t output[USB_REMOTE_TUNING_FORWARD_BATCH_SIZE], uint8_t *length);

/**
 * @brief Takes one host telemetry subscription report.
 *
 * Applies pending telemetry selection, encodes the selected host marker and report header, and
 * consumes queued five-byte control records into the 64-byte output.
 *
 * @param[in,out] service Service state containing telemetry controls.
 * @param[in] wheel_mode Current attached-wheel mode.
 * @param[in] host Host transport framing to encode.
 * @param[out] output Destination for the complete host report.
 * @return True when at least one control record was encoded; otherwise false.
 */
bool usb_remote_tuning_service_take_host_report(UsbRemoteTuningService *service, uint8_t wheel_mode,
                                                UsbRemoteTuningHost host,
                                                uint8_t output[USB_REMOTE_TUNING_HOST_REPORT_SIZE]);

/**
 * @brief Takes the next locally generated telemetry report.
 *
 * Applies pending selection, consumes route-two records, and emits a dirty telemetry report when
 * the session is active and the current wheel mode is not extended remote tuning.
 *
 * @param[in,out] service Service state containing records and telemetry.
 * @param[in] wheel_mode Current attached-wheel mode.
 * @param[out] output Destination for the complete telemetry report.
 * @return True when a telemetry report was emitted; otherwise false.
 */
bool usb_remote_tuning_service_take_telemetry_report(UsbRemoteTuningService *service,
                                                     uint8_t wheel_mode,
                                                     uint8_t output[REMOTE_TELEMETRY_REPORT_SIZE]);

/**
 * @brief Updates telemetry selection from physical controls.
 *
 * While remote tuning, profile presentation, and tuning display support are active outside
 * extended wheel mode, consumes tuning-input or standard-wheel navigation edges to change the
 * selected metric. An inactive session requests clearing the selected metric.
 *
 * @param[in,out] service Service state containing telemetry selection and input history.
 * @param[in] wheel_mode Current attached-wheel mode.
 * @param[in] profile_mode Whether the wheel currently presents the profile context.
 * @param[in] tuning_display_supported Whether the wheel supports tuning presentation.
 * @param[in] tuning_input Signed tuning-control input.
 * @param[in] auxiliary_buttons Current standard-wheel navigation buttons.
 * @return True when the selected metric changed; otherwise false.
 */
bool usb_remote_tuning_service_update_physical_selection(UsbRemoteTuningService *service,
                                                         uint8_t wheel_mode, bool profile_mode,
                                                         bool tuning_display_supported,
                                                         int8_t tuning_input,
                                                         uint8_t auxiliary_buttons);

/**
 * @brief Updates legacy telemetry selection from a physical rotary encoder.
 *
 * Detects changed encoder positions, wraps the one-based selection counter, and queues a setup
 * response while legacy remote tuning is active.
 *
 * @param[in,out] service Service state containing encoder selection and position history.
 * @param[in] wheel_mode Current attached-wheel mode.
 * @param[in] rotary_position Current physical rotary position.
 * @return True when a changed position queued a selection response; otherwise false.
 */
bool usb_remote_tuning_service_update_legacy_encoder(UsbRemoteTuningService *service,
                                                     uint8_t wheel_mode, uint8_t rotary_position);

/**
 * @brief Updates extended setup-page navigation from physical motion.
 *
 * Latches each motion edge, wraps the setup-page range, and queues the corresponding page response
 * while extended wheel mode presents the profile context.
 *
 * @param[in,out] service Service state containing setup page and motion history.
 * @param[in] wheel_mode Current attached-wheel mode.
 * @param[in] profile_mode Whether the wheel currently presents the profile context.
 * @param[in] motion Current setup-navigation motion code.
 * @return True when the setup page changed and a response was queued; otherwise false.
 */
bool usb_remote_tuning_service_update_setup_navigation(UsbRemoteTuningService *service,
                                                       uint8_t wheel_mode, bool profile_mode,
                                                       uint8_t motion);

#endif
