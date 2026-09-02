#ifndef OPENTEC_BASE_WHEEL_ACCESSORY_H
#define OPENTEC_BASE_WHEEL_ACCESSORY_H

#include <stdbool.h>
#include <stdint.h>

/**
 * @brief Protocol kind detected from the attached wheel accessory processor.
 *
 * The kind is updated only when a probe identifies a supported protocol.
 */
typedef enum {
    WHEEL_ACCESSORY_DISCONNECTED, /**< No supported accessory has been identified. */
    WHEEL_ACCESSORY_LEGACY,       /**< The accessory uses the legacy protocol. */
    WHEEL_ACCESSORY_STANDARD,     /**< The accessory uses the standard protocol. */
    WHEEL_ACCESSORY_EXTENDED,     /**< The accessory uses a position-specific extended protocol. */
} WheelAccessoryKind;

/**
 * @brief Attached wheel accessory identity and protocol state.
 *
 * The state retains the latest probe values and the last supported protocol classification.
 */
typedef struct {
    uint32_t version;       /**< Latest accessory version value returned by a probe. */
    int8_t initial_status;  /**< Signed status byte returned by the latest probe. */
    uint8_t model;          /**< Five-bit model value extracted from a supported negative status. */
    uint8_t accessory_type; /**< Extended accessory type returned by the accessory service. */
    WheelAccessoryKind kind; /**< Protocol classification from the latest supported probe. */
} WheelAccessory;

/**
 * @brief Initializes attached wheel accessory state.
 *
 * Clears the destination state and leaves the accessory classified as disconnected. A null
 * destination is ignored.
 *
 * @param[out] accessory Accessory state to initialize.
 */
void wheel_accessory_init(WheelAccessory *accessory);

/**
 * @brief Applies a completed attached wheel accessory probe.
 *
 * Stores the signed status and version before classifying the status. Nonnegative status selects
 * the legacy protocol; negative status selects standard for protocol zero or extended for
 * protocols one and two and extracts the model. Protocol three is unsupported, so it leaves the
 * previous kind and model in place.
 *
 * @param[in,out] accessory Accessory identity and protocol state.
 * @param[in] status Signed probe status byte.
 * @param[in] version Accessory version value.
 * @return true when the status identifies a supported protocol; false for a null state or an
 * unsupported protocol.
 */
bool wheel_accessory_apply_probe(WheelAccessory *accessory, int8_t status, uint32_t version);

/**
 * @brief Returns the accessory transfer version code.
 *
 * Selects the low six bits of the retained version value for the accessory transfer protocol.
 *
 * @param[in] accessory Current accessory state.
 * @return The six-bit transfer code, or zero when accessory is null.
 */
uint8_t wheel_accessory_transfer_code(const WheelAccessory *accessory);

/**
 * @brief Returns the accessory protocol mode code.
 *
 * Legacy status maps to zero. Negative status maps protocol bits zero through three to mode codes
 * one through four, including the unsupported protocol code.
 *
 * @param[in] accessory Current accessory state.
 * @return The accessory mode code, or zero when accessory is null or has nonnegative status.
 */
uint8_t wheel_accessory_mode_code(const WheelAccessory *accessory);

/**
 * @brief Builds the compact accessory mode flags.
 *
 * Combines the status sign, a nonzero-protocol flag, and the retained five-bit model in the
 * published bit positions.
 *
 * @param[in] accessory Current accessory state.
 * @return Compact accessory mode flags, or zero when accessory is null.
 */
uint8_t wheel_accessory_mode_flags(const WheelAccessory *accessory);

/**
 * @brief Returns the position modulus selected by the reported accessory model.
 *
 * Position protocols use one of two controller-specific absolute-position moduli. The model bit
 * reported in the signed identity status selects the modulus; other protocols return zero.
 *
 * @param[in] accessory Current accessory identity.
 * @return Model-dependent position modulus, or zero when no position protocol is active.
 */
uint32_t wheel_accessory_position_modulus(const WheelAccessory *accessory);

#endif
