#ifndef OPENTEC_BASE_PEDAL_PROTOCOL_H
#define OPENTEC_BASE_PEDAL_PROTOCOL_H

#include <stdbool.h>
#include <stdint.h>

#include "pedal/input.h"

/**
 * @brief Identifies the detected pedal controller.
 *
 * Values are the device bytes returned by the pedal discovery exchange.
 */
typedef enum {
    PEDAL_DEVICE_NONE,           /**< No pedal device has been detected. */
    PEDAL_DEVICE_V3 = 0x1a,      /**< Fanatec V3 pedal controller. */
    PEDAL_DEVICE_V4 = 0x2a,      /**< Fanatec V4 pedal controller. */
    PEDAL_DEVICE_INVALID = 0xff, /**< Invalid or unavailable device response. */
} PedalDevice;

/**
 * @brief Identifies the pedal transport selected after discovery.
 */
typedef enum {
    PEDAL_PROTOCOL_REDISCOVER, /**< Restart device discovery. */
    PEDAL_PROTOCOL_LEGACY,     /**< Use the byte-oriented legacy transport. */
    PEDAL_PROTOCOL_V3,         /**< Use framed V3 transport. */
    PEDAL_PROTOCOL_V4,         /**< Use transfer-session V4 transport. */
} PedalProtocol;

/**
 * @brief Identifies a channel in the legacy pedal polling cycle.
 */
typedef enum {
    PEDAL_LEGACY_AXIS_1,        /**< First pedal axis channel. */
    PEDAL_LEGACY_AXIS_2,        /**< Second pedal axis channel. */
    PEDAL_LEGACY_AXIS_3,        /**< Third pedal axis channel. */
    PEDAL_LEGACY_AUXILIARY,     /**< Auxiliary channel. */
    PEDAL_LEGACY_CHANNEL_COUNT, /**< Number of legacy channels. */
} PedalLegacyChannel;

/**
 * @brief Stores the protocol selectors and scale published to a pedal controller.
 */
typedef struct {
    uint8_t value;  /**< Protocol value selector. */
    uint8_t first;  /**< First pedal channel selector. */
    uint8_t second; /**< Second pedal channel selector. */
    uint8_t scale;  /**< Legacy pedal scale setting. */
} PedalProtocolStatus;

/**
 * @brief Identifies pending V3 pedal calibration controls.
 *
 * Values are bit flags and may be combined when several controls are queued.
 */
typedef enum {
    PEDAL_V3_CONTROL_UP = 1u << 0,        /**< Move calibration upward. */
    PEDAL_V3_CONTROL_DOWN = 1u << 1,      /**< Move calibration downward. */
    PEDAL_V3_CONTROL_ENABLE = 1u << 2,    /**< Enable pedal calibration. */
    PEDAL_V3_CONTROL_DISABLE = 1u << 3,   /**< Disable pedal calibration. */
    PEDAL_V3_CONTROL_AUTOMATIC = 1u << 4, /**< Run automatic pedal calibration. */
} PedalV3Control;

/**
 * @brief Selects the pedal transport from discovery responses.
 *
 * Recognizes V3 and V4 response pairs, selects legacy transport for other valid pairs, and asks
 * the caller to rediscover invalid or empty responses.
 *
 * @param[in] device Device byte returned by discovery.
 * @param[in] response Protocol byte returned by the controller.
 * @return Selected pedal transport.
 */
PedalProtocol pedal_protocol_select(uint8_t device, uint8_t response);

/**
 * @brief Builds one legacy channel request byte.
 *
 * Combines the selected channel prefix with configured protocol selectors where required.
 *
 * @param[in] channel Legacy channel to request.
 * @param[in] protocol_first Selector bits for legacy axis two.
 * @param[in] protocol_second Selector bits for legacy axis three.
 * @return Request byte, or zero for auxiliary, count, and unsupported values.
 */
uint8_t pedal_legacy_request(PedalLegacyChannel channel, uint8_t protocol_first,
                             uint8_t protocol_second);

/**
 * @brief Applies one legacy response byte to the selected input channel.
 *
 * Expands legacy axis values to the internal sixteen-bit representation and preserves a locked
 * auxiliary channel.
 *
 * @param[in] channel Channel that produced response.
 * @param[in] response Received response byte.
 * @param[in] auxiliary_locked True when another source owns the auxiliary channel.
 * @param[in,out] input Pedal input state to update.
 */
void pedal_legacy_apply_response(PedalLegacyChannel channel, uint8_t response,
                                 bool auxiliary_locked, PedalInput *input);

/**
 * @brief Builds a V3 startup or recovery handshake frame.
 *
 * Selects the startup marker pair or its complementary recovery pair.
 *
 * @param[in] recovering True when building a recovery handshake.
 * @param[out] frame Frame destination.
 */
void pedal_v3_build_handshake(bool recovering, PedalFrame *frame);

/**
 * @brief Builds a V3 protocol-status frame.
 *
 * Copies the configured status fields into the frame payload.
 *
 * @param[in] status Protocol status to encode.
 * @param[out] frame Frame destination.
 */
void pedal_v3_build_status(const PedalProtocolStatus *status, PedalFrame *frame);

/**
 * @brief Builds the next V3 calibration-control frame.
 *
 * Encodes all pending direct controls together or encodes automatic control when no direct control
 * is pending.
 *
 * @param[in] pending Pending control bit mask.
 * @param[out] frame Frame destination.
 * @return Control bits not consumed by the encoded frame.
 */
uint8_t pedal_v3_build_control(uint8_t pending, PedalFrame *frame);

/**
 * @brief Builds a V3 three-axis input-command frame.
 *
 * Copies the supplied command values into the frame payload.
 *
 * @param[in] values Three command bytes to encode.
 * @param[out] frame Frame destination.
 */
void pedal_v3_build_input_command(const uint8_t values[PEDAL_INPUT_AXIS_COUNT], PedalFrame *frame);

/**
 * @brief Builds a V3 brake-force configuration frame.
 *
 * Encodes the force percentage using the selected calibration step and optional reset marker.
 *
 * @param[in] brake_force Brake-force percentage to encode.
 * @param[in] fine_scale True for five-point steps; false for ten-point steps.
 * @param[in] reset True to include the reset marker.
 * @param[out] frame Frame destination.
 */
void pedal_v3_build_configuration(uint8_t brake_force, bool fine_scale, bool reset,
                                  PedalFrame *frame);

/**
 * @brief Builds a V3 calibration keepalive frame.
 *
 * Selects the keepalive frame type and clears its payload.
 *
 * @param[out] frame Frame destination.
 */
void pedal_v3_build_keepalive(PedalFrame *frame);

#endif
