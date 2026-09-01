#ifndef OPENTEC_BASE_WHEEL_PROTOCOL_H
#define OPENTEC_BASE_WHEEL_PROTOCOL_H

#include <stdbool.h>
#include <stdint.h>

#include "wheel/authentication.h"
#include "wheel/axis_override.h"
#include "wheel/capability.h"
#include "wheel/motion.h"
#include "wheel/output_reports.h"
#include "wheel/packet_adapter.h"
#include "wheel/packet_alternate.h"
#include "wheel/packet_axis_mode.h"
#include "wheel/packet_crc.h"
#include "wheel/packet_display.h"
#include "wheel/packet_extended.h"
#include "wheel/packet_metadata.h"
#include "wheel/packet_mode_four.h"
#include "wheel/packet_mode_one.h"
#include "wheel/packet_packed.h"
#include "wheel/packet_remapped.h"
#include "wheel/packet_remote_tuning.h"
#include "wheel/pulse_gate.h"

/** @brief Attached-wheel protocol dimensions, offsets, commands, and modes. */
enum {
    WHEEL_PROTOCOL_PACKET_SIZE = 57,  /**< Complete request or response size in bytes. */
    WHEEL_PROTOCOL_CONTENT_SIZE = 32, /**< Checksummed content size in bytes. */
    WHEEL_PROTOCOL_INTERFACE_MODE_GATE_OFFSET =
        31,                                      /**< Request offset of the interface gate byte. */
    WHEEL_PROTOCOL_SNAPSHOT_SIZE = 30,           /**< Normalized input snapshot size in bytes. */
    WHEEL_PROTOCOL_CHECKSUM_OFFSET = 32,         /**< Packet offset of the checksum byte. */
    WHEEL_PROTOCOL_FLAGS_OFFSET = 56,            /**< Packet offset of response flags. */
    WHEEL_PROTOCOL_REQUEST_READY = 0x02,         /**< Request flag indicating a ready exchange. */
    WHEEL_PROTOCOL_RESPONSE_ACKNOWLEDGED = 0x01, /**< Response flag acknowledging a request. */
    WHEEL_PROTOCOL_HOST_CAPABILITY = 0x40,       /**< Response flag advertising host capability. */
    WHEEL_PROTOCOL_COMMAND_SELECT_MODE = 0xa5,   /**< Select-mode command byte. */
    WHEEL_PROTOCOL_COMMAND_AUTHENTICATE = 0xa6,  /**< Authenticated exchange command byte. */
    WHEEL_PROTOCOL_COMMAND_AUTHENTICATE_REPLY = 0xa7, /**< Authentication reply command byte. */
    WHEEL_PROTOCOL_COMMAND_SCAN_PRIMARY = 0xc1,       /**< Primary scan command byte. */
    WHEEL_PROTOCOL_COMMAND_SCAN_SECONDARY = 0x81,     /**< Secondary scan command byte. */
    WHEEL_MODE_UNKNOWN = 0x00,                        /**< No negotiated wheel mode. */
    WHEEL_MODE_SCAN_PRIMARY = 0x07,                   /**< Primary display-scan mode. */
    WHEEL_MODE_SCAN_SECONDARY = 0x08,                 /**< Secondary display-scan mode. */
    WHEEL_MODE_REMOTE_TUNING_LEGACY = 0x0e,           /**< Legacy remote-tuning mode. */
    WHEEL_MODE_LEGACY_ALTERNATE = 0x0f,               /**< Legacy alternate mode. */
    WHEEL_MODE_CRC_AUTHENTICATED = 0x15,              /**< CRC-authenticated mode. */
    WHEEL_MODE_LEGACY_COMPATIBILITY = 0x17,           /**< Legacy compatibility mode. */
    WHEEL_MODE_FILTERED_PULSE = 0x18,                 /**< Filtered pulse mode. */
    WHEEL_MODE_REMOTE_TUNING_EXTENDED = 0x1c,         /**< Extended remote-tuning mode. */
    WHEEL_MODE_MAXIMUM = 0x1e, /**< Highest mode value accepted by the protocol. */
};

