#include "wheel/accessory.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/**
 * @brief Bit fields and protocol values in an accessory probe status byte.
 *
 * The status sign selects legacy versus negative protocol encoding, while the low bits select the
 * protocol and the middle bits carry the accessory model.
 */
enum {
    ACCESSORY_PROTOCOL_MASK = 0x03,      /**< Mask for the two-bit protocol selector. */
    ACCESSORY_MODEL_MASK = 0x7c,         /**< Mask for the five-bit model selector. */
    ACCESSORY_MODEL_SHIFT = 2,           /**< Shift from the status byte to the model value. */
    ACCESSORY_STATUS_FLAG = 0x80,        /**< Status sign bit as represented in the raw byte. */
    ACCESSORY_TRANSFER_CODE_MASK = 0x3f, /**< Mask for the six-bit transfer version code. */
    ACCESSORY_PROTOCOL_STANDARD = 0,     /**< Protocol selector for the standard protocol. */
    ACCESSORY_PROTOCOL_POSITION_A = 1,   /**< Protocol selector for the first position protocol. */
    ACCESSORY_PROTOCOL_POSITION_B = 2,   /**< Protocol selector for the second position protocol. */
};

/**
 * @brief Initializes attached wheel accessory state.
 *
 * Starts disconnected with no retained identity, status, model, or version. A null destination is
 * ignored.
 *
 * @param[out] accessory Accessory state to initialize.
 */
void wheel_accessory_init(WheelAccessory *accessory) {
    if (accessory != NULL) {
        *accessory = (WheelAccessory){0};
    }
}

/**
 * @brief Applies a completed attached wheel accessory probe.
 *
 * Stores the status and version before classifying the status. Nonnegative status selects the
 * legacy protocol. Negative status selects standard for protocol zero or extended for protocols
 * one and two and extracts the five-bit model. Protocol three is unsupported, so it leaves the
 * previous kind and model in place.
 *
 * @param[in,out] accessory Accessory identity and protocol state.
 * @param[in] status Signed probe status byte.
 * @param[in] version Accessory version value.
 * @return true when the status identifies a supported protocol; false for a null state or an
 * unsupported protocol.
 */
bool wheel_accessory_apply_probe(WheelAccessory *accessory, int8_t status, uint32_t version) {
    if (accessory == NULL) {
        return false;
    }
    accessory->initial_status = status;
    accessory->version = version;
    if (status >= 0) {
        accessory->kind = WHEEL_ACCESSORY_LEGACY;
        return true;
    }

    uint8_t packet = (uint8_t)status;
    uint8_t protocol = packet & ACCESSORY_PROTOCOL_MASK;
    if (protocol == ACCESSORY_PROTOCOL_STANDARD) {
        accessory->kind = WHEEL_ACCESSORY_STANDARD;
    } else if (protocol == ACCESSORY_PROTOCOL_POSITION_A ||
               protocol == ACCESSORY_PROTOCOL_POSITION_B) {
        accessory->kind = WHEEL_ACCESSORY_EXTENDED;
    } else {
        return false;
    }
    accessory->model = (packet & ACCESSORY_MODEL_MASK) >> ACCESSORY_MODEL_SHIFT;
    return true;
}

/**
 * @brief Returns the accessory transfer version code.
 *
 * Selects the low six bits of the retained version value.
 *
 * @param[in] accessory Current accessory state.
 * @return Six-bit transfer code, or zero when accessory is null.
 */
uint8_t wheel_accessory_transfer_code(const WheelAccessory *accessory) {
    return accessory == NULL ? 0 : (uint8_t)accessory->version & ACCESSORY_TRANSFER_CODE_MASK;
}

/**
 * @brief Returns the accessory protocol mode code.
 *
 * Nonnegative legacy status maps to zero. Negative status maps protocol bits zero through three to
 * mode codes one through four.
 *
 * @param[in] accessory Current accessory state.
 * @return Accessory mode code, or zero when accessory is null or has nonnegative status.
 */
uint8_t wheel_accessory_mode_code(const WheelAccessory *accessory) {
    if (accessory == NULL || accessory->initial_status >= 0) {
        return 0;
    }
    return (uint8_t)(((uint8_t)accessory->initial_status & ACCESSORY_PROTOCOL_MASK) + 1);
}

/**
 * @brief Builds the compact accessory mode flags.
 *
 * Combines the status sign, a nonzero-protocol flag, and the retained five-bit model shifted into
 * its published position.
 *
 * @param[in] accessory Current accessory state.
 * @return Compact accessory mode flags, or zero when accessory is null.
 */
uint8_t wheel_accessory_mode_flags(const WheelAccessory *accessory) {
    if (accessory == NULL) {
        return 0;
    }
    uint8_t status = (uint8_t)accessory->initial_status;
    uint8_t flags = status & ACCESSORY_STATUS_FLAG;
    if ((status & ACCESSORY_PROTOCOL_MASK) != 0) {
        flags |= 1;
    }
    return (uint8_t)(flags | accessory->model << 1);
}
