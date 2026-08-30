#include "wheel/accessory.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

enum {
    ACCESSORY_PROTOCOL_MASK = 0x03,
    ACCESSORY_MODEL_MASK = 0x7c,
    ACCESSORY_MODEL_SHIFT = 2,
    ACCESSORY_STATUS_FLAG = 0x80,
    ACCESSORY_TRANSFER_CODE_MASK = 0x3f,
    ACCESSORY_PROTOCOL_STANDARD = 0,
    ACCESSORY_PROTOCOL_POSITION_A = 1,
    ACCESSORY_PROTOCOL_POSITION_B = 2,
};

/**
 * @brief Initializes attached wheel accessory state.
 *
 * Starts disconnected with no retained identity, status, model, or version.
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
 * Nonnegative status selects the legacy protocol. Negative status selects Standard for protocol
 * zero or Extended for either position protocol and extracts the five-bit model. Protocol three
 * is rejected without replacing the last accepted kind or model.
 *
 * @param[in,out] accessory Accessory identity and protocol state.
 * @param[in] status Signed probe status byte.
 * @param[in] version Accessory version value.
 * @return True when the status identifies a supported protocol.
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
 * @return Six-bit transfer code, or zero when state is unavailable.
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
 * @return Accessory mode code, or zero when state is unavailable.
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
 * @return Compact accessory mode flags, or zero when state is unavailable.
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