/** @brief Handshake and active phases of the attached-wheel protocol. */
typedef enum {
    WHEEL_PROTOCOL_WAITING,            /**< Waiting for a ready request. */
    WHEEL_PROTOCOL_SYNCHRONIZING,      /**< Synchronizing the request stream. */
    WHEEL_PROTOCOL_ACKNOWLEDGING,      /**< Sending a handshake acknowledgement. */
    WHEEL_PROTOCOL_SELECTING,          /**< Processing a wheel-mode selection. */
    WHEEL_PROTOCOL_AUTHENTICATING,     /**< Waiting for or processing authentication. */
    WHEEL_PROTOCOL_ACTIVE,             /**< Processing active wheel traffic. */
    WHEEL_PROTOCOL_UNSUPPORTED,        /**< Selected wheel mode is unsupported. */
    WHEEL_PROTOCOL_SCANNING_PRIMARY,   /**< Scanning the primary display. */
    WHEEL_PROTOCOL_SCANNING_SECONDARY, /**< Scanning the secondary display. */
} WheelProtocolPhase;

/** @brief Complete state for one attached-wheel protocol session. */
typedef struct {
    uint8_t response[WHEEL_PROTOCOL_PACKET_SIZE];       /**< Current response packet. */
    uint8_t request[WHEEL_PROTOCOL_SNAPSHOT_SIZE];      /**< Current normalized request snapshot. */
    WheelAuthentication authentication;                 /**< Authentication state. */
    WheelCapabilityState capabilities;                  /**< Retained wheel capabilities. */
    WheelMotion motion;                                 /**< Queued primary and auxiliary motion. */
    WheelAxisOverrideProcessor axis_override_processor; /**< Axis override state. */
    WheelPacketModeOneButtonFilter mode_one_button_filter; /**< Mode-one button filter. */
    WheelPacketModeOneControlAxisFilter mode_one_control_axis_filter; /**< Mode-one axis filter. */
    WheelPacketModeOneInput mode_one_input;              /**< Current mode-one input. */
    WheelPacketModeOneReportState mode_one_report_state; /**< Retained mode-one report fields. */
    WheelPacketModeOneOutput mode_one_output;            /**< Mode-one response output. */
    WheelPacketModeFourFilter mode_four_filter;          /**< Mode-four input filter. */
    WheelPacketModeFourInput mode_four_input;            /**< Current mode-four input. */
    WheelPacketModeFourRuntime mode_four_runtime;        /**< Mode-four runtime latches. */
    WheelPacketModeFourOutput mode_four_output;          /**< Mode-four response output. */
    WheelPacketDisplayFilter display_filter;             /**< Standard display-packet filter. */
    WheelPacketDisplayInput display_input;               /**< Current standard display input. */
    WheelPacketRemappedFilter remapped_filter;           /**< Remapped-packet filter. */
    WheelPacketRemappedInput remapped_input;             /**< Current remapped input. */
    WheelPacketAlternateFilter alternate_filter;         /**< Alternate-packet filter. */
    WheelPacketAlternateInput alternate_input;           /**< Current alternate input. */
    WheelPacketAlternateOutput alternate_output;         /**< Alternate response output. */
    WheelPacketPackedFilter packed_filter;               /**< Packed-packet filter. */
    WheelPacketPackedInput packed_input;                 /**< Current packed input. */
    WheelPacketCommonFilter common_filter;               /**< Common-packet filter. */
    WheelPacketCommonInput common_input;                 /**< Current common input. */
    WheelPacketExtendedPulseState extended_pulse_state;  /**< Extended pulse state. */
    WheelPacketAdapterOutput adapter_output;             /**< Adapter response output. */
    WheelPacketCrcFilter crc_filter;                     /**< CRC-packet filter. */
    WheelPacketCrcInput crc_input;                       /**< Current CRC input. */
    WheelPacketCrcOutput crc_output;                     /**< CRC response output. */
    WheelAdapterInput adapter;                           /**< Current attached-adapter state. */
    WheelPacketRemoteTuningOutput system_control_output; /**< Pending system-control response. */
    WheelPacketRemoteTuningOutput remote_tuning_output;  /**< Pending remote-tuning response. */
    WheelOutputReports output_reports;     /**< Pending attached-wheel output reports. */
    WheelProtocolPhase phase;              /**< Current protocol phase. */
    uint32_t now_ms;                       /**< Current monotonic time in milliseconds. */
    WheelPulseGate pulse_gate;             /**< Interface pulse timing state. */
    uint8_t mode;                          /**< Negotiated attached-wheel mode. */
    uint8_t interface_mode;                /**< Active host interface mode. */
    uint8_t configured_axis_override_mode; /**< Configured axis override mode. */
    uint8_t paddle_bite_point_percent;     /**< Current analog-paddle bite point. */
    uint8_t system_status_code;            /**< Pending system status code. */
    uint8_t legacy_pedal_status[2];        /**< Two legacy pedal status bytes. */
    uint8_t remote_tuning_controls[30];    /**< Retained remote-tuning control payload. */
    int16_t display_rotation_angle;        /**< Current display rotation angle. */
    bool button_latch_enabled;             /**< Whether packet button latching is enabled. */
    bool display_character_mode;           /**< Whether mode-nine glyphs become characters. */
    bool display_rotation_enabled;         /**< Whether display rotation is enabled. */
    bool host_capability_enabled;          /**< Whether host capability is advertised. */
    bool profile_transition_pending;   /**< Whether a profile transition is suppressing latches. */
    bool system_status_pending;        /**< Whether a system status code awaits publication. */
    bool request_ready;                /**< Whether the current request passed ready handling. */
    bool request_changed;              /**< Whether the normalized request changed. */
    bool acknowledgement_input_active; /**< Whether input can acknowledge a display overlay. */
    bool remote_tuning_controls_pending; /**< Whether control payload is ready to take. */
} WheelProtocol;

/**
 * @brief Initializes attached-wheel protocol state.
 *
 * Clears packet-family state, output queues, and handshake fields, then starts in the waiting
 * phase with an unknown wheel mode.
 *
 * @param[out] protocol Protocol state to initialize.
 */
void wheel_protocol_init(WheelProtocol *protocol);

/**
 * @brief Sets standard packet-family response output.
 *
 * Replaces the display, vibration, and legacy-axis values used by mode-one responses.
 *
 * @param[in,out] protocol Protocol state to update.
 * @param[in] output Mode-one response output.
 */
void wheel_protocol_set_mode_one_output(WheelProtocol *protocol,
                                        const WheelPacketModeOneOutput *output);

/**
 * @brief Sets mode-four response output.
 *
 * Replaces the display, vibration, and legacy-axis values used by mode-four responses.
 *
 * @param[in,out] protocol Protocol state to update.
 * @param[in] output Mode-four response output.
 */
void wheel_protocol_set_mode_four_output(WheelProtocol *protocol,
                                         const WheelPacketModeFourOutput *output);

/**
 * @brief Sets CRC-family response output.
 *
 * Replaces the display, vibration, legacy-axis, motor-link restart, and report-status values used
 * by CRC responses.
 *
 * @param[in,out] protocol Protocol state to update.
 * @param[in] output CRC-family response output.
 */
void wheel_protocol_set_crc_output(WheelProtocol *protocol, const WheelPacketCrcOutput *output);

/**
 * @brief Selects host capability advertisement.
 *
 * Retains whether the host-controlled capability is enabled for subsequent wheel responses.
 *
 * @param[in,out] protocol Protocol state to update.
 * @param[in] enabled True to advertise host capability.
 */
void wheel_protocol_set_host_capability(WheelProtocol *protocol, bool enabled);

/**
 * @brief Sets the protocol response acknowledgement flag.
 *
 * Updates the acknowledgement bit while preserving the other response flags.
 *
 * @param[in,out] protocol Protocol state and response packet to update.
 * @param[in] acknowledged True to set the flag; false to clear it.
 */
void wheel_protocol_set_response_acknowledged(WheelProtocol *protocol, bool acknowledged);

/**
 * @brief Sets attached-adapter input state.
 *
 * Copies adapter buttons, axes, selectors, connection state, and queued motion into the protocol.
 *
 * @param[in,out] protocol Protocol state to update.
 * @param[in] adapter Adapter input state to copy.
 */
void wheel_protocol_set_adapter(WheelProtocol *protocol, const WheelAdapterInput *adapter);

/**
 * @brief Queues a remote-tuning response for the active wheel.
 *
 * Accepts a response only when its remote-tuning link matches the negotiated legacy or extended
 * wheel mode.
 *
 * @param[in,out] protocol Protocol state owning the response queue.
 * @param[in] response Semantic remote-tuning response to queue.
 * @return True when the response was accepted; otherwise false.
 */
bool wheel_protocol_queue_remote_tuning_response(WheelProtocol *protocol,
                                                 const RemoteTuningResponse *response);

/**
 * @brief Queues a system-control remote-tuning response.
 *
 * Retains a response for the system-control output queue when its encoding is supported.
 *
 * @param[in,out] protocol Protocol state owning the response queue.
 * @param[in] response Semantic response to queue.
 * @return True when the response was accepted; otherwise false.
 */
bool wheel_protocol_queue_system_control_response(WheelProtocol *protocol,
                                                  const RemoteTuningResponse *response);

/**
 * @brief Reports whether remote-tuning output is pending.
 *
 * Reads the active remote-tuning response queue without consuming it.
 *
 * @param[in] protocol Protocol state to inspect.
 * @return True while a remote-tuning response is pending; otherwise false.
 */
bool wheel_protocol_remote_tuning_response_pending(const WheelProtocol *protocol);

/**
 * @brief Queues a remote telemetry report.
 *
 * Uses the active mode's telemetry queue and rejects a new payload while one is already pending.
 *
 * @param[in,out] protocol Protocol state owning the telemetry queue.
 * @param[in] payload Complete thirty-byte telemetry payload.
 * @return True when the payload was retained; otherwise false.
 */
bool wheel_protocol_queue_remote_telemetry(
    WheelProtocol *protocol, const uint8_t payload[WHEEL_OUTPUT_REMOTE_TELEMETRY_SIZE]);

/**
 * @brief Reports whether remote telemetry is pending.
 *
 * Reads the active mode's telemetry queue without consuming it.
 *
 * @param[in] protocol Protocol state to inspect.
 * @return True while telemetry remains queued; otherwise false.
 */
bool wheel_protocol_remote_telemetry_pending(const WheelProtocol *protocol);

/**
 * @brief Configures attached-wheel axis processing.
 *
 * Retains the host interface mode, axis override mode, current time, and bite-point percentage
 * used while normalizing wheel input. The bite point is left unchanged during an active adjustment.
 *
 * @param[in,out] protocol Protocol state to update.
 * @param[in] interface_mode Active host interface mode.
 * @param[in] override_mode Configured axis override mode.
 * @param[in] bite_point_percent Active analog-paddle bite-point percentage.
 * @param[in] now_ms Current monotonic time in milliseconds.
 */
void wheel_protocol_set_axis_processing(WheelProtocol *protocol, uint8_t interface_mode,
                                        uint8_t override_mode, uint8_t bite_point_percent,
                                        uint32_t now_ms);

/**
 * @brief Takes a completed analog-paddle bite-point adjustment.
 *
 * Returns and clears the one-shot adjustment produced by the wheel protocol.
 *
 * @param[in,out] protocol Protocol state holding the adjustment.
 * @param[out] updated_percent Destination for the completed percentage.
 * @return True when a completed adjustment was available; otherwise false.
 */
bool wheel_protocol_take_bite_point(WheelProtocol *protocol, uint8_t *updated_percent);

/**
 * @brief Takes a pending analog-paddle bite-point report.
 *
 * Returns and clears the one-shot percentage update intended for the next primary input report.
 *
 * @param[in,out] protocol Protocol state holding the report update.
 * @param[out] updated_percent Destination for the updated percentage.
 * @return True when a percentage update was available; otherwise false.
 */
bool wheel_protocol_take_bite_point_report(WheelProtocol *protocol, uint8_t *updated_percent);

/**
 * @brief Configures standard-packet button latching.
 *
 * Retains the latch enable and profile-transition state used during input normalization.
 *
 * @param[in,out] protocol Protocol state to update.
 * @param[in] enabled True to enable button latching.
 * @param[in] profile_transition_pending True while latching is suppressed.
 */
void wheel_protocol_set_button_latch(WheelProtocol *protocol, bool enabled,
                                     bool profile_transition_pending);

/**
 * @brief Selects character output for mode-nine displays.
 *
 * Enables conversion of supported seven-segment glyphs to characters before response encoding.
 *
 * @param[in,out] protocol Protocol state to update.
 * @param[in] enabled True to use character output; false to retain raw glyphs.
 */
void wheel_protocol_set_display_character_mode(WheelProtocol *protocol, bool enabled);

/**
 * @brief Selects display rotation output.
 *
 * Retains whether the display angle is enabled and the angle value used in legacy status output.
 *
 * @param[in,out] protocol Protocol state to update.
 * @param[in] enabled True to include display rotation.
 * @param[in] angle Signed angle in hundredths of a degree.
 */
void wheel_protocol_set_display_rotation(WheelProtocol *protocol, bool enabled, int16_t angle);

/**
 * @brief Sets the two legacy pedal-status bytes.
 *
 * Replaces the pedal status returned in legacy wheel status responses.
 *
 * @param[in,out] protocol Protocol state to update.
 * @param[in] first First pedal status byte.
 * @param[in] second Second pedal status byte.
 */
void wheel_protocol_set_legacy_pedal_status(WheelProtocol *protocol, uint8_t first, uint8_t second);

/**
 * @brief Queues a system status code.
 *
 * Retains the code's low byte for publication in the next supported attached-wheel response.
 *
 * @param[in,out] protocol Protocol state to update.
 * @param[in] code System status code to publish.
 */
void wheel_protocol_queue_system_status(WheelProtocol *protocol, uint16_t code);

/**
 * @brief Accepts one attached-wheel protocol packet.
 *
 * Advances handshake or active-mode processing and prepares the corresponding response packet.
 *
 * @param[in,out] protocol Protocol state and response storage.
 * @param[in] request Complete fifty-seven-byte request packet.
 */
void wheel_protocol_accept(WheelProtocol *protocol,
                           const uint8_t request[WHEEL_PROTOCOL_PACKET_SIZE]);
/**
 * @brief Returns the current attached-wheel response.
 *
 * Exposes the complete response packet prepared by handshake or active-mode processing.
 *
 * @param[in] protocol Protocol state to inspect.
 * @return Pointer to the current fifty-seven-byte response packet.
 */
const uint8_t *wheel_protocol_response(const WheelProtocol *protocol);

/**
 * @brief Returns the normalized attached-wheel request snapshot.
 *
 * Exposes the thirty-byte snapshot after a supported active request has been captured.
 *
 * @param[in] protocol Protocol state to inspect.
 * @return Pointer to the current snapshot, or null before supported input is ready.
 */
const uint8_t *wheel_protocol_request(const WheelProtocol *protocol);

/**
 * @brief Returns the current mode-one input.
 *
 * Exposes decoded mode-one input only after a supported mode-one request has been captured.
 *
 * @param[in] protocol Protocol state to inspect.
 * @return Pointer to mode-one input, or null when unavailable.
 */
const WheelPacketModeOneInput *wheel_protocol_mode_one_input(const WheelProtocol *protocol);

/**
 * @brief Returns the current mode-four input.
 *
 * Exposes decoded mode-four input only after a mode-four request has been captured.
 *
 * @param[in] protocol Protocol state to inspect.
 * @return Pointer to mode-four input, or null when unavailable.
 */
const WheelPacketModeFourInput *wheel_protocol_mode_four_input(const WheelProtocol *protocol);

/**
 * @brief Returns the current standard display-packet input.
 *
 * Exposes decoded mode-0x10 input only after a supported request has been captured.
 *
 * @param[in] protocol Protocol state to inspect.
 * @return Pointer to display-packet input, or null when unavailable.
 */
const WheelPacketDisplayInput *wheel_protocol_display_input(const WheelProtocol *protocol);

/**
 * @brief Returns the current remapped-packet input.
 *
 * Exposes decoded mode-0x11 input only after a supported request has been captured.
 *
 * @param[in] protocol Protocol state to inspect.
 * @return Pointer to remapped input, or null when unavailable.
 */
const WheelPacketRemappedInput *wheel_protocol_remapped_input(const WheelProtocol *protocol);

/**
 * @brief Returns the current alternate-packet input.
 *
 * Exposes decoded mode-0x12 input only after a supported request has been captured.
 *
 * @param[in] protocol Protocol state to inspect.
 * @return Pointer to alternate input, or null when unavailable.
 */
const WheelPacketAlternateInput *wheel_protocol_alternate_input(const WheelProtocol *protocol);

/**
 * @brief Returns the current packed-packet input.
 *
 * Exposes decoded packed input only after a supported request has been captured.
 *
 * @param[in] protocol Protocol state to inspect.
 * @return Pointer to packed input, or null when unavailable.
 */
const WheelPacketPackedInput *wheel_protocol_packed_input(const WheelProtocol *protocol);

/**
 * @brief Returns the current axis-mode input.
 *
 * Exposes decoded and normalized axis-mode input only after a supported request has been captured.
 *
 * @param[in] protocol Protocol state to inspect.
 * @return Pointer to axis-mode input, or null when unavailable.
 */
const WheelPacketAxisModeInput *wheel_protocol_axis_mode_input(const WheelProtocol *protocol);

/**
 * @brief Returns the current extended-packet input.
 *
 * Exposes decoded and normalized extended input only after a supported request has been captured.
 *
 * @param[in] protocol Protocol state to inspect.
 * @return Pointer to extended input, or null when unavailable.
 */
const WheelPacketExtendedInput *wheel_protocol_extended_input(const WheelProtocol *protocol);

/**
 * @brief Returns the current adapter-oriented input.
 *
 * Exposes decoded, filtered, merged, and normalized input after a mode-0x0C request is captured.
 *
 * @param[in] protocol Protocol state to inspect.
 * @return Pointer to adapter-oriented input, or null when unavailable.
 */
const WheelPacketAdapterInput *wheel_protocol_adapter_input(const WheelProtocol *protocol);

/**
 * @brief Returns the current metadata-only input.
 *
 * Exposes axis values and report metadata after a mode-0x1E request is captured.
 *
 * @param[in] protocol Protocol state to inspect.
 * @return Pointer to metadata input, or null when unavailable.
 */
const WheelPacketMetadataInput *wheel_protocol_metadata_input(const WheelProtocol *protocol);

/**
 * @brief Returns the current CRC-family input.
 *
 * Exposes decoded CRC-family input only after a supported CRC request has been captured.
 *
 * @param[in] protocol Protocol state to inspect.
 * @return Pointer to CRC input, or null when unavailable.
 */
const WheelPacketCrcInput *wheel_protocol_crc_input(const WheelProtocol *protocol);

/**
 * @brief Returns separately retained mode-one report fields.
 *
 * Exposes axis values and report metadata retained before mode-one normalization.
 *
 * @param[in] protocol Protocol state to inspect.
 * @return Pointer to mode-one report state, or null when unavailable.
 */
const WheelPacketModeOneReportState *
wheel_protocol_mode_one_report_state(const WheelProtocol *protocol);

/**
 * @brief Returns the attached-wheel axis override processor.
 *
 * Exposes the axis override state maintained while normalizing wheel packets.
 *
 * @param[in] protocol Protocol state to inspect.
 * @return Pointer to the current axis override processor.
 */
const WheelAxisOverrideProcessor *wheel_protocol_axis_overrides(const WheelProtocol *protocol);

/**
 * @brief Returns the attached-wheel capability state.
 *
 * Exposes capabilities retained from supported active wheel reports.
 *
 * @param[in] protocol Protocol state to inspect.
 * @return Pointer to the current capability state.
 */
const WheelCapabilityState *wheel_protocol_capabilities(const WheelProtocol *protocol);

/**
 * @brief Returns the attached wheel's axis limit.
 *
 * Selects the active packet's axis-limit field and returns zero before supported input is ready.
 *
 * @param[in] protocol Protocol state to inspect.
 * @return Current axis-limit value, or zero when unavailable.
 */
uint8_t wheel_protocol_axis_limit(const WheelProtocol *protocol);

/**
 * @brief Returns the attached wheel's secondary button byte.
 *
 * Selects the mode-button field from the active packet input.
 *
 * @param[in] protocol Protocol state to inspect.
 * @return Current secondary button byte, or zero when unavailable.
 */
uint8_t wheel_protocol_mode_buttons(const WheelProtocol *protocol);

/**
 * @brief Returns the attached wheel's primary axis-output bytes.
 *
 * Selects the active packet input's two axis-output bytes.
 *
 * @param[in] protocol Protocol state to inspect.
 * @return Pointer to two axis-output bytes, or null when unavailable.
 */
const uint8_t *wheel_protocol_axis_outputs(const WheelProtocol *protocol);

/**
 * @brief Reports whether the attached wheel enabled its axis report.
 *
 * Selects the active packet input's axis-report flag and reports disabled for unsupported input.
 *
 * @param[in] protocol Protocol state to inspect.
 * @return True when the active input enables its axis report; otherwise false.
 */
bool wheel_protocol_axis_report_enabled(const WheelProtocol *protocol);

/**
 * @brief Copies the attached wheel's two axis values.
 *
 * Selects the active packet's axis values and clears the destination when no input is available.
 *
 * @param[in] protocol Protocol state to inspect.
 * @param[out] values Destination for two 16-bit axis values.
 * @return True when axis values were available; otherwise false.
 */
bool wheel_protocol_axis_values(const WheelProtocol *protocol, uint16_t values[2]);

/**
 * @brief Copies the attached wheel's eight control bytes.
 *
 * Selects normalized controls from the active packet and clears the destination when unavailable.
 *
 * @param[in] protocol Protocol state to inspect.
 * @param[out] controls Destination for eight control bytes.
 * @return True when controls were available; otherwise false.
 */
bool wheel_protocol_controls(const WheelProtocol *protocol, uint8_t controls[8]);
/**
 * @brief Reads the queued primary motion direction.
 *
 * Inspects the protocol's primary motion counter without consuming it.
 *
 * @param[in] protocol Protocol state to inspect.
 * @return Negative one, zero, or positive one.
 */
int8_t wheel_protocol_motion_direction(const WheelProtocol *protocol);

/**
 * @brief Consumes one queued primary motion step.
 *
 * Moves the protocol's primary motion counter one step toward zero.
 *
 * @param[in,out] protocol Protocol state to update.
 * @return Negative one, zero, or positive one.
 */
int8_t wheel_protocol_take_motion(WheelProtocol *protocol);

/**
 * @brief Reads one queued auxiliary-axis motion direction.
 *
 * Inspects the selected protocol motion counter without consuming it.
 *
 * @param[in] protocol Protocol state to inspect.
 * @param[in] axis Zero-based auxiliary motion axis index.
 * @return Negative one, zero, or positive one; zero for an unsupported axis.
 */
int8_t wheel_protocol_axis_motion_direction(const WheelProtocol *protocol, uint8_t axis);

/**
 * @brief Consumes one queued auxiliary-axis motion step.
 *
 * Moves the selected protocol motion counter one step toward zero.
 *
 * @param[in,out] protocol Protocol state to update.
 * @param[in] axis Zero-based auxiliary motion axis index.
 * @return Negative one, zero, or positive one; zero for an unsupported axis.
 */
int8_t wheel_protocol_take_axis_motion(WheelProtocol *protocol, uint8_t axis);

/**
 * @brief Takes the latest remote-tuning control payload.
 *
 * Copies the retained thirty-byte payload and clears its one-shot pending latch.
 *
 * @param[in,out] protocol Protocol state holding the payload.
 * @param[out] output Destination for thirty control bytes.
 * @return True when a pending payload was copied; otherwise false.
 */
bool wheel_protocol_take_remote_tuning_controls(WheelProtocol *protocol, uint8_t output[30]);

/**
 * @brief Takes the attached-wheel request-change latch.
 *
 * Returns whether the normalized request changed since the previous take and clears the latch.
 *
 * @param[in,out] protocol Protocol state holding the change latch.
 * @return True when a request change was pending; otherwise false.
 */
bool wheel_protocol_request_changed(WheelProtocol *protocol);

/**
 * @brief Reports display-acknowledgement input from the wheel.
 *
 * Reads the active packet's eligibility latch without changing it.
 *
 * @param[in] protocol Protocol state to inspect.
 * @return True while eligible input is active; otherwise false.
 */
bool wheel_protocol_acknowledgement_input_active(const WheelProtocol *protocol);

/**
 * @brief Calculates an attached-wheel packet checksum.
 *
 * Applies the wheel CRC-8 to the first thirty-two bytes of the complete packet.
 *
 * @param[in] packet Complete fifty-seven-byte protocol packet.
 * @return CRC-8 for the packet content region.
 */
uint8_t wheel_protocol_message_checksum(const uint8_t packet[WHEEL_PROTOCOL_PACKET_SIZE]);

/**
 * @brief Validates an attached-wheel packet checksum.
 *
 * Compares the packet checksum byte with the CRC-8 calculated over its content region.
 *
 * @param[in] packet Complete fifty-seven-byte protocol packet.
 * @return True when the checksum matches; otherwise false.
 */
bool wheel_protocol_message_valid(const uint8_t packet[WHEEL_PROTOCOL_PACKET_SIZE]);

#endif
